#include "io.h"

#include <iostream>

namespace apple2e {

// ============================================================
// VIA (6522)
// ============================================================

VIA::VIA() : m_t2Counter(0x100) {
    // Initialize T2 counter for 5551 clock emulation
}

void VIA::setPortAReadCallback(PortAReadCallback cb) {
    m_portAReadCallback = cb;
}

void VIA::setPortAWriteCallback(PortAWriteCallback cb) {
    m_portAWriteCallback = cb;
}

void VIA::setPortBReadCallback(PortBReadCallback cb) {
    m_portBReadCallback = cb;
}

void VIA::setPortBWriteCallback(PortBWriteCallback cb) {
    m_portBWriteCallback = cb;
}

uint8_t VIA::read(uint8_t reg) {
    switch (reg) {
        case PORTA:    return readPortA();
        case DDR_A:    return m_ddrA;
        case PORTB:    return readPortB();
        case DDR_B:    return m_ddrB;
        case T1_LO:    return m_t1Counter & 0xFF;
        case T1_HI:    return (m_t1Counter >> 8) & 0xFF;
        case T1_LATCH_LO: return m_t1Latch & 0xFF;
        case T1_LATCH_HI: return (m_t1Latch >> 8) & 0xFF;
        case SR:       return m_srData;
        case ACR:      return m_acr;
        case PCR:      return m_pcr;
        case IFR: {
            uint8_t ifr = m_ifr & 0x7F;  // Clear pending bit when read
            if (m_ier & 0x80) {
                m_ifr &= ~0x80;  // Clear interrupt enable read flag
            }
            return ifr;
        }
        case IER: {
            m_ifr &= ~0x80;  // Clear pending bit
            return 0x80 | m_ier;  // Bit 7 is interrupt enable
        }
        case ORB:      return m_orB;
        default:       return 0;
    }
}

void VIA::write(uint8_t reg, uint8_t val) {
    switch (reg) {
        case PORTA:    writePortA(val); break;
        case DDR_A:    m_ddrA = val; break;
        case PORTB:    writePortB(val); break;
        case DDR_B:    m_ddrB = val; break;
        case T1_LO: {
            m_t1Counter = (m_t1Counter & 0xFF00) | val;
            if (m_t1Counter == 0) m_t1Counter = 0x10000;
            break;
        }
        case T1_HI: {
            m_t1Counter = (m_t1Counter & 0x00FF) | (val << 8);
            if (m_t1Counter == 0) m_t1Counter = 0x10000;
            m_t1FirstMatch = false;
            break;
        }
        case T1_LATCH_LO: m_t1Latch = (m_t1Latch & 0xFF00) | val; break;
        case T1_LATCH_HI: m_t1Latch = (m_t1Latch & 0x00FF) | (val << 8); break;
        case SR:       m_srData = val; break;
        case ACR:      m_acr = val; break;
        case PCR:      m_pcr = val; break;
        case IFR: {
            // Writing 1 to a bit clears the interrupt flag
            m_ifr &= ~val;
            break;
        }
        case IER: {
            m_ier = val & 0x7F;  // Bit 7 sets interrupt enable
            if (val & 0x80) {
                m_ifr |= 0x80;  // Set pending bit
            }
            break;
        }
        case ORB:      m_orB = val; break;
    }
}

uint8_t VIA::readPortA() {
    if (m_portAReadCallback) {
        return m_portAReadCallback();
    }
    return m_portA;
}

void VIA::writePortA(uint8_t val) {
    m_orA = val;
    if (m_portAWriteCallback) {
        m_portAWriteCallback(val);
    }
}

uint8_t VIA::readPortB() {
    if (m_portBReadCallback) {
        return m_portBReadCallback();
    }
    return m_portB;
}

void VIA::writePortB(uint8_t val) {
    m_orB = val;
    if (m_portBWriteCallback) {
        m_portBWriteCallback(val);
    }
}

void VIA::cycle(uint32_t cycles) {
    // Timer 1 countdown
    if (m_t1Running && m_t1Counter > cycles) {
        m_t1Counter -= cycles;
        if (m_t1Counter <= 0) {
            m_t1Counter = m_t1Latch;
            m_t1FirstMatch = true;
            m_ifr |= 0x04;  // Set Timer 1 interrupt flag
        }
    } else if (m_t1Running) {
        m_t1Counter = 0;
    }

    // Timer 2 countdown (for 5551 clock)
    // T2 counts at 1 MHz (1/14 of video clock)
    if (m_acr & 0x40) {  // T2 free-running mode
        for (uint32_t i = 0; i < cycles; i++) {
            if (--m_t2Counter == 0) {
                m_t2Counter = 0x100;
                m_t2Overflow = !m_t2Overflow;
            }
        }
    }
}

void VIA::setSpeakerState(bool state) {
    m_speakerState = state;
}

bool VIA::getSpeakerState() const {
    return m_speakerState;
}

// ============================================================
// PIA (6820)
// ============================================================

PIA::PIA() {}

void PIA::setPortAReadCallback(PortAReadCallback cb) {
    m_portAReadCallback = cb;
}

void PIA::setPortAWriteCallback(PortAWriteCallback cb) {
    m_portAWriteCallback = cb;
}

void PIA::setPortBReadCallback(PortBReadCallback cb) {
    m_portBReadCallback = cb;
}

void PIA::setPortBWriteCallback(PortBWriteCallback cb) {
    m_portBWriteCallback = cb;
}

uint8_t PIA::read(uint8_t reg) {
    switch (reg) {
        case DATA_A: {
            if (m_portAReadCallback) {
                return m_portAReadCallback();
            }
            return m_portA;
        }
        case DDR_A:  return m_ddrA;
        case DATA_B: {
            if (m_portBReadCallback) {
                return m_portBReadCallback();
            }
            return m_portB;
        }
        case DDR_B:  return m_ddrB;
        default:     return 0;
    }
}

void PIA::write(uint8_t reg, uint8_t val) {
    switch (reg) {
        case DATA_A:
            m_orA = val;
            if (m_portAWriteCallback) m_portAWriteCallback(val);
            break;
        case DDR_A:  m_ddrA = val; break;
        case DATA_B:
            m_orB = val;
            if (m_portBWriteCallback) m_portBWriteCallback(val);
            break;
        case DDR_B:  m_ddrB = val; break;
    }
}

// ============================================================
// IOController
// ============================================================

IOController::IOController() {}

void IOController::setVia1(VIA* via) {
    m_via1 = via;
}

void IOController::setVia2(VIA* via) {
    m_via2 = via;
}

void IOController::setPIA(PIA* pia) {
    m_pia = pia;
}

uint8_t IOController::read(uint16_t addr) {
    // Apple IIe I/O address decoding
    // The low nibble determines the register, the high nibble determines the device

    uint8_t highNibble = (addr >> 4) & 0x0F;
    uint8_t lowNibble = addr & 0x0F;

    // VIA1: $C020-$C027 (keyboard, cassette, speaker)
    if (highNibble == 0x2 && m_via1) {
        return m_via1->read(lowNibble);
    }

    // PIA: $C060-$C063 (keyboard matrix, printer)
    if (highNibble == 0x6 && m_pia) {
        return m_pia->read(lowNibble);
    }

    // VIA2: $C0A0-$C0A7 (disk, serial)
    if (highNibble == 0xA && m_via2) {
        return m_via2->read(lowNibble);
    }

    // Unknown I/O - return 0xFF (floating bus)
    return 0xFF;
}

void IOController::write(uint16_t addr, uint8_t val) {
    uint8_t highNibble = (addr >> 4) & 0x0F;
    uint8_t lowNibble = addr & 0x0F;

    // VIA1: $C020-$C027
    if (highNibble == 0x2 && m_via1) {
        m_via1->write(lowNibble, val);
        // Speaker is controlled via VIA1 Port B
        // Bit 7 of Port B controls speaker relay
        bool speakerOn = val & 0x80;
        m_via1->setSpeakerState(speakerOn);
        return;
    }

    // PIA: $C060-$C063
    if (highNibble == 0x6 && m_pia) {
        m_pia->write(lowNibble, val);
        return;
    }

    // VIA2: $C0A0-$C0A7
    if (highNibble == 0xA && m_via2) {
        m_via2->write(lowNibble, val);
        return;
    }
}

void IOController::setSpeakerVolume(float volume) {
    m_speakerVolume = volume;
}

float IOController::getSpeakerVolume() const {
    return m_speakerVolume;
}

void IOController::cycle(uint32_t cycles) {
    if (m_via1) m_via1->cycle(cycles);
    if (m_via2) m_via2->cycle(cycles);
}

} // namespace apple2e
