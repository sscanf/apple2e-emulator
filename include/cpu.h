#pragma once

#include <cstdint>

namespace apple2e {

class StateReader;
class StateWriter;

// Memory interface seen by the CPU
class Bus {
public:
    virtual ~Bus() = default;
    virtual uint8_t read(uint16_t addr) = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};

// Status register (P) bits
enum : uint8_t {
    FLAG_C = 0x01,  // Carry
    FLAG_Z = 0x02,  // Zero
    FLAG_I = 0x04,  // IRQ disable
    FLAG_D = 0x08,  // Decimal mode
    FLAG_B = 0x10,  // Break (only exists on the stack)
    FLAG_U = 0x20,  // Unused, always 1
    FLAG_V = 0x40,  // Overflow
    FLAG_N = 0x80,  // Negative
};

// 65C02 as fitted to the enhanced Apple IIe (NCR/GTE part: no Rockwell
// BBR/BBS/RMB/SMB and no WDC WAI/STP; those opcodes execute as NOPs).
// The Rockwell variant adds the bit instructions; it exists so the core can be
// validated against test suites that exercise them.
class CPU {
public:
    enum class Variant { NCR65C02, Rockwell65C02 };

    explicit CPU(Bus& bus, Variant variant = Variant::NCR65C02);

    void reset();
    void setIrqLine(bool asserted) { m_irqLine = asserted; }
    void nmi() { m_nmiPending = true; }

    // Execute one instruction (or service a pending interrupt), returns cycles used
    uint32_t step();

    uint16_t pc() const { return m_pc; }
    uint8_t a() const { return m_a; }
    uint8_t x() const { return m_x; }
    uint8_t y() const { return m_y; }
    uint8_t sp() const { return m_sp; }
    uint8_t p() const { return m_p; }
    void setPC(uint16_t pc) { m_pc = pc; }

    // Save states (see state.h)
    void saveState(StateWriter& w) const;
    void loadState(StateReader& r);

private:
    void execute(uint8_t op);
    void executeRockwell(uint8_t op);
    void interrupt(uint16_t vector, bool brk);

    // Memory access
    uint8_t read(uint16_t addr) { return m_bus.read(addr); }
    void write(uint16_t addr, uint8_t val) { m_bus.write(addr, val); }
    uint16_t read16(uint16_t addr) { return read(addr) | (read(addr + 1) << 8); }
    uint8_t fetch() { return read(m_pc++); }
    uint16_t fetch16() { uint16_t v = read16(m_pc); m_pc += 2; return v; }

    // Stack
    void push(uint8_t val) { write(0x0100 | m_sp--, val); }
    uint8_t pop() { return read(0x0100 | ++m_sp); }
    void push16(uint16_t val) { push(val >> 8); push(val & 0xFF); }
    uint16_t pop16() { uint8_t lo = pop(); return lo | (pop() << 8); }

    // Addressing modes (return effective address). `penalty` adds the extra
    // cycle that read instructions take when indexing crosses a page.
    uint16_t addrZP() { return fetch(); }
    uint16_t addrZPX() { return static_cast<uint8_t>(fetch() + m_x); }
    uint16_t addrZPY() { return static_cast<uint8_t>(fetch() + m_y); }
    uint16_t addrABS() { return fetch16(); }
    uint16_t addrABX(bool penalty);
    uint16_t addrABY(bool penalty);
    uint16_t addrIZX();               // (zp,X)
    uint16_t addrIZY(bool penalty);   // (zp),Y
    uint16_t addrIZP();               // (zp)   - 65C02
    uint16_t readZPPointer(uint8_t zp);

    // Flags
    void setFlag(uint8_t flag, bool on) { m_p = on ? (m_p | flag) : (m_p & ~flag); }
    void setNZ(uint8_t v) { setFlag(FLAG_Z, v == 0); setFlag(FLAG_N, v & 0x80); }

    // ALU
    void lda(uint8_t v) { m_a = v; setNZ(v); }
    void ora(uint8_t v) { m_a |= v; setNZ(m_a); }
    void and_(uint8_t v) { m_a &= v; setNZ(m_a); }
    void eor(uint8_t v) { m_a ^= v; setNZ(m_a); }
    void adc(uint8_t v);
    void sbc(uint8_t v);
    void compare(uint8_t reg, uint8_t v);
    void cmpA(uint8_t v) { compare(m_a, v); }
    void bit(uint8_t v);
    void branch(bool cond);

    // Read-modify-write
    using RmwFn = uint8_t (CPU::*)(uint8_t);
    void rmw(uint16_t addr, RmwFn fn) { write(addr, (this->*fn)(read(addr))); }
    uint8_t asl(uint8_t v);
    uint8_t lsr(uint8_t v);
    uint8_t rol(uint8_t v);
    uint8_t ror(uint8_t v);
    uint8_t inc(uint8_t v) { setNZ(++v); return v; }
    uint8_t dec(uint8_t v) { setNZ(--v); return v; }
    uint8_t tsb(uint8_t v) { setFlag(FLAG_Z, (v & m_a) == 0); return v | m_a; }
    uint8_t trb(uint8_t v) { setFlag(FLAG_Z, (v & m_a) == 0); return v & ~m_a; }

    Bus& m_bus;
    Variant m_variant;

    uint8_t m_a = 0;
    uint8_t m_x = 0;
    uint8_t m_y = 0;
    uint8_t m_sp = 0xFD;
    uint8_t m_p = FLAG_U | FLAG_I;
    uint16_t m_pc = 0;

    bool m_irqLine = false;
    bool m_nmiPending = false;

    // Cycles added on top of the base table (page crossing, branches, decimal)
    uint32_t m_extraCycles = 0;
};

} // namespace apple2e
