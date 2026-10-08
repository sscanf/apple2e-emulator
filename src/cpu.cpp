#include "cpu.h"
#include "state.h"

namespace apple2e {

namespace {

// Base cycle count per opcode (65C02). Undefined opcodes are NOPs.
constexpr uint8_t kCycles[256] = {
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    7, 6, 2, 1, 5, 3, 5, 1, 3, 2, 2, 1, 6, 4, 6, 1,  // 0
    2, 5, 5, 1, 5, 4, 6, 1, 2, 4, 2, 1, 6, 4, 6, 1,  // 1
    6, 6, 2, 1, 3, 3, 5, 1, 4, 2, 2, 1, 4, 4, 6, 1,  // 2
    2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 2, 1, 4, 4, 6, 1,  // 3
    6, 6, 2, 1, 3, 3, 5, 1, 3, 2, 2, 1, 3, 4, 6, 1,  // 4
    2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 3, 1, 8, 4, 6, 1,  // 5
    6, 6, 2, 1, 3, 3, 5, 1, 4, 2, 2, 1, 6, 4, 6, 1,  // 6
    2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 4, 1, 6, 4, 6, 1,  // 7
    3, 6, 2, 1, 3, 3, 3, 1, 2, 2, 2, 1, 4, 4, 4, 1,  // 8
    2, 6, 5, 1, 4, 4, 4, 1, 2, 5, 2, 1, 4, 5, 5, 1,  // 9
    2, 6, 2, 1, 3, 3, 3, 1, 2, 2, 2, 1, 4, 4, 4, 1,  // A
    2, 5, 5, 1, 4, 4, 4, 1, 2, 4, 2, 1, 4, 4, 4, 1,  // B
    2, 6, 2, 1, 3, 3, 5, 1, 2, 2, 2, 1, 4, 4, 6, 1,  // C
    2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 3, 1, 4, 4, 7, 1,  // D
    2, 6, 2, 1, 3, 3, 5, 1, 2, 2, 2, 1, 4, 4, 6, 1,  // E
    2, 5, 5, 1, 4, 4, 6, 1, 2, 4, 4, 1, 4, 4, 7, 1,  // F
};

bool pageCrossed(uint16_t a, uint16_t b) { return (a ^ b) & 0xFF00; }

} // namespace

CPU::CPU(Bus& bus, Variant variant) : m_bus(bus), m_variant(variant) {}

void CPU::reset() {
    m_sp = 0xFD;
    m_p = (m_p | FLAG_U | FLAG_I) & ~FLAG_D;
    m_irqLine = false;
    m_nmiPending = false;
    m_pc = read16(0xFFFC);
}

uint32_t CPU::step() {
    if (m_nmiPending) {
        m_nmiPending = false;
        interrupt(0xFFFA, false);
        return 7;
    }
    if (m_irqLine && !(m_p & FLAG_I)) {
        interrupt(0xFFFE, false);
        return 7;
    }

    m_extraCycles = 0;
    uint8_t op = fetch();
    if (m_variant == Variant::Rockwell65C02 && (op & 0x07) == 0x07) {
        // RMBn/SMBn take 5 cycles, BBRn/BBSn 5 (+branch penalty)
        executeRockwell(op);
        return 5 + m_extraCycles;
    }
    execute(op);
    return kCycles[op] + m_extraCycles;
}

void CPU::interrupt(uint16_t vector, bool brk) {
    push16(m_pc);
    uint8_t pushed = m_p | FLAG_U;
    push(brk ? (pushed | FLAG_B) : (pushed & ~FLAG_B));
    m_p = (m_p | FLAG_I) & ~FLAG_D;  // 65C02 clears D on interrupts
    m_pc = read16(vector);
}

// ============================================================
// Addressing modes
// ============================================================

uint16_t CPU::readZPPointer(uint8_t zp) {
    return read(zp) | (read(static_cast<uint8_t>(zp + 1)) << 8);
}

uint16_t CPU::addrABX(bool penalty) {
    uint16_t base = fetch16();
    uint16_t addr = base + m_x;
    if (penalty && pageCrossed(base, addr)) m_extraCycles++;
    return addr;
}

uint16_t CPU::addrABY(bool penalty) {
    uint16_t base = fetch16();
    uint16_t addr = base + m_y;
    if (penalty && pageCrossed(base, addr)) m_extraCycles++;
    return addr;
}

uint16_t CPU::addrIZX() {
    return readZPPointer(static_cast<uint8_t>(fetch() + m_x));
}

uint16_t CPU::addrIZY(bool penalty) {
    uint16_t base = readZPPointer(fetch());
    uint16_t addr = base + m_y;
    if (penalty && pageCrossed(base, addr)) m_extraCycles++;
    return addr;
}

uint16_t CPU::addrIZP() {
    return readZPPointer(fetch());
}

// ============================================================
// ALU
// ============================================================

void CPU::adc(uint8_t v) {
    unsigned carry = m_p & FLAG_C;

    if (m_p & FLAG_D) {
        // 65C02 decimal mode (N/Z/V/C all valid), per 6502.org "Decimal Mode"
        unsigned lo = (m_a & 0x0F) + (v & 0x0F) + carry;
        if (lo >= 0x0A) lo = ((lo + 0x06) & 0x0F) + 0x10;
        unsigned result = (m_a & 0xF0) + (v & 0xF0) + lo;
        int sresult = static_cast<int8_t>(m_a & 0xF0) + static_cast<int8_t>(v & 0xF0) + static_cast<int>(lo);
        setFlag(FLAG_V, sresult < -128 || sresult > 127);
        if (result >= 0xA0) result += 0x60;
        setFlag(FLAG_C, result >= 0x100);
        m_a = result & 0xFF;
        m_extraCycles++;
    } else {
        unsigned sum = m_a + v + carry;
        setFlag(FLAG_V, ~(m_a ^ v) & (m_a ^ sum) & 0x80);
        setFlag(FLAG_C, sum > 0xFF);
        m_a = sum & 0xFF;
    }
    setNZ(m_a);
}

void CPU::sbc(uint8_t v) {
    if (!(m_p & FLAG_D)) {
        adc(v ^ 0xFF);
        return;
    }

    // Carry and overflow behave exactly as in binary mode
    int carry = m_p & FLAG_C;
    unsigned bin = m_a + (v ^ 0xFF) + carry;
    setFlag(FLAG_V, ~(m_a ^ (v ^ 0xFF)) & (m_a ^ bin) & 0x80);
    setFlag(FLAG_C, bin > 0xFF);

    int lo = (m_a & 0x0F) - (v & 0x0F) + carry - 1;
    int result = m_a - v + carry - 1;
    if (result < 0) result -= 0x60;
    if (lo < 0) result -= 0x06;
    m_a = result & 0xFF;
    setNZ(m_a);
    m_extraCycles++;
}

void CPU::compare(uint8_t reg, uint8_t v) {
    setFlag(FLAG_C, reg >= v);
    setNZ(static_cast<uint8_t>(reg - v));
}

void CPU::bit(uint8_t v) {
    setFlag(FLAG_Z, (m_a & v) == 0);
    setFlag(FLAG_N, v & 0x80);
    setFlag(FLAG_V, v & 0x40);
}

void CPU::branch(bool cond) {
    int8_t offset = static_cast<int8_t>(fetch());
    if (!cond) return;
    uint16_t target = m_pc + offset;
    m_extraCycles += pageCrossed(m_pc, target) ? 2 : 1;
    m_pc = target;
}

uint8_t CPU::asl(uint8_t v) {
    setFlag(FLAG_C, v & 0x80);
    v <<= 1;
    setNZ(v);
    return v;
}

uint8_t CPU::lsr(uint8_t v) {
    setFlag(FLAG_C, v & 0x01);
    v >>= 1;
    setNZ(v);
    return v;
}

uint8_t CPU::rol(uint8_t v) {
    uint8_t carryIn = m_p & FLAG_C;
    setFlag(FLAG_C, v & 0x80);
    v = (v << 1) | carryIn;
    setNZ(v);
    return v;
}

uint8_t CPU::ror(uint8_t v) {
    uint8_t carryIn = (m_p & FLAG_C) ? 0x80 : 0;
    setFlag(FLAG_C, v & 0x01);
    v = (v >> 1) | carryIn;
    setNZ(v);
    return v;
}

// ============================================================
// Instruction decode
// ============================================================

// Opcodes of the form aaabbbcc share addressing-mode columns; these macros
// expand one group per mnemonic
#define ALU_GROUP(base, fn)                                      \
    case base + 0x01: fn(read(addrIZX())); break;                \
    case base + 0x05: fn(read(addrZP())); break;                 \
    case base + 0x09: fn(fetch()); break;                        \
    case base + 0x0D: fn(read(addrABS())); break;                \
    case base + 0x11: fn(read(addrIZY(true))); break;            \
    case base + 0x12: fn(read(addrIZP())); break;                \
    case base + 0x15: fn(read(addrZPX())); break;                \
    case base + 0x19: fn(read(addrABY(true))); break;            \
    case base + 0x1D: fn(read(addrABX(true))); break;

#define SHIFT_GROUP(base, fn)                                    \
    case base + 0x06: rmw(addrZP(), &CPU::fn); break;            \
    case base + 0x0A: m_a = fn(m_a); break;                      \
    case base + 0x0E: rmw(addrABS(), &CPU::fn); break;           \
    case base + 0x16: rmw(addrZPX(), &CPU::fn); break;           \
    case base + 0x1E: rmw(addrABX(false), &CPU::fn); break;

void CPU::execute(uint8_t op) {
    switch (op) {
        ALU_GROUP(0x00, ora)
        ALU_GROUP(0x20, and_)
        ALU_GROUP(0x40, eor)
        ALU_GROUP(0x60, adc)
        ALU_GROUP(0xA0, lda)
        ALU_GROUP(0xC0, cmpA)
        ALU_GROUP(0xE0, sbc)

        SHIFT_GROUP(0x00, asl)
        SHIFT_GROUP(0x20, rol)
        SHIFT_GROUP(0x40, lsr)
        SHIFT_GROUP(0x60, ror)

        // STA
        case 0x81: write(addrIZX(), m_a); break;
        case 0x85: write(addrZP(), m_a); break;
        case 0x8D: write(addrABS(), m_a); break;
        case 0x91: write(addrIZY(false), m_a); break;
        case 0x92: write(addrIZP(), m_a); break;
        case 0x95: write(addrZPX(), m_a); break;
        case 0x99: write(addrABY(false), m_a); break;
        case 0x9D: write(addrABX(false), m_a); break;

        // STX / STY / STZ
        case 0x86: write(addrZP(), m_x); break;
        case 0x8E: write(addrABS(), m_x); break;
        case 0x96: write(addrZPY(), m_x); break;
        case 0x84: write(addrZP(), m_y); break;
        case 0x8C: write(addrABS(), m_y); break;
        case 0x94: write(addrZPX(), m_y); break;
        case 0x64: write(addrZP(), 0); break;
        case 0x74: write(addrZPX(), 0); break;
        case 0x9C: write(addrABS(), 0); break;
        case 0x9E: write(addrABX(false), 0); break;

        // LDX / LDY
        case 0xA2: m_x = fetch(); setNZ(m_x); break;
        case 0xA6: m_x = read(addrZP()); setNZ(m_x); break;
        case 0xAE: m_x = read(addrABS()); setNZ(m_x); break;
        case 0xB6: m_x = read(addrZPY()); setNZ(m_x); break;
        case 0xBE: m_x = read(addrABY(true)); setNZ(m_x); break;
        case 0xA0: m_y = fetch(); setNZ(m_y); break;
        case 0xA4: m_y = read(addrZP()); setNZ(m_y); break;
        case 0xAC: m_y = read(addrABS()); setNZ(m_y); break;
        case 0xB4: m_y = read(addrZPX()); setNZ(m_y); break;
        case 0xBC: m_y = read(addrABX(true)); setNZ(m_y); break;

        // CPX / CPY
        case 0xE0: compare(m_x, fetch()); break;
        case 0xE4: compare(m_x, read(addrZP())); break;
        case 0xEC: compare(m_x, read(addrABS())); break;
        case 0xC0: compare(m_y, fetch()); break;
        case 0xC4: compare(m_y, read(addrZP())); break;
        case 0xCC: compare(m_y, read(addrABS())); break;

        // BIT (immediate form only affects Z)
        case 0x24: bit(read(addrZP())); break;
        case 0x2C: bit(read(addrABS())); break;
        case 0x34: bit(read(addrZPX())); break;
        case 0x3C: bit(read(addrABX(true))); break;
        case 0x89: setFlag(FLAG_Z, (m_a & fetch()) == 0); break;

        // INC / DEC
        case 0xE6: rmw(addrZP(), &CPU::inc); break;
        case 0xEE: rmw(addrABS(), &CPU::inc); break;
        case 0xF6: rmw(addrZPX(), &CPU::inc); break;
        case 0xFE: rmw(addrABX(false), &CPU::inc); break;
        case 0x1A: m_a = inc(m_a); break;
        case 0xC6: rmw(addrZP(), &CPU::dec); break;
        case 0xCE: rmw(addrABS(), &CPU::dec); break;
        case 0xD6: rmw(addrZPX(), &CPU::dec); break;
        case 0xDE: rmw(addrABX(false), &CPU::dec); break;
        case 0x3A: m_a = dec(m_a); break;
        case 0xE8: m_x = inc(m_x); break;
        case 0xC8: m_y = inc(m_y); break;
        case 0xCA: m_x = dec(m_x); break;
        case 0x88: m_y = dec(m_y); break;

        // TSB / TRB
        case 0x04: rmw(addrZP(), &CPU::tsb); break;
        case 0x0C: rmw(addrABS(), &CPU::tsb); break;
        case 0x14: rmw(addrZP(), &CPU::trb); break;
        case 0x1C: rmw(addrABS(), &CPU::trb); break;

        // Branches
        case 0x10: branch(!(m_p & FLAG_N)); break;
        case 0x30: branch(m_p & FLAG_N); break;
        case 0x50: branch(!(m_p & FLAG_V)); break;
        case 0x70: branch(m_p & FLAG_V); break;
        case 0x90: branch(!(m_p & FLAG_C)); break;
        case 0xB0: branch(m_p & FLAG_C); break;
        case 0xD0: branch(!(m_p & FLAG_Z)); break;
        case 0xF0: branch(m_p & FLAG_Z); break;
        case 0x80: {  // BRA: base cycles already count the taken branch
            int8_t offset = static_cast<int8_t>(fetch());
            uint16_t target = m_pc + offset;
            if (pageCrossed(m_pc, target)) m_extraCycles++;
            m_pc = target;
            break;
        }

        // Jumps and subroutines
        case 0x4C: m_pc = fetch16(); break;
        case 0x6C: m_pc = read16(fetch16()); break;  // 65C02: no page-wrap bug
        case 0x7C: m_pc = read16(fetch16() + m_x); break;
        case 0x20: {
            uint16_t target = fetch16();
            push16(m_pc - 1);
            m_pc = target;
            break;
        }
        case 0x60: m_pc = pop16() + 1; break;
        case 0x40:
            m_p = (pop() | FLAG_U) & ~FLAG_B;
            m_pc = pop16();
            break;
        case 0x00:
            m_pc++;  // BRK skips its signature byte
            interrupt(0xFFFE, true);
            break;

        // Stack
        case 0x48: push(m_a); break;
        case 0xDA: push(m_x); break;
        case 0x5A: push(m_y); break;
        case 0x08: push(m_p | FLAG_B | FLAG_U); break;
        case 0x68: m_a = pop(); setNZ(m_a); break;
        case 0xFA: m_x = pop(); setNZ(m_x); break;
        case 0x7A: m_y = pop(); setNZ(m_y); break;
        case 0x28: m_p = (pop() | FLAG_U) & ~FLAG_B; break;

        // Transfers
        case 0xAA: m_x = m_a; setNZ(m_x); break;
        case 0xA8: m_y = m_a; setNZ(m_y); break;
        case 0x8A: m_a = m_x; setNZ(m_a); break;
        case 0x98: m_a = m_y; setNZ(m_a); break;
        case 0xBA: m_x = m_sp; setNZ(m_x); break;
        case 0x9A: m_sp = m_x; break;

        // Flags
        case 0x18: setFlag(FLAG_C, false); break;
        case 0x38: setFlag(FLAG_C, true); break;
        case 0x58: setFlag(FLAG_I, false); break;
        case 0x78: setFlag(FLAG_I, true); break;
        case 0xB8: setFlag(FLAG_V, false); break;
        case 0xD8: setFlag(FLAG_D, false); break;
        case 0xF8: setFlag(FLAG_D, true); break;

        // Undefined opcodes: NOPs that still consume their operand bytes
        case 0x02: case 0x22: case 0x42: case 0x62:
        case 0x82: case 0xC2: case 0xE2:
        case 0x44: case 0x54: case 0xD4: case 0xF4:
            m_pc++;
            break;
        case 0x5C: case 0xDC: case 0xFC:
            m_pc += 2;
            break;

        case 0xEA:  // NOP
        default:    // remaining undefined opcodes are 1-byte NOPs
            break;
    }
}

// Rockwell bit instructions: column 7 is RMBn/SMBn zp, column F is BBRn/BBSn zp,rel
void CPU::executeRockwell(uint8_t op) {
    uint8_t mask = 1 << ((op >> 4) & 0x07);
    bool setVariant = op & 0x80;
    uint8_t zp = fetch();
    uint8_t val = read(zp);

    if ((op & 0x0F) == 0x07) {
        write(zp, setVariant ? (val | mask) : (val & ~mask));
    } else {
        bool bitSet = val & mask;
        branch(setVariant ? bitSet : !bitSet);
    }
}

#undef ALU_GROUP
#undef SHIFT_GROUP

// ============================================================
// Save states
// ============================================================

void CPU::saveState(StateWriter& w) const {
    w.put(m_a); w.put(m_x); w.put(m_y); w.put(m_sp); w.put(m_p); w.put(m_pc);
    w.put(m_irqLine); w.put(m_nmiPending);
}

void CPU::loadState(StateReader& r) {
    r.get(m_a); r.get(m_x); r.get(m_y); r.get(m_sp); r.get(m_p); r.get(m_pc);
    r.get(m_irqLine); r.get(m_nmiPending);
}

} // namespace apple2e
