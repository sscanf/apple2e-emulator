#include "io.h"

#include "audio.h"
#include "card.h"
#include "gameio.h"
#include "keyboard.h"
#include "memory.h"

namespace apple2e {

IOController::IOController(SoftSwitches& switches, KeyboardController& keyboard,
                           AudioController& audio, GameIO& gameIO, const uint64_t& cycles)
    : m_sw(switches), m_keyboard(keyboard), m_audio(audio), m_gameIO(gameIO), m_cycles(cycles) {}

bool IOController::inVerticalBlank() const {
    return (m_cycles % kCyclesPerFrame) >= kVisibleLines * kCyclesPerLine;
}

uint8_t IOController::read(uint16_t addr) {
    if (addr >= 0xC090) {
        Card* card = m_cards[(addr >> 4) & 0x07];
        return card ? card->io(addr & 0x0F, false, 0) : 0x00;
    }

    uint8_t keyLatch = m_keyboard.data() & 0x7F;
    auto status = [keyLatch](bool on) -> uint8_t { return (on ? 0x80 : 0x00) | keyLatch; };

    switch (addr & 0xFFF0) {
        case 0xC000:
            return m_keyboard.data();

        case 0xC010:
            switch (addr) {
                case 0xC010: return m_keyboard.clearStrobe();
                case 0xC011: return status(m_sw.lcBank2);
                case 0xC012: return status(m_sw.lcReadRam);
                case 0xC013: return status(m_sw.ramrd);
                case 0xC014: return status(m_sw.ramwrt);
                case 0xC015: return status(m_sw.intcxrom);
                case 0xC016: return status(m_sw.altzp);
                case 0xC017: return status(m_sw.slotc3rom);
                case 0xC018: return status(m_sw.store80);
                case 0xC019: return status(!inVerticalBlank());  // IIe: low during VBL
                case 0xC01A: return status(m_sw.text);
                case 0xC01B: return status(m_sw.mixed);
                case 0xC01C: return status(m_sw.page2);
                case 0xC01D: return status(m_sw.hires);
                case 0xC01E: return status(m_sw.altcharset);
                case 0xC01F: return status(m_sw.col80);
            }
            break;

        case 0xC060:
            switch (addr & 0x07) {
                case 0: return 0x00;  // cassette in
                case 1: return (m_keyboard.openApple() || m_gameIO.button(0)) ? 0x80 : 0x00;
                case 2: return (m_keyboard.solidApple() || m_gameIO.button(1)) ? 0x80 : 0x00;
                case 3: return m_gameIO.button(2) ? 0x80 : 0x00;
                default: return m_gameIO.paddleTimerRunning(addr & 0x03, m_cycles) ? 0x80 : 0x00;
            }

        case 0xC080:
            accessLanguageCard(addr, true);
            return 0x00;
    }

    accessCommon(addr, true);
    return 0x00;
}

void IOController::write(uint16_t addr, uint8_t val) {
    if (addr >= 0xC090) {
        if (Card* card = m_cards[(addr >> 4) & 0x07]) card->io(addr & 0x0F, true, val);
        return;
    }

    switch (addr & 0xFFF0) {
        case 0xC000:
            switch (addr) {
                case 0xC000: m_sw.store80 = false; break;
                case 0xC001: m_sw.store80 = true; break;
                case 0xC002: m_sw.ramrd = false; break;
                case 0xC003: m_sw.ramrd = true; break;
                case 0xC004: m_sw.ramwrt = false; break;
                case 0xC005: m_sw.ramwrt = true; break;
                case 0xC006: m_sw.intcxrom = false; break;
                case 0xC007: m_sw.intcxrom = true; break;
                case 0xC008: m_sw.altzp = false; break;
                case 0xC009: m_sw.altzp = true; break;
                case 0xC00A: m_sw.slotc3rom = false; break;
                case 0xC00B: m_sw.slotc3rom = true; break;
                case 0xC00C: m_sw.col80 = false; break;
                case 0xC00D: m_sw.col80 = true; break;
                case 0xC00E: m_sw.altcharset = false; break;
                case 0xC00F: m_sw.altcharset = true; break;
            }
            return;

        case 0xC010:
            m_keyboard.clearStrobe();
            return;

        case 0xC080:
            accessLanguageCard(addr, false);
            return;
    }

    accessCommon(addr, false);
}

void IOController::accessCommon(uint16_t addr, bool isRead) {
    (void)isRead;

    switch (addr & 0xFFF0) {
        case 0xC030:
            m_audio.toggleSpeaker(m_cycles);
            break;

        case 0xC050:
            switch (addr & 0x0F) {
                case 0x0: m_sw.text = false; break;
                case 0x1: m_sw.text = true; break;
                case 0x2: m_sw.mixed = false; break;
                case 0x3: m_sw.mixed = true; break;
                case 0x4: m_sw.page2 = false; break;
                case 0x5: m_sw.page2 = true; break;
                case 0x6: m_sw.hires = false; break;
                case 0x7: m_sw.hires = true; break;
                case 0xE: m_sw.dhires = true; break;
                case 0xF: m_sw.dhires = false; break;
                default: break;  // annunciators 0-2
            }
            break;

        case 0xC070:
            m_gameIO.trigger(m_cycles);
            break;

        default:
            break;  // cassette out, game I/O strobe
    }
}

// $C080-$C08F. Bit 3 selects bank 1/2 for $D000; bits 0-1 select:
//   0: read RAM, no write     1: read ROM, write RAM
//   2: read ROM, no write     3: read RAM, write RAM
// Write enable needs two consecutive reads of an odd address.
void IOController::accessLanguageCard(uint16_t addr, bool isRead) {
    m_sw.lcBank2 = !(addr & 0x08);
    uint8_t mode = addr & 0x03;
    m_sw.lcReadRam = mode == 0 || mode == 3;

    if (addr & 0x01) {
        if (isRead && m_sw.lcPrewrite) m_sw.lcWriteRam = true;
        m_sw.lcPrewrite = isRead;
    } else {
        m_sw.lcWriteRam = false;
        m_sw.lcPrewrite = false;
    }
}

} // namespace apple2e
