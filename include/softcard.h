#pragma once

#include "card.h"
#include "cpu.h"
#include "z80.h"

#include <cstdint>

namespace apple2e {

// Microsoft SoftCard: a Z80 on a card, used to run CP/M. Every write to
// $Cn00 flips control between the 6502 and the Z80 (the Z80 reaches that
// address as $En00). While the Z80 runs the 6502 is halted. The Z80 sees
// Apple memory remapped:
//   Z80 $0000-$AFFF -> $1000-$BFFF     Z80 $E000-$EFFF -> $C000-$CFFF
//   Z80 $B000-$DFFF -> $D000-$FFFF     Z80 $F000-$FFFF -> $0000-$0FFF
class SoftCard : public Card {
public:
    explicit SoftCard(Bus& appleBus);

    uint8_t io(uint8_t, bool, uint8_t) override { return 0; }  // no device registers
    uint8_t rom(uint8_t) const override { return 0; }          // no ROM
    void romWrite(uint8_t offset, uint8_t val) override;

    // True while the Z80 owns the bus
    bool z80Active() const { return m_z80Active; }

    // Run one Z80 instruction; returns the time taken in 6502 cycles
    uint32_t step();

    // RESET line: back to the 6502, Z80 restarts at $0000
    void reset();

private:
    // The Z80's view of the Apple bus
    class Z80Bus : public Bus {
    public:
        explicit Z80Bus(Bus& apple) : m_apple(apple) {}
        uint8_t read(uint16_t addr) override { return m_apple.read(map(addr)); }
        void write(uint16_t addr, uint8_t val) override { m_apple.write(map(addr), val); }

    private:
        static uint16_t map(uint16_t addr);
        Bus& m_apple;
    };

    Z80Bus m_bus;
    Z80 m_z80;
    bool m_z80Active = false;
    uint32_t m_halfCycle = 0;  // leftover Z80 T-state (two per 6502 cycle)
};

} // namespace apple2e
