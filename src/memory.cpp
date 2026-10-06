#include "memory.h"

#include <cstring>
#include <fstream>
#include <iostream>

namespace apple2e {

Memory::Memory() {
    // Clear all memory
    m_memory.fill(0);
}

bool Memory::loadRom(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << path << std::endl;
        return false;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    m_romData.resize(fileSize);
    if (!file.read(reinterpret_cast<char*>(m_romData.data()), fileSize)) {
        std::cerr << "Failed to read ROM file: " << path << std::endl;
        return false;
    }

    std::cout << "ROM loaded: " << fileSize << " bytes from " << path << std::endl;
    return true;
}

uint8_t Memory::read(uint16_t addr) {
    // Check I/O callback first (I/O space: $C000-$C0FF)
    if (m_ioReadCallback && isIoAddress(addr)) {
        return m_ioReadCallback(addr);
    }

    // Check ROM callback (ROM space: $E000-$FFFF)
    if (m_romReadCallback && isRomAddress(addr)) {
        return m_romReadCallback(addr);
    }

    // Check banked memory
    Bank* bank = getBankForAddress(addr);
    if (bank && bank->loaded && bank->data) {
        uint16_t bankOffset = addr & 0x0FFF;
        if (bankOffset < bank->size) {
            return bank->data[bankOffset];
        }
    }

    // Return from main memory
    return m_memory[addr];
}

void Memory::write(uint16_t addr, uint8_t val) {
    // Check I/O callback (I/O space: $C000-$C0FF)
    if (m_ioWriteCallback && isIoAddress(addr)) {
        m_ioWriteCallback(addr, val);
        return;
    }

    // ROM regions are read-only (but we allow writes for debugging)
    if (isRomAddress(addr)) {
        if (m_romWriteCallback) {
            m_romWriteCallback(addr, val);
        }
        return;
    }

    // Check banked memory
    Bank* bank = getBankForAddress(addr);
    if (bank && bank->loaded) {
        uint16_t bankOffset = addr & 0x0FFF;
        if (bankOffset < bank->size) {
            // Bank data is read-only ROM; ignore writes
            return;
        }
    }

    // Write to main memory
    m_memory[addr] = val;
}

void Memory::setIoReadCallback(ReadCallback cb) {
    m_ioReadCallback = cb;
}

void Memory::setIoWriteCallback(WriteCallback cb) {
    m_ioWriteCallback = cb;
}

void Memory::setRomReadCallback(ReadCallback cb) {
    m_romReadCallback = cb;
}

void Memory::setRomWriteCallback(WriteCallback cb) {
    m_romWriteCallback = cb;
}

void Memory::setBank(uint16_t bankAddr, const uint8_t* data, size_t size) {
    if (bankAddr < 0x4000 || bankAddr > 0xBFFF) return;
    if ((bankAddr & 0x0FFF) != 0) return;  // Must be 4KB aligned

    uint16_t bankIndex = (bankAddr - 0x4000) / 0x1000;
    if (bankIndex >= m_banks.size()) return;

    m_banks[bankIndex].data = data;
    m_banks[bankIndex].size = (size < 4096) ? size : 4096;
    m_banks[bankIndex].loaded = true;
}

void Memory::clearBank(uint16_t bankAddr) {
    if (bankAddr < 0x4000 || bankAddr > 0xBFFF) return;
    if ((bankAddr & 0x0FFF) != 0) return;

    uint16_t bankIndex = (bankAddr - 0x4000) / 0x1000;
    if (bankIndex >= m_banks.size()) return;

    m_banks[bankIndex].loaded = false;
    m_banks[bankIndex].data = nullptr;
    m_banks[bankIndex].size = 0;
}

uint8_t* Memory::getRawPtr(uint16_t addr) {
    return &m_memory[addr];
}

const uint8_t* Memory::getRawPtr(uint16_t addr) const {
    return &m_memory[addr];
}

std::string Memory::dumpHex(uint16_t start, size_t count) const {
    std::string result;
    char buf[16];
    for (size_t i = 0; i < count; i++) {
        uint16_t addr = start + i;
        uint8_t val = m_memory[addr];
        snprintf(buf, sizeof(buf), "%02X ", val);
        result += buf;
    }
    return result;
}

bool Memory::isIoAddress(uint16_t addr) const {
    return (addr >= 0xC000 && addr <= 0xC0FF);
}

bool Memory::isRomAddress(uint16_t addr) const {
    return (addr >= 0xE000 && addr <= 0xFFFF);
}

Memory::Bank* Memory::getBankForAddress(uint16_t addr) {
    if (addr < 0x4000 || addr > 0xBFFF) return nullptr;
    uint16_t bankIndex = (addr & 0x3000) / 0x1000;
    if (bankIndex >= m_banks.size()) return nullptr;
    return &m_banks[bankIndex];
}

} // namespace apple2e
