#pragma once

#include "card.h"
#include "cpu.h"

#include <array>
#include <cstdint>
#include <string>

namespace apple2e {

class StateReader;
class StateWriter;

class IOController;

// Soft-switch state of the IIe MMU and IOU. Shared by memory (bank
// selection), I/O (which flips the switches) and video (display mode).
struct SoftSwitches {
    // MMU
    bool store80 = false;    // PAGE2 selects aux text/hires instead of page 2
    bool ramrd = false;      // $0200-$BFFF reads from aux RAM
    bool ramwrt = false;     // $0200-$BFFF writes to aux RAM
    bool intcxrom = false;   // $C100-$CFFF from internal ROM
    bool altzp = false;      // zero page, stack and language card in aux RAM
    bool slotc3rom = false;  // $C300 from slot 3 instead of internal 80-col firmware
    bool intc8rom = false;   // $C800-$CFFF claimed by internal 80-col firmware

    // Language card ($D000-$FFFF RAM)
    bool lcReadRam = false;
    bool lcWriteRam = true;
    bool lcBank2 = true;
    bool lcPrewrite = false;

    // IOU / video
    bool text = true;
    bool mixed = false;
    bool page2 = false;
    bool hires = false;
    bool col80 = false;
    bool altcharset = false;
    bool dhires = false;

    // State after the RESET line is pulled (video switches are left alone)
    void resetMMU();
};

// Apple IIe memory map:
//   $0000-$01FF  Zero page and stack (main or aux, ALTZP)
//   $0200-$BFFF  RAM (main or aux, RAMRD/RAMWRT/80STORE)
//     $0400-$07FF  Text / lo-res page 1   ($0800-$0BFF page 2)
//     $2000-$3FFF  Hi-res page 1          ($4000-$5FFF page 2)
//   $C000-$C0FF  I/O and soft switches
//   $C100-$C7FF  Peripheral slot ROMs (or internal ROM)
//   $C800-$CFFF  Expansion ROM (or internal ROM)
//   $D000-$FFFF  Monitor/Applesoft ROM, or language card RAM
class Memory : public Bus {
public:
    explicit Memory(SoftSwitches& switches);

    // Accepts a 16 KB $C000-$FFFF image, or a 32 KB image whose upper half is
    bool loadRom(const std::string& path);
    void setIO(IOController* io) { m_io = io; }
    void setCard(int slot, Card* card) { m_cards[slot] = card; }
    void clearRam();

    uint8_t read(uint16_t addr) override;
    void write(uint16_t addr, uint8_t val) override;

    // Raw RAM banks (used by the video generator)
    const uint8_t* mainRam() const { return m_main.data(); }
    const uint8_t* auxRam() const { return m_aux.data(); }

    // Save states (see state.h)
    void saveState(StateWriter& w) const;
    void loadState(StateReader& r);

private:
    using Bank = std::array<uint8_t, 0x10000>;

    // Bank holding $0200-$BFFF for the given access (honours 80STORE)
    Bank& ramBank(uint16_t addr, bool auxSelected);
    // Language card: bank 1 of $D000-$DFFF is stored at $C000-$CFFF
    uint16_t lcOffset(uint16_t addr) const;
    uint8_t readPeripheralRom(uint16_t addr);

    SoftSwitches& m_sw;
    IOController* m_io = nullptr;
    std::array<Card*, 8> m_cards{};

    Bank m_main{};
    Bank m_aux{};
    std::array<uint8_t, 0x4000> m_rom{};  // $C000-$FFFF
};

} // namespace apple2e
