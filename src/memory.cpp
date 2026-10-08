#include "memory.h"
#include "state.h"
#include "io.h"

#include <fstream>
#include <iostream>
#include <vector>

namespace apple2e {

void SoftSwitches::resetMMU() {
    store80 = false;
    ramrd = false;
    ramwrt = false;
    intcxrom = false;
    altzp = false;
    slotc3rom = false;
    intc8rom = false;
    col80 = false;
    altcharset = false;

    lcReadRam = false;
    lcWriteRam = true;
    lcBank2 = true;
    lcPrewrite = false;
}

Memory::Memory(SoftSwitches& switches) : m_sw(switches) {}

bool Memory::loadRom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open ROM file: " << path << std::endl;
        return false;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.size() != 0x4000 && data.size() != 0x8000) {
        std::cerr << "Unsupported ROM size " << data.size()
                  << " bytes (expected 16384 or 32768)" << std::endl;
        return false;
    }

    // In 32 KB dumps the $C000-$FFFF image is the upper half
    std::copy(data.end() - m_rom.size(), data.end(), m_rom.begin());

    std::cout << "ROM loaded: " << data.size() << " bytes from " << path << std::endl;
    return true;
}

void Memory::clearRam() {
    m_main.fill(0);
    m_aux.fill(0);
}

Memory::Bank& Memory::ramBank(uint16_t addr, bool auxSelected) {
    if (m_sw.store80) {
        bool textPage = addr >= 0x0400 && addr < 0x0800;
        bool hiresPage = m_sw.hires && addr >= 0x2000 && addr < 0x4000;
        if (textPage || hiresPage) return m_sw.page2 ? m_aux : m_main;
    }
    return auxSelected ? m_aux : m_main;
}

uint16_t Memory::lcOffset(uint16_t addr) const {
    return (addr < 0xE000 && !m_sw.lcBank2) ? addr - 0x1000 : addr;
}

uint8_t Memory::readPeripheralRom(uint16_t addr) {
    uint8_t slot = (addr >> 8) & 0x0F;
    uint8_t value = 0;  // empty slots

    if (addr < 0xC800) {
        bool internalC3 = slot == 3 && !m_sw.slotc3rom;
        if (internalC3) m_sw.intc8rom = true;
        if (m_sw.intcxrom || internalC3) {
            value = m_rom[addr - 0xC000];
        } else if (m_cards[slot]) {
            value = m_cards[slot]->rom(addr & 0xFF);
        }
    } else {
        if (m_sw.intcxrom || m_sw.intc8rom) value = m_rom[addr - 0xC000];
        if (addr == 0xCFFF) m_sw.intc8rom = false;
    }
    return value;
}

uint8_t Memory::read(uint16_t addr) {
    if (addr < 0x0200) return (m_sw.altzp ? m_aux : m_main)[addr];
    if (addr < 0xC000) return ramBank(addr, m_sw.ramrd)[addr];
    if (addr < 0xC100) return m_io ? m_io->read(addr) : 0;
    if (addr < 0xD000) return readPeripheralRom(addr);

    if (m_sw.lcReadRam) return (m_sw.altzp ? m_aux : m_main)[lcOffset(addr)];
    return m_rom[addr - 0xC000];
}

void Memory::write(uint16_t addr, uint8_t val) {
    if (addr < 0x0200) {
        (m_sw.altzp ? m_aux : m_main)[addr] = val;
    } else if (addr < 0xC000) {
        ramBank(addr, m_sw.ramwrt)[addr] = val;
    } else if (addr < 0xC100) {
        if (m_io) m_io->write(addr, val);
    } else if (addr < 0xD000) {
        uint8_t slot = (addr >> 8) & 0x0F;
        bool internal = m_sw.intcxrom || (slot == 3 && !m_sw.slotc3rom);
        if (addr < 0xC800 && !internal && m_cards[slot]) m_cards[slot]->romWrite(addr & 0xFF, val);
        if (addr == 0xCFFF) m_sw.intc8rom = false;
    } else if (m_sw.lcWriteRam) {
        (m_sw.altzp ? m_aux : m_main)[lcOffset(addr)] = val;
    }
}

void Memory::saveState(StateWriter& w) const {
    w.putBytes(m_main.data(), m_main.size());
    w.putBytes(m_aux.data(), m_aux.size());
}

void Memory::loadState(StateReader& r) {
    r.getBytes(m_main.data(), m_main.size());
    r.getBytes(m_aux.data(), m_aux.size());
}

} // namespace apple2e
