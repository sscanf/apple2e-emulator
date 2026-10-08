#include "softcard.h"
#include "state.h"

namespace apple2e {

uint16_t SoftCard::Z80Bus::map(uint16_t addr) {
    if (addr < 0xB000) return addr + 0x1000;
    if (addr < 0xE000) return addr + 0x2000;
    if (addr < 0xF000) return addr - 0x2000;
    return addr - 0xF000;
}

SoftCard::SoftCard(Bus& appleBus) : m_bus(appleBus), m_z80(m_bus) {}

void SoftCard::romWrite(uint8_t offset, uint8_t) {
    if (offset == 0x00) m_z80Active = !m_z80Active;
}

uint32_t SoftCard::step() {
    // The Z80 runs at ~2.04 MHz, two T-states per 6502 cycle
    uint32_t tstates = m_z80.step() + m_halfCycle;
    m_halfCycle = tstates & 1;
    return tstates / 2;
}

void SoftCard::reset() {
    m_z80Active = false;
    m_halfCycle = 0;
    m_z80.reset();
}

void SoftCard::saveState(StateWriter& w) const {
    w.put(m_z80Active);
    w.put(m_halfCycle);
    m_z80.saveState(w);
}

void SoftCard::loadState(StateReader& r) {
    r.get(m_z80Active);
    r.get(m_halfCycle);
    m_z80.loadState(r);
}

} // namespace apple2e
