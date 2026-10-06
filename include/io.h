#pragma once

#include <cstdint>
#include <functional>
#include <array>

namespace apple2e {

// VIA (Versatile Interface Adapter) - 6522
// Used for keyboard, cassette, speaker, timer
class VIA {
public:
    using PortAReadCallback = std::function<uint8_t()>;
    using PortAWriteCallback = std::function<void(uint8_t val)>;
    using PortBReadCallback = std::function<uint8_t()>;
    using PortBWriteCallback = std::function<void(uint8_t val)>;

    VIA();

    void setPortAReadCallback(PortAReadCallback cb);
    void setPortAWriteCallback(PortAWriteCallback cb);
    void setPortBReadCallback(PortBReadCallback cb);
    void setPortBWriteCallback(PortBWriteCallback cb);

    // Read/write registers
    uint8_t read(uint8_t reg);
    void write(uint8_t reg, uint8_t val);

    // Cycle-accurate timer
    void cycle(uint32_t cycles);

    // Speaker control
    void setSpeakerState(bool state);
    bool getSpeakerState() const;

private:
    // VIA registers
    enum Reg : uint8_t {
        PORTA    = 0x00,
        DDR_A    = 0x01,
        PORTB    = 0x02,
        DDR_B    = 0x03,
        T1_LO    = 0x04,
        T1_HI    = 0x05,
        T1_LATCH_LO = 0x06,
        T1_LATCH_HI = 0x07,
        SR       = 0x08,
        ACR      = 0x09,
        PCR      = 0x0A,
        IFR      = 0x0B,
        IER      = 0x0C,
        ORB      = 0x0D,  // Output latch B (shadow)
    };

    // Data registers
    uint8_t m_portA = 0;
    uint8_t m_ddrA = 0;
    uint8_t m_portB = 0;
    uint8_t m_ddrB = 0;

    // Timer 1
    uint16_t m_t1Counter = 0;
    uint16_t m_t1Latch = 0;
    bool m_t1FirstMatch = false;
    bool m_t1Running = false;

    // Timer 2 (5551 clock emulation)
    uint16_t m_t2Counter = 0;
    bool m_t2Overflow = false;

    // Shift register
    uint8_t m_srData = 0;
    uint8_t m_srCount = 0;

    // Control registers
    uint8_t m_acr = 0;
    uint8_t m_pcr = 0;
    uint8_t m_ifr = 0;
    uint8_t m_ier = 0;

    // Output latches
    uint8_t m_orA = 0;
    uint8_t m_orB = 0;

    // Speaker
    bool m_speakerState = false;

    // Callbacks
    PortAReadCallback m_portAReadCallback = nullptr;
    PortAWriteCallback m_portAWriteCallback = nullptr;
    PortBReadCallback m_portBReadCallback = nullptr;
    PortBWriteCallback m_portBWriteCallback = nullptr;

    uint8_t readPortA();
    void writePortA(uint8_t val);
    uint8_t readPortB();
    void writePortB(uint8_t val);
};

// PIA (Peripheral Interface Adapter) - 6820
// Used for keyboard matrix and parallel port
class PIA {
public:
    using PortAReadCallback = std::function<uint8_t()>;
    using PortAWriteCallback = std::function<void(uint8_t val)>;
    using PortBReadCallback = std::function<uint8_t()>;
    using PortBWriteCallback = std::function<void(uint8_t val)>;

    PIA();

    void setPortAReadCallback(PortAReadCallback cb);
    void setPortAWriteCallback(PortAWriteCallback cb);
    void setPortBReadCallback(PortBReadCallback cb);
    void setPortBWriteCallback(PortBWriteCallback cb);

    uint8_t read(uint8_t reg);
    void write(uint8_t reg, uint8_t val);

private:
    enum Reg : uint8_t {
        DATA_A = 0,
        DDR_A  = 1,
        DATA_B = 2,
        DDR_B  = 3,
    };

    uint8_t m_portA = 0;
    uint8_t m_ddrA = 0;
    uint8_t m_portB = 0;
    uint8_t m_ddrB = 0;
    uint8_t m_orA = 0;
    uint8_t m_orB = 0;

    PortAReadCallback m_portAReadCallback = nullptr;
    PortAWriteCallback m_portAWriteCallback = nullptr;
    PortBReadCallback m_portBReadCallback = nullptr;
    PortBWriteCallback m_portBWriteCallback = nullptr;
};

// Apple IIe I/O controller
class IOController {
public:
    IOController();

    // Register devices
    void setVia1(VIA* via);
    void setVia2(VIA* via);
    void setPIA(PIA* pia);

    // Read/write I/O ports
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t val);

    // Speaker
    void setSpeakerVolume(float volume);
    float getSpeakerVolume() const;

    // Cycle-accurate timing
    void cycle(uint32_t cycles);

private:
    VIA* m_via1 = nullptr;
    VIA* m_via2 = nullptr;
    PIA* m_pia = nullptr;

    float m_speakerVolume = 0.0f;

    // I/O address decoding
    bool isIoAddress(uint16_t addr) const;
    VIA* getViaForAddress(uint16_t addr);
    PIA* getPIAForAddress(uint16_t addr);
};

} // namespace apple2e
