#pragma once

#include "cpu.h"

#include <cstdint>

namespace apple2e {

// Zilog Z80, including the undocumented flags (X/Y), IXH/IXL/IYH/IYL and
// DDCB/FDCB register copies. I/O ports read as $FF (nothing is wired to
// them on the SoftCard). Interrupts are not used and not implemented.
class Z80 {
public:
    explicit Z80(Bus& bus);

    void reset();

    // Execute one instruction, returns T-states used
    uint32_t step();

    uint16_t pc() const { return m_pc; }
    void setPC(uint16_t pc) { m_pc = pc; }
    uint16_t sp() const { return m_sp; }
    uint8_t a() const { return m_a; }
    uint8_t c() const { return m_bc & 0xFF; }
    uint8_t e() const { return m_de & 0xFF; }
    uint16_t de() const { return m_de; }
    bool halted() const { return m_halted; }

    // Return from the current subroutine (for harnesses that trap calls)
    void ret() { m_pc = pop(); }

private:
    // Instruction groups
    void execMain(uint8_t op);
    void execCB();
    void execIndexedCB();
    void execED();

    // Memory
    uint8_t rd(uint16_t addr) { return m_bus.read(addr); }
    void wr(uint16_t addr, uint8_t val) { m_bus.write(addr, val); }
    uint16_t rd16(uint16_t addr) { return rd(addr) | (rd(addr + 1) << 8); }
    void wr16(uint16_t addr, uint16_t val) { wr(addr, val & 0xFF); wr(addr + 1, val >> 8); }
    uint8_t fetch() { return rd(m_pc++); }
    uint16_t fetch16() { uint16_t lo = fetch(); return lo | (fetch() << 8); }
    uint8_t fetchOpcode();
    void push(uint16_t val) { m_sp -= 2; wr16(m_sp, val); }
    uint16_t pop() { uint16_t v = rd16(m_sp); m_sp += 2; return v; }

    // I/O (unconnected)
    uint8_t in(uint16_t) { return 0xFF; }
    void out(uint16_t, uint8_t) {}

    // Registers. With a DD/FD prefix, "HL" means IX/IY and H/L mean their halves.
    uint16_t& hlx() { return m_index == 0 ? m_hl : (m_index == 1 ? m_ix : m_iy); }
    uint8_t reg(int r);                   // r[0..7] except 6, honouring the prefix
    void setReg(int r, uint8_t val);
    uint8_t regPlain(int r);              // ignoring the prefix (used with (IX+d))
    void setRegPlain(int r, uint8_t val);
    uint16_t rp(int p);                   // BC, DE, HL/IX/IY, SP
    void setRp(int p, uint16_t val);
    uint16_t rp2(int p);                  // BC, DE, HL/IX/IY, AF
    void setRp2(int p, uint16_t val);
    uint16_t memOperand();                // address of (HL) or (IX+d)/(IY+d)
    bool condition(int cc) const;

    // ALU
    void alu(int op, uint8_t val);
    void add8(uint8_t val, int carry);
    void sub8(uint8_t val, int carry);
    void cp8(uint8_t val);
    uint8_t inc8(uint8_t val);
    uint8_t dec8(uint8_t val);
    uint16_t add16(uint16_t a, uint16_t b);
    void adc16(uint16_t val);
    void sbc16(uint16_t val);
    uint8_t rotate(int op, uint8_t val);
    void bit(int n, uint8_t val, uint8_t xy);
    void daa();

    // Block instructions (dir = +1/-1)
    void blockLd(int dir, bool repeat);
    void blockCp(int dir, bool repeat);
    void blockIn(int dir, bool repeat);
    void blockOut(int dir, bool repeat);

    Bus& m_bus;

    uint8_t m_a = 0xFF;
    uint8_t m_f = 0xFF;
    uint16_t m_bc = 0, m_de = 0, m_hl = 0;
    uint16_t m_af2 = 0, m_bc2 = 0, m_de2 = 0, m_hl2 = 0;
    uint16_t m_ix = 0xFFFF, m_iy = 0xFFFF;
    uint16_t m_sp = 0xFFFF;
    uint16_t m_pc = 0;
    uint16_t m_wz = 0;  // internal MEMPTR, visible through BIT n,(HL) flags
    uint8_t m_i = 0, m_r = 0;
    bool m_iff1 = false, m_iff2 = false;
    int m_im = 0;
    bool m_halted = false;

    int m_index = 0;    // current prefix: 0 none, 1 DD (IX), 2 FD (IY)
    uint32_t m_tstates = 0;
};

} // namespace apple2e
