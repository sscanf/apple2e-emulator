#include "z80.h"
#include "state.h"

#include <utility>

namespace apple2e {

namespace {

enum : uint8_t {
    FC = 0x01, FN = 0x02, FPV = 0x04, FX = 0x08,
    FH = 0x10, FY = 0x20, FZ = 0x40, FS = 0x80,
};

struct FlagTables {
    uint8_t sz53[256];   // S, Z and the undocumented X/Y copies of bits 3/5
    uint8_t sz53p[256];  // ... plus even parity in P/V

    constexpr FlagTables() : sz53{}, sz53p{} {
        for (int i = 0; i < 256; i++) {
            uint8_t f = (i & (FS | FY | FX)) | (i == 0 ? FZ : 0);
            int bits = 0;
            for (int b = 0; b < 8; b++) bits += (i >> b) & 1;
            sz53[i] = f;
            sz53p[i] = f | ((bits & 1) ? 0 : FPV);
        }
    }
};
constexpr FlagTables kFlags;

// Base T-states for unprefixed opcodes (conditional branches: not taken)
constexpr uint8_t kCycles[256] = {
//   0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
     4, 10,  7,  6,  4,  4,  7,  4,  4, 11,  7,  6,  4,  4,  7,  4,  // 0
     8, 10,  7,  6,  4,  4,  7,  4, 12, 11,  7,  6,  4,  4,  7,  4,  // 1
     7, 10, 16,  6,  4,  4,  7,  4,  7, 11, 16,  6,  4,  4,  7,  4,  // 2
     7, 10, 13,  6, 11, 11, 10,  4,  7, 11, 13,  6,  4,  4,  7,  4,  // 3
     4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  // 4
     4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  // 5
     4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  // 6
     7,  7,  7,  7,  7,  7,  4,  7,  4,  4,  4,  4,  4,  4,  7,  4,  // 7
     4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  // 8
     4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  // 9
     4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  // A
     4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,  // B
     5, 10, 10, 10, 10, 11,  7, 11,  5, 10, 10,  0, 10, 17,  7, 11,  // C
     5, 10, 10, 11, 10, 11,  7, 11,  5,  4, 10, 11, 10,  0,  7, 11,  // D
     5, 10, 10, 19, 10, 11,  7, 11,  5,  4, 10,  4, 10,  0,  7, 11,  // E
     5, 10, 10,  4, 10, 11,  7, 11,  5,  6, 10,  4, 10,  0,  7, 11,  // F
};

} // namespace

Z80::Z80(Bus& bus) : m_bus(bus) {}

void Z80::reset() {
    m_pc = 0;
    m_i = 0;
    m_r = 0;
    m_iff1 = m_iff2 = false;
    m_im = 0;
    m_halted = false;
    m_sp = 0xFFFF;
    m_a = m_f = 0xFF;
}

uint8_t Z80::fetchOpcode() {
    m_r = (m_r & 0x80) | ((m_r + 1) & 0x7F);
    return fetch();
}

uint32_t Z80::step() {
    if (m_halted) {
        // HALT keeps executing NOPs until an interrupt
        m_r = (m_r & 0x80) | ((m_r + 1) & 0x7F);
        return 4;
    }

    m_index = 0;
    m_tstates = 0;
    uint8_t op = fetchOpcode();
    while (op == 0xDD || op == 0xFD) {
        m_index = op == 0xDD ? 1 : 2;
        m_tstates += 4;
        op = fetchOpcode();
    }

    if (op == 0xCB) {
        if (m_index) {
            execIndexedCB();
        } else {
            execCB();
        }
    } else if (op == 0xED) {
        m_index = 0;  // ED instructions ignore a DD/FD prefix
        execED();
    } else {
        execMain(op);
    }
    return m_tstates;
}

// ============================================================
// Register access
// ============================================================

uint8_t Z80::regPlain(int r) {
    switch (r) {
        case 0: return m_bc >> 8;
        case 1: return m_bc & 0xFF;
        case 2: return m_de >> 8;
        case 3: return m_de & 0xFF;
        case 4: return m_hl >> 8;
        case 5: return m_hl & 0xFF;
        default: return m_a;
    }
}

void Z80::setRegPlain(int r, uint8_t v) {
    switch (r) {
        case 0: m_bc = (m_bc & 0x00FF) | (v << 8); break;
        case 1: m_bc = (m_bc & 0xFF00) | v; break;
        case 2: m_de = (m_de & 0x00FF) | (v << 8); break;
        case 3: m_de = (m_de & 0xFF00) | v; break;
        case 4: m_hl = (m_hl & 0x00FF) | (v << 8); break;
        case 5: m_hl = (m_hl & 0xFF00) | v; break;
        default: m_a = v; break;
    }
}

uint8_t Z80::reg(int r) {
    if (r == 4) return hlx() >> 8;
    if (r == 5) return hlx() & 0xFF;
    return regPlain(r);
}

void Z80::setReg(int r, uint8_t v) {
    uint16_t& hl = hlx();
    if (r == 4) {
        hl = (hl & 0x00FF) | (v << 8);
    } else if (r == 5) {
        hl = (hl & 0xFF00) | v;
    } else {
        setRegPlain(r, v);
    }
}

uint16_t Z80::rp(int p) {
    switch (p) {
        case 0: return m_bc;
        case 1: return m_de;
        case 2: return hlx();
        default: return m_sp;
    }
}

void Z80::setRp(int p, uint16_t v) {
    switch (p) {
        case 0: m_bc = v; break;
        case 1: m_de = v; break;
        case 2: hlx() = v; break;
        default: m_sp = v; break;
    }
}

uint16_t Z80::rp2(int p) {
    return p == 3 ? static_cast<uint16_t>((m_a << 8) | m_f) : rp(p);
}

void Z80::setRp2(int p, uint16_t v) {
    if (p == 3) {
        m_a = v >> 8;
        m_f = v & 0xFF;
    } else {
        setRp(p, v);
    }
}

uint16_t Z80::memOperand() {
    if (m_index == 0) return m_hl;
    auto d = static_cast<int8_t>(fetch());
    uint16_t addr = hlx() + d;
    m_wz = addr;
    return addr;
}

bool Z80::condition(int cc) const {
    switch (cc) {
        case 0: return !(m_f & FZ);
        case 1: return m_f & FZ;
        case 2: return !(m_f & FC);
        case 3: return m_f & FC;
        case 4: return !(m_f & FPV);
        case 5: return m_f & FPV;
        case 6: return !(m_f & FS);
        default: return m_f & FS;
    }
}

// ============================================================
// ALU
// ============================================================

void Z80::add8(uint8_t v, int carry) {
    unsigned r = m_a + v + carry;
    m_f = kFlags.sz53[r & 0xFF] | ((r >> 8) & FC) | ((m_a ^ v ^ r) & FH) |
          ((~(m_a ^ v) & (m_a ^ r) & 0x80) ? FPV : 0);
    m_a = static_cast<uint8_t>(r);
}

void Z80::sub8(uint8_t v, int carry) {
    unsigned r = m_a - v - carry;
    m_f = kFlags.sz53[r & 0xFF] | FN | ((r >> 8) & FC) | ((m_a ^ v ^ r) & FH) |
          (((m_a ^ v) & (m_a ^ r) & 0x80) ? FPV : 0);
    m_a = static_cast<uint8_t>(r);
}

void Z80::cp8(uint8_t v) {
    // Like SUB, but X/Y come from the operand
    unsigned r = m_a - v;
    m_f = (kFlags.sz53[r & 0xFF] & ~(FX | FY)) | (v & (FX | FY)) | FN | ((r >> 8) & FC) |
          ((m_a ^ v ^ r) & FH) | (((m_a ^ v) & (m_a ^ r) & 0x80) ? FPV : 0);
}

void Z80::alu(int op, uint8_t v) {
    switch (op) {
        case 0: add8(v, 0); break;
        case 1: add8(v, m_f & FC); break;
        case 2: sub8(v, 0); break;
        case 3: sub8(v, m_f & FC); break;
        case 4: m_a &= v; m_f = kFlags.sz53p[m_a] | FH; break;
        case 5: m_a ^= v; m_f = kFlags.sz53p[m_a]; break;
        case 6: m_a |= v; m_f = kFlags.sz53p[m_a]; break;
        default: cp8(v); break;
    }
}

uint8_t Z80::inc8(uint8_t v) {
    uint8_t r = v + 1;
    m_f = (m_f & FC) | kFlags.sz53[r] | ((v & 0x0F) == 0x0F ? FH : 0) | (v == 0x7F ? FPV : 0);
    return r;
}

uint8_t Z80::dec8(uint8_t v) {
    uint8_t r = v - 1;
    m_f = (m_f & FC) | FN | kFlags.sz53[r] | ((v & 0x0F) == 0 ? FH : 0) | (v == 0x80 ? FPV : 0);
    return r;
}

uint16_t Z80::add16(uint16_t a, uint16_t b) {
    uint32_t r = a + b;
    m_f = (m_f & (FS | FZ | FPV)) | ((r >> 16) & FC) | (((a ^ b ^ r) >> 8) & FH) | ((r >> 8) & (FX | FY));
    m_wz = a + 1;
    return static_cast<uint16_t>(r);
}

void Z80::adc16(uint16_t v) {
    uint32_t r = m_hl + v + (m_f & FC);
    m_f = ((r >> 16) & FC) | (((m_hl ^ v ^ r) >> 8) & FH) |
          ((~(m_hl ^ v) & (m_hl ^ r) & 0x8000) ? FPV : 0) | ((r >> 8) & (FS | FX | FY)) |
          ((r & 0xFFFF) ? 0 : FZ);
    m_wz = m_hl + 1;
    m_hl = static_cast<uint16_t>(r);
}

void Z80::sbc16(uint16_t v) {
    uint32_t r = m_hl - v - (m_f & FC);
    m_f = FN | ((r >> 16) & FC) | (((m_hl ^ v ^ r) >> 8) & FH) |
          (((m_hl ^ v) & (m_hl ^ r) & 0x8000) ? FPV : 0) | ((r >> 8) & (FS | FX | FY)) |
          ((r & 0xFFFF) ? 0 : FZ);
    m_wz = m_hl + 1;
    m_hl = static_cast<uint16_t>(r);
}

// CB-prefixed rotates and shifts: RLC RRC RL RR SLA SRA SLL SRL
uint8_t Z80::rotate(int op, uint8_t v) {
    uint8_t r = 0;
    uint8_t carry = 0;
    switch (op) {
        case 0: carry = v >> 7; r = (v << 1) | carry; break;
        case 1: carry = v & 1; r = (v >> 1) | (carry << 7); break;
        case 2: carry = v >> 7; r = (v << 1) | (m_f & FC); break;
        case 3: carry = v & 1; r = (v >> 1) | ((m_f & FC) << 7); break;
        case 4: carry = v >> 7; r = v << 1; break;
        case 5: carry = v & 1; r = (v >> 1) | (v & 0x80); break;
        case 6: carry = v >> 7; r = (v << 1) | 1; break;
        default: carry = v & 1; r = v >> 1; break;
    }
    m_f = kFlags.sz53p[r] | carry;
    return r;
}

// BIT n: X/Y come from the register, or from MEMPTR's high byte for memory operands
void Z80::bit(int n, uint8_t v, uint8_t xy) {
    uint8_t r = v & (1 << n);
    m_f = (m_f & FC) | FH | (r ? 0 : (FZ | FPV)) | (r & FS) | (xy & (FX | FY));
}

void Z80::daa() {
    uint8_t correction = 0;
    bool carry = m_f & FC;
    if ((m_f & FH) || (m_a & 0x0F) > 9) correction |= 0x06;
    if (carry || m_a > 0x99) {
        correction |= 0x60;
        carry = true;
    }
    uint8_t r = (m_f & FN) ? m_a - correction : m_a + correction;
    m_f = kFlags.sz53p[r] | (carry ? FC : 0) | (m_f & FN) | ((m_a ^ r) & FH);
    m_a = r;
}

// ============================================================
// Block instructions
// ============================================================

void Z80::blockLd(int dir, bool repeat) {
    uint8_t v = rd(m_hl);
    wr(m_de, v);
    m_hl += dir;
    m_de += dir;
    m_bc--;
    uint8_t n = v + m_a;
    m_f = (m_f & (FS | FZ | FC)) | (m_bc ? FPV : 0) | (n & FX) | ((n & 0x02) << 4);
    m_tstates += 16;
    if (repeat && m_bc) {
        m_pc -= 2;
        m_wz = m_pc + 1;
        m_tstates += 5;
    }
}

void Z80::blockCp(int dir, bool repeat) {
    uint8_t v = rd(m_hl);
    unsigned r = m_a - v;
    m_hl += dir;
    m_bc--;
    m_wz += dir;
    m_f = (m_f & FC) | FN | (kFlags.sz53[r & 0xFF] & ~(FX | FY)) | ((m_a ^ v ^ r) & FH) | (m_bc ? FPV : 0);
    uint8_t n = static_cast<uint8_t>(r - ((m_f & FH) ? 1 : 0));
    m_f |= (n & FX) | ((n & 0x02) << 4);
    m_tstates += 16;
    if (repeat && m_bc && (r & 0xFF) != 0) {
        m_pc -= 2;
        m_wz = m_pc + 1;
        m_tstates += 5;
    }
}

void Z80::blockIn(int dir, bool repeat) {
    uint8_t v = in(m_bc);
    wr(m_hl, v);
    m_hl += dir;
    uint8_t b = (m_bc >> 8) - 1;
    m_bc = (m_bc & 0x00FF) | (b << 8);
    m_f = kFlags.sz53[b] | ((v & 0x80) ? FN : 0);
    m_tstates += 16;
    if (repeat && b) {
        m_pc -= 2;
        m_tstates += 5;
    }
}

void Z80::blockOut(int dir, bool repeat) {
    uint8_t v = rd(m_hl);
    uint8_t b = (m_bc >> 8) - 1;
    m_bc = (m_bc & 0x00FF) | (b << 8);
    out(m_bc, v);
    m_hl += dir;
    m_f = kFlags.sz53[b] | ((v & 0x80) ? FN : 0);
    m_tstates += 16;
    if (repeat && b) {
        m_pc -= 2;
        m_tstates += 5;
    }
}

// ============================================================
// Unprefixed (and DD/FD-prefixed) opcodes, decoded as x/y/z/p/q fields
// ============================================================

void Z80::execMain(uint8_t op) {
    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;
    int p = y >> 1;
    int q = y & 1;
    m_tstates += kCycles[op];

    switch (x) {
    case 0:
        switch (z) {
        case 0:
            switch (y) {
                case 0: break;  // NOP
                case 1: {       // EX AF,AF'
                    uint16_t af = (m_a << 8) | m_f;
                    std::swap(af, m_af2);
                    m_a = af >> 8;
                    m_f = af & 0xFF;
                    break;
                }
                case 2: {       // DJNZ d
                    auto d = static_cast<int8_t>(fetch());
                    uint8_t b = (m_bc >> 8) - 1;
                    m_bc = (m_bc & 0x00FF) | (b << 8);
                    if (b) {
                        m_pc += d;
                        m_wz = m_pc;
                        m_tstates += 5;
                    }
                    break;
                }
                case 3: {       // JR d
                    auto d = static_cast<int8_t>(fetch());
                    m_pc += d;
                    m_wz = m_pc;
                    break;
                }
                default: {      // JR cc,d
                    auto d = static_cast<int8_t>(fetch());
                    if (condition(y - 4)) {
                        m_pc += d;
                        m_wz = m_pc;
                        m_tstates += 5;
                    }
                    break;
                }
            }
            break;

        case 1:
            if (q == 0) {
                setRp(p, fetch16());                // LD rp,nn
            } else {
                hlx() = add16(hlx(), rp(p));         // ADD HL,rp
            }
            break;

        case 2:
            switch (y) {
                case 0: wr(m_bc, m_a); m_wz = ((m_bc + 1) & 0xFF) | (m_a << 8); break;
                case 1: m_a = rd(m_bc); m_wz = m_bc + 1; break;
                case 2: wr(m_de, m_a); m_wz = ((m_de + 1) & 0xFF) | (m_a << 8); break;
                case 3: m_a = rd(m_de); m_wz = m_de + 1; break;
                case 4: { uint16_t nn = fetch16(); wr16(nn, hlx()); m_wz = nn + 1; break; }
                case 5: { uint16_t nn = fetch16(); hlx() = rd16(nn); m_wz = nn + 1; break; }
                case 6: { uint16_t nn = fetch16(); wr(nn, m_a); m_wz = ((nn + 1) & 0xFF) | (m_a << 8); break; }
                default: { uint16_t nn = fetch16(); m_a = rd(nn); m_wz = nn + 1; break; }
            }
            break;

        case 3:  // INC/DEC rp (no flags)
            setRp(p, q == 0 ? rp(p) + 1 : rp(p) - 1);
            break;

        case 4:  // INC r
        case 5:  // DEC r
            if (y == 6) {
                uint16_t addr = memOperand();
                if (m_index) m_tstates += 8;
                uint8_t v = rd(addr);
                wr(addr, z == 4 ? inc8(v) : dec8(v));
            } else {
                setReg(y, z == 4 ? inc8(reg(y)) : dec8(reg(y)));
            }
            break;

        case 6:  // LD r,n
            if (y == 6) {
                uint16_t addr = memOperand();
                if (m_index) m_tstates += 5;
                wr(addr, fetch());
            } else {
                setReg(y, fetch());
            }
            break;

        case 7:
            switch (y) {
                case 0: {  // RLCA
                    m_a = (m_a << 1) | (m_a >> 7);
                    m_f = (m_f & (FS | FZ | FPV)) | (m_a & (FX | FY | FC));
                    break;
                }
                case 1: {  // RRCA
                    uint8_t c = m_a & 1;
                    m_a = (m_a >> 1) | (c << 7);
                    m_f = (m_f & (FS | FZ | FPV)) | c | (m_a & (FX | FY));
                    break;
                }
                case 2: {  // RLA
                    uint8_t c = m_a >> 7;
                    m_a = (m_a << 1) | (m_f & FC);
                    m_f = (m_f & (FS | FZ | FPV)) | c | (m_a & (FX | FY));
                    break;
                }
                case 3: {  // RRA
                    uint8_t c = m_a & 1;
                    m_a = (m_a >> 1) | ((m_f & FC) << 7);
                    m_f = (m_f & (FS | FZ | FPV)) | c | (m_a & (FX | FY));
                    break;
                }
                case 4: daa(); break;
                case 5:    // CPL
                    m_a = ~m_a;
                    m_f = (m_f & (FS | FZ | FPV | FC)) | FH | FN | (m_a & (FX | FY));
                    break;
                case 6:    // SCF
                    m_f = (m_f & (FS | FZ | FPV)) | FC | (m_a & (FX | FY));
                    break;
                default:   // CCF
                    m_f = (m_f & (FS | FZ | FPV)) | ((m_f & FC) ? FH : FC) | (m_a & (FX | FY));
                    break;
            }
            break;
        }
        break;

    case 1:
        if (y == 6 && z == 6) {
            m_halted = true;                     // HALT
        } else if (y == 6) {                     // LD (HL),r
            uint16_t addr = memOperand();
            if (m_index) m_tstates += 8;
            wr(addr, m_index ? regPlain(z) : reg(z));
        } else if (z == 6) {                     // LD r,(HL)
            uint16_t addr = memOperand();
            if (m_index) m_tstates += 8;
            uint8_t v = rd(addr);
            if (m_index) {
                setRegPlain(y, v);
            } else {
                setReg(y, v);
            }
        } else {
            setReg(y, reg(z));                   // LD r,r'
        }
        break;

    case 2: {                                    // ALU A,r
        uint8_t v;
        if (z == 6) {
            uint16_t addr = memOperand();
            if (m_index) m_tstates += 8;
            v = rd(addr);
        } else {
            v = reg(z);
        }
        alu(y, v);
        break;
    }

    case 3:
        switch (z) {
        case 0:  // RET cc
            if (condition(y)) {
                m_pc = pop();
                m_wz = m_pc;
                m_tstates += 6;
            }
            break;

        case 1:
            if (q == 0) {
                setRp2(p, pop());                // POP rp2
            } else {
                switch (p) {
                    case 0: m_pc = pop(); m_wz = m_pc; break;   // RET
                    case 1:                                     // EXX
                        std::swap(m_bc, m_bc2);
                        std::swap(m_de, m_de2);
                        std::swap(m_hl, m_hl2);
                        break;
                    case 2: m_pc = hlx(); break;                // JP (HL)
                    default: m_sp = hlx(); break;               // LD SP,HL
                }
            }
            break;

        case 2: {  // JP cc,nn
            uint16_t nn = fetch16();
            m_wz = nn;
            if (condition(y)) m_pc = nn;
            break;
        }

        case 3:
            switch (y) {
                case 0: m_pc = fetch16(); m_wz = m_pc; break;   // JP nn
                case 2: {                                       // OUT (n),A
                    uint8_t n = fetch();
                    out((m_a << 8) | n, m_a);
                    m_wz = ((n + 1) & 0xFF) | (m_a << 8);
                    break;
                }
                case 3: {                                       // IN A,(n)
                    uint8_t n = fetch();
                    uint16_t port = (m_a << 8) | n;
                    m_a = in(port);
                    m_wz = port + 1;
                    break;
                }
                case 4: {                                       // EX (SP),HL
                    uint16_t v = rd16(m_sp);
                    wr16(m_sp, hlx());
                    hlx() = v;
                    m_wz = v;
                    break;
                }
                case 5: std::swap(m_de, m_hl); break;           // EX DE,HL (never IX/IY)
                case 6: m_iff1 = m_iff2 = false; break;         // DI
                case 7: m_iff1 = m_iff2 = true; break;          // EI
                default: break;                                 // CB prefix, handled earlier
            }
            break;

        case 4: {  // CALL cc,nn
            uint16_t nn = fetch16();
            m_wz = nn;
            if (condition(y)) {
                push(m_pc);
                m_pc = nn;
                m_tstates += 7;
            }
            break;
        }

        case 5:
            if (q == 0) {
                push(rp2(p));                    // PUSH rp2
            } else if (p == 0) {                 // CALL nn
                uint16_t nn = fetch16();
                m_wz = nn;
                push(m_pc);
                m_pc = nn;
            }
            break;  // DD/ED/FD prefixes are handled in step()

        case 6:
            alu(y, fetch());                     // ALU A,n
            break;

        default:  // RST
            push(m_pc);
            m_pc = y * 8;
            m_wz = m_pc;
            break;
        }
        break;
    }
}

// ============================================================
// CB prefix
// ============================================================

void Z80::execCB() {
    uint8_t op = fetchOpcode();
    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;

    if (z == 6) {
        uint8_t v = rd(m_hl);
        if (x == 1) {
            bit(y, v, m_wz >> 8);
            m_tstates += 12;
            return;
        }
        uint8_t r = x == 0 ? rotate(y, v) : (x == 2 ? (v & ~(1 << y)) : (v | (1 << y)));
        wr(m_hl, r);
        m_tstates += 15;
        return;
    }

    uint8_t v = regPlain(z);
    m_tstates += 8;
    switch (x) {
        case 0: setRegPlain(z, rotate(y, v)); break;
        case 1: bit(y, v, v); break;
        case 2: setRegPlain(z, v & ~(1 << y)); break;
        default: setRegPlain(z, v | (1 << y)); break;
    }
}

// DDCB d op / FDCB d op: always operates on (IX+d); non-BIT results are also
// copied to register z (undocumented) unless z is 6
void Z80::execIndexedCB() {
    auto d = static_cast<int8_t>(fetch());
    uint8_t op = fetch();
    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;

    uint16_t addr = hlx() + d;
    m_wz = addr;
    uint8_t v = rd(addr);

    if (x == 1) {
        bit(y, v, addr >> 8);
        m_tstates += 16;
        return;
    }

    uint8_t r = x == 0 ? rotate(y, v) : (x == 2 ? (v & ~(1 << y)) : (v | (1 << y)));
    wr(addr, r);
    if (z != 6) setRegPlain(z, r);
    m_tstates += 19;
}

// ============================================================
// ED prefix
// ============================================================

void Z80::execED() {
    uint8_t op = fetchOpcode();
    int x = op >> 6;
    int y = (op >> 3) & 7;
    int z = op & 7;
    int p = y >> 1;
    int q = y & 1;

    if (x == 2 && z <= 3 && y >= 4) {
        int dir = (y & 1) ? -1 : 1;
        bool repeat = y >= 6;
        switch (z) {
            case 0: blockLd(dir, repeat); break;
            case 1: blockCp(dir, repeat); break;
            case 2: blockIn(dir, repeat); break;
            default: blockOut(dir, repeat); break;
        }
        return;
    }

    if (x != 1) {
        m_tstates += 8;  // undefined ED opcodes act as two NOPs
        return;
    }

    switch (z) {
        case 0: {  // IN r,(C) (r=6: flags only)
            uint8_t v = in(m_bc);
            if (y != 6) setRegPlain(y, v);
            m_f = (m_f & FC) | kFlags.sz53p[v];
            m_wz = m_bc + 1;
            m_tstates += 12;
            break;
        }
        case 1:    // OUT (C),r (r=6: outputs 0)
            out(m_bc, y == 6 ? 0 : regPlain(y));
            m_wz = m_bc + 1;
            m_tstates += 12;
            break;
        case 2:    // SBC/ADC HL,rp
            if (q == 0) {
                sbc16(rp(p));
            } else {
                adc16(rp(p));
            }
            m_tstates += 15;
            break;
        case 3: {  // LD (nn),rp / LD rp,(nn)
            uint16_t nn = fetch16();
            if (q == 0) {
                wr16(nn, rp(p));
            } else {
                setRp(p, rd16(nn));
            }
            m_wz = nn + 1;
            m_tstates += 20;
            break;
        }
        case 4: {  // NEG
            uint8_t v = m_a;
            m_a = 0;
            sub8(v, 0);
            m_tstates += 8;
            break;
        }
        case 5:    // RETN / RETI
            m_pc = pop();
            m_wz = m_pc;
            m_iff1 = m_iff2;
            m_tstates += 14;
            break;
        case 6: {  // IM 0/1/2
            static constexpr int kModes[8] = {0, 0, 1, 2, 0, 0, 1, 2};
            m_im = kModes[y];
            m_tstates += 8;
            break;
        }
        default:
            switch (y) {
                case 0: m_i = m_a; m_tstates += 9; break;   // LD I,A
                case 1: m_r = m_a; m_tstates += 9; break;   // LD R,A
                case 2:                                      // LD A,I
                case 3:                                      // LD A,R
                    m_a = y == 2 ? m_i : m_r;
                    m_f = (m_f & FC) | kFlags.sz53[m_a] | (m_iff2 ? FPV : 0);
                    m_tstates += 9;
                    break;
                case 4: {                                    // RRD
                    uint8_t m = rd(m_hl);
                    wr(m_hl, (m_a << 4) | (m >> 4));
                    m_a = (m_a & 0xF0) | (m & 0x0F);
                    m_f = (m_f & FC) | kFlags.sz53p[m_a];
                    m_wz = m_hl + 1;
                    m_tstates += 18;
                    break;
                }
                case 5: {                                    // RLD
                    uint8_t m = rd(m_hl);
                    wr(m_hl, (m << 4) | (m_a & 0x0F));
                    m_a = (m_a & 0xF0) | (m >> 4);
                    m_f = (m_f & FC) | kFlags.sz53p[m_a];
                    m_wz = m_hl + 1;
                    m_tstates += 18;
                    break;
                }
                default: m_tstates += 8; break;              // NOPs
            }
            break;
    }
}

// ============================================================
// Save states
// ============================================================

void Z80::saveState(StateWriter& w) const {
    w.put(m_a); w.put(m_f); w.put(m_bc); w.put(m_de); w.put(m_hl);
    w.put(m_af2); w.put(m_bc2); w.put(m_de2); w.put(m_hl2);
    w.put(m_ix); w.put(m_iy); w.put(m_sp); w.put(m_pc); w.put(m_wz);
    w.put(m_i); w.put(m_r); w.put(m_iff1); w.put(m_iff2); w.put(m_im); w.put(m_halted);
}

void Z80::loadState(StateReader& r) {
    r.get(m_a); r.get(m_f); r.get(m_bc); r.get(m_de); r.get(m_hl);
    r.get(m_af2); r.get(m_bc2); r.get(m_de2); r.get(m_hl2);
    r.get(m_ix); r.get(m_iy); r.get(m_sp); r.get(m_pc); r.get(m_wz);
    r.get(m_i); r.get(m_r); r.get(m_iff1); r.get(m_iff2); r.get(m_im); r.get(m_halted);
}

} // namespace apple2e
