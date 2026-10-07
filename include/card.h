#pragma once

#include <cstdint>

namespace apple2e {

// Peripheral card in one of slots 1-7
class Card {
public:
    virtual ~Card() = default;

    // Device select space $C080+slot*16: `reg` is the low nibble
    virtual uint8_t io(uint8_t reg, bool isWrite, uint8_t val) = 0;

    // Slot ROM $Cn00-$CnFF
    virtual uint8_t rom(uint8_t offset) const = 0;

    // Write to the slot ROM space (most cards ignore it)
    virtual void romWrite(uint8_t offset, uint8_t val) { (void)offset; (void)val; }
};

} // namespace apple2e
