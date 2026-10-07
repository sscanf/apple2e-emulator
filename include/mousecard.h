#pragma once

#include "card.h"
#include "cpu.h"

#include <SDL.h>

#include <array>
#include <cstdint>
#include <functional>

namespace apple2e {

// AppleMouse II card. Apple's firmware (and the card's 6805) is replaced by a
// small 6502 stub with the documented entry points (SETMOUSE, SERVEMOUSE,
// READMOUSE, CLEARMOUSE, POSMOUSE, CLAMPMOUSE, HOMEMOUSE, INITMOUSE); each one
// stores A into a card register, the command runs here, and the stub returns
// with carry set on error. Results go to the usual screen holes.
//
// The host pointer over the Apple screen sets an absolute position within the
// clamp window, so the Apple cursor follows the host one.
class MouseCard : public Card {
public:
    MouseCard(Bus& bus, int slot);

    uint8_t io(uint8_t reg, bool isWrite, uint8_t val) override;
    uint8_t rom(uint8_t offset) const override { return m_rom[offset]; }

    // Host mouse input; `screen` is the Apple display in renderer coordinates.
    // Returns true when the event was used (the mouse is enabled by software).
    bool handleEvent(const SDL_Event& event, const SDL_Rect& screen);

    // Software has switched the mouse on (SETMOUSE with mode bit 0)
    bool enabled() const { return m_mode & kModeOn; }

    // Called once per frame at vertical blank (drives the VBL interrupt)
    void vblank();

    // IRQ line to the CPU
    void setIrqCallback(std::function<void(bool)> cb) { m_setIrq = std::move(cb); }

    void reset();

private:
    enum Command : uint8_t { SET, SERVE, READ, CLEAR, POS, CLAMP, HOME, INIT, kCommands };
    enum : uint8_t {
        kModeOn = 0x01, kModeIrqMove = 0x02, kModeIrqButton = 0x04, kModeIrqVbl = 0x08,
    };
    enum : uint8_t {  // status byte
        kStatusIrqMove = 0x02, kStatusIrqButton = 0x04, kStatusIrqVbl = 0x08,
        kStatusMoved = 0x20, kStatusWasDown = 0x40, kStatusDown = 0x80,
    };
    static constexpr uint8_t kResultReg = 0x0F;

    bool execute(Command cmd, uint8_t a);
    void writeHoles();
    void clampPosition();
    void raise(uint8_t statusFlag);
    uint16_t hole16(uint16_t loAddr, uint16_t hiAddr);

    Bus& m_bus;
    int m_slot;
    std::array<uint8_t, 256> m_rom{};

    uint8_t m_mode = 0;
    int m_x = 0, m_y = 0;
    int m_minX = 0, m_maxX = 1023, m_minY = 0, m_maxY = 1023;
    bool m_down = false;          // button state now
    bool m_downAtLastRead = false;
    bool m_moved = false;
    uint8_t m_irqFlags = 0;       // pending interrupt causes (status bits 1-3)
    uint8_t m_result = 0;         // 0 = OK, 1 = error / not ours
    std::function<void(bool)> m_setIrq;
};

} // namespace apple2e
