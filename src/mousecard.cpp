#include "mousecard.h"
#include "state.h"

#include <algorithm>

namespace apple2e {

namespace {

// Screen holes used by the mouse firmware; +slot for the per-slot ones
constexpr uint16_t kXLo = 0x0478, kYLo = 0x04F8, kXHi = 0x0578, kYHi = 0x05F8;
constexpr uint16_t kStatus = 0x0778, kModeHole = 0x07F8;

constexpr uint8_t kStubBase = 0x20;  // command stubs, 8 bytes each

} // namespace

MouseCard::MouseCard(Bus& bus, int slot) : m_bus(bus), m_slot(slot) {
    // Identification bytes checked by mouse-aware software
    m_rom[0x00] = 0x60;  // RTS: BASIC (PR#n/IN#n) entry not supported
    m_rom[0x05] = 0x38;
    m_rom[0x07] = 0x18;
    m_rom[0x0B] = 0x01;
    m_rom[0x0C] = 0x20;  // AppleMouse
    m_rom[0xFB] = 0xD6;

    // Pascal entry points (offsets 0D-10) and the mouse table (12-19)
    const uint8_t pascalError = 0x60;  // points at a SEC/RTS below
    m_rom[0x60] = 0x38;
    m_rom[0x61] = 0x60;
    for (int i = 0x0D; i <= 0x10; i++) m_rom[i] = pascalError;

    // Each stub: STA $C0n0+cmd / LDA $C0nF / LSR A (carry = error) / RTS
    uint8_t io = 0x80 + slot * 16;
    for (int cmd = 0; cmd < kCommands; cmd++) {
        uint8_t at = kStubBase + cmd * 8;
        m_rom[0x12 + cmd] = at;
        const uint8_t stub[8] = {0x8D, static_cast<uint8_t>(io + cmd), 0xC0,
                                 0xAD, static_cast<uint8_t>(io + kResultReg), 0xC0,
                                 0x4A, 0x60};
        std::copy(stub, stub + 8, m_rom.begin() + at);
    }
}

void MouseCard::reset() {
    m_mode = 0;
    m_irqFlags = 0;
    if (m_setIrq) m_setIrq(false);
}

uint8_t MouseCard::io(uint8_t reg, bool isWrite, uint8_t val) {
    if (reg == kResultReg) return m_result;
    if (isWrite && reg < kCommands) m_result = execute(static_cast<Command>(reg), val) ? 0 : 1;
    return 0;
}

uint16_t MouseCard::hole16(uint16_t loAddr, uint16_t hiAddr) {
    return m_bus.read(loAddr) | (m_bus.read(hiAddr) << 8);
}

void MouseCard::writeHoles() {
    m_bus.write(kXLo + m_slot, m_x & 0xFF);
    m_bus.write(kXHi + m_slot, (m_x >> 8) & 0xFF);
    m_bus.write(kYLo + m_slot, m_y & 0xFF);
    m_bus.write(kYHi + m_slot, (m_y >> 8) & 0xFF);
}

void MouseCard::clampPosition() {
    m_x = std::clamp(m_x, m_minX, m_maxX);
    m_y = std::clamp(m_y, m_minY, m_maxY);
}

void MouseCard::raise(uint8_t statusFlag) {
    m_irqFlags |= statusFlag;
    if (m_setIrq) m_setIrq(true);
}

bool MouseCard::execute(Command cmd, uint8_t a) {
    switch (cmd) {
        case SET:
            if (a > 0x0F) return false;
            m_mode = a;
            m_bus.write(kModeHole + m_slot, m_mode);
            return true;

        case SERVE: {
            // Report which interrupt sources fired, then release the IRQ line
            if (!m_irqFlags) return false;
            uint8_t status = m_bus.read(kStatus + m_slot) & ~(kStatusIrqMove | kStatusIrqButton | kStatusIrqVbl);
            m_bus.write(kStatus + m_slot, status | m_irqFlags);
            m_irqFlags = 0;
            if (m_setIrq) m_setIrq(false);
            return true;
        }

        case READ: {
            writeHoles();
            uint8_t status = (m_down ? kStatusDown : 0) | (m_downAtLastRead ? kStatusWasDown : 0) |
                             (m_moved ? kStatusMoved : 0);
            m_bus.write(kStatus + m_slot, status);
            m_downAtLastRead = m_down;
            m_moved = false;
            return true;
        }

        case CLEAR:
            m_x = m_y = 0;
            clampPosition();
            writeHoles();
            return true;

        case POS:
            m_x = static_cast<int16_t>(hole16(kXLo + m_slot, kXHi + m_slot));
            m_y = static_cast<int16_t>(hole16(kYLo + m_slot, kYHi + m_slot));
            clampPosition();
            return true;

        case CLAMP: {
            // Bounds come from the slot-0 screen holes
            int lo = static_cast<int16_t>(hole16(kXLo, kXHi));
            int hi = static_cast<int16_t>(hole16(kYLo, kYHi));
            if (a == 0) {
                m_minX = lo;
                m_maxX = hi;
            } else {
                m_minY = lo;
                m_maxY = hi;
            }
            clampPosition();
            return true;
        }

        case HOME:
            m_x = m_minX;
            m_y = m_minY;
            writeHoles();
            return true;

        case INIT:
            m_minX = m_minY = 0;
            m_maxX = m_maxY = 1023;
            m_x = m_y = 0;
            m_mode = 0;
            m_down = m_downAtLastRead = m_moved = false;
            m_irqFlags = 0;
            if (m_setIrq) m_setIrq(false);
            writeHoles();
            m_bus.write(kStatus + m_slot, 0);
            m_bus.write(kModeHole + m_slot, 0);
            return true;

        default:
            return false;
    }
}

void MouseCard::vblank() {
    if ((m_mode & (kModeOn | kModeIrqVbl)) == (kModeOn | kModeIrqVbl)) raise(kStatusIrqVbl);
}

bool MouseCard::handleEvent(const SDL_Event& event, const SDL_Rect& screen) {
    if (!enabled()) return false;

    switch (event.type) {
        case SDL_MOUSEMOTION: {
            SDL_Point p = {event.motion.x, event.motion.y};
            if (!SDL_PointInRect(&p, &screen)) return false;
            int x = m_minX + (p.x - screen.x) * (m_maxX - m_minX) / std::max(1, screen.w - 1);
            int y = m_minY + (p.y - screen.y) * (m_maxY - m_minY) / std::max(1, screen.h - 1);
            if (x != m_x || y != m_y) {
                m_x = x;
                m_y = y;
                m_moved = true;
                if (m_mode & kModeIrqMove) raise(kStatusIrqMove);
            }
            return true;
        }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            if (event.button.button != SDL_BUTTON_LEFT) return false;
            SDL_Point p = {event.button.x, event.button.y};
            bool down = event.type == SDL_MOUSEBUTTONDOWN;
            if (down && !SDL_PointInRect(&p, &screen)) return false;
            if (down != m_down) {
                m_down = down;
                if (m_mode & kModeIrqButton) raise(kStatusIrqButton);
            }
            return true;
        }

        default:
            return false;
    }
}

void MouseCard::saveState(StateWriter& w) const {
    w.put(m_mode); w.put(m_x); w.put(m_y);
    w.put(m_minX); w.put(m_maxX); w.put(m_minY); w.put(m_maxY);
    w.put(m_down); w.put(m_downAtLastRead); w.put(m_moved); w.put(m_irqFlags); w.put(m_result);
}

void MouseCard::loadState(StateReader& r) {
    r.get(m_mode); r.get(m_x); r.get(m_y);
    r.get(m_minX); r.get(m_maxX); r.get(m_minY); r.get(m_maxY);
    r.get(m_down); r.get(m_downAtLastRead); r.get(m_moved); r.get(m_irqFlags); r.get(m_result);
    if (m_setIrq) m_setIrq(m_irqFlags != 0);
}

} // namespace apple2e
