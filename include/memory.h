#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <functional>

namespace apple2e {

// Apple IIe memory map:
// $0000-$00FF  Zero page + stack
// $0100-$01FF  Stack area
// $0200-$03FF  Free RAM (or display in some modes)
// $0400-$3FFF  Display memory / free RAM
// $4000-$7FFF  Expansion ROM / RAM (bankable)
// $8000-$BFFF  BASIC ROM / RAM (bankable)
// $C000-$C0FF  I/O region
// $C100-$DFFF  Free RAM
// $E000-$EFFF  Monitor ROM
// $F000-$FFFF  Integer BASIC ROM (or Applesoft)

class Memory {
public:
    Memory();

    // Load ROM file
    bool loadRom(const std::string& path);

    // Read/write
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t val);

    // Memory access callbacks (for I/O and ROM mapping)
    using ReadCallback = std::function<uint8_t(uint16_t addr)>;
    using WriteCallback = std::function<void(uint16_t addr, uint8_t val)>;

    void setIoReadCallback(ReadCallback cb);
    void setIoWriteCallback(WriteCallback cb);
    void setRomReadCallback(ReadCallback cb);
    void setRomWriteCallback(WriteCallback cb);

    // Bank switching
    void setBank(uint16_t bankAddr, const uint8_t* data, size_t size);
    void clearBank(uint16_t bankAddr);

    // Get raw memory pointer for a range
    uint8_t* getRawPtr(uint16_t addr);
    const uint8_t* getRawPtr(uint16_t addr) const;

    // Get full 64KB memory view
    const uint8_t* getMemory() const { return m_memory.data(); }
    uint8_t* getMemory() { return m_memory.data(); }

    // Get loaded ROM data
    const std::vector<uint8_t>& getRomData() const { return m_romData; }

    // Dump memory for debugging
    std::string dumpHex(uint16_t start, size_t count) const;

private:
    // Main 64KB memory
    std::array<uint8_t, 0x10000> m_memory;

    // ROM data (loaded from file)
    std::vector<uint8_t> m_romData;

    // Banked memory regions
    struct Bank {
        const uint8_t* data = nullptr;
        size_t size = 0;
        bool loaded = false;
    };
    std::array<Bank, 0x4000 / 0x1000> m_banks; // 8 banks of 4KB

    // Callbacks
    ReadCallback m_ioReadCallback = nullptr;
    WriteCallback m_ioWriteCallback = nullptr;
    ReadCallback m_romReadCallback = nullptr;
    WriteCallback m_romWriteCallback = nullptr;

    // Check if address is in I/O range
    bool isIoAddress(uint16_t addr) const;
    // Check if address is in ROM range
    bool isRomAddress(uint16_t addr) const;
    // Check if address is in a banked region
    Bank* getBankForAddress(uint16_t addr);
};

} // namespace apple2e
