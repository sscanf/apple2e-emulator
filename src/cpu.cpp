#include "cpu.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iomanip>

namespace apple2e {

CPU::CPU() : m_a(0), m_x(0), m_y(0), m_pc(0), m_sp(0xFF), m_flags(static_cast<uint8_t>(Flag::BREAK2)) {
    // 65C02: B flag is always 1 on reset
}

void CPU::setReadCallback(MemoryCallback callback) {
    m_readCallback = callback;
}

void CPU::setWriteCallback(MemoryWriteCallback callback) {
    m_writeCallback = callback;
}

void CPU::setCycleCallback(CycleCallback callback) {
    m_cycleCallback = callback;
}

void CPU::reset() {
    m_a = 0;
    m_x = 0;
    m_y = 0;
    m_sp = 0xFD;  // Stack pointer starts at $01FD on Apple II
    m_flags = static_cast<uint8_t>(Flag::BREAK2);  // Only B flag set

    // Read reset vector from $FFFC-$FFFD
    m_pc = read(0xFFFD) | (read(0xFFFC) << 8);
}

void CPU::irq() {
    m_nmiPending = false;  // Clear any pending NMI
    // IRQ is only recognized if I flag is clear
    if (!getFlag(Flag::INTERRUPT)) {
        // Push PC (high byte first)
        push((m_pc >> 8) & 0xFF);
        push(m_pc & 0xFF);
        // Push status with B flag cleared
        push(m_flags & ~static_cast<uint8_t>(Flag::BREAK));
        // Set interrupt flag
        setFlag(Flag::INTERRUPT, true);
        // Read IRQ vector from $FFFE-$FFFF
        uint16_t irqVec = read(0xFFFF) | (read(0xFFFE) << 8);
        m_pc = irqVec;
    }
}

void CPU::nmi() {
    m_nmiPending = true;
    // Push PC
    push((m_pc >> 8) & 0xFF);
    push(m_pc & 0xFF);
    // Push status
    push(m_flags & ~static_cast<uint8_t>(Flag::BREAK));
    // Read NMI vector from $FFFA-$FFFB
    uint16_t nmiVec = read(0xFFFB) | (read(0xFFFA) << 8);
    m_pc = nmiVec;
}

void CPU::brake() {
    // BRK instruction: push PC+2, set B flag
    push((m_pc >> 8) & 0xFF);
    push(m_pc & 0xFF);
    push(m_flags | static_cast<uint8_t>(Flag::BREAK) | static_cast<uint8_t>(Flag::BREAK2));
    // Read vector from $FFFE-$FFFF
    uint16_t vec = read(0xFFFF) | (read(0xFFFE) << 8);
    m_pc = vec;
}

uint32_t CPU::step() {
    if (m_halted) return 0;

    // Check for NMI
    if (m_nmiPending) {
        m_nmiPending = false;
        // Push PC
        push((m_pc >> 8) & 0xFF);
        push(m_pc & 0xFF);
        push(m_flags & ~static_cast<uint8_t>(Flag::BREAK));
        uint16_t nmiVec = read(0xFFFB) | (read(0xFFFA) << 8);
        m_pc = nmiVec;
        if (m_cycleCallback) m_cycleCallback(7);
        return 7;
    }

    // Fetch opcode
    uint8_t opcode = read(m_pc++);

    // Get instruction info
    const Instruction& instr = kInstructions[opcode];

    // Execute instruction
    if (instr.fn) {
        (this->*instr.fn)(instr.mode);
    } else {
        invalid_opcode(AddrMode::IMMEDIATE);
    }

    uint32_t cycles = instr.cycles;

    // Extra cycle for page crossing on some instructions
    // (Already included in the cycles table for simplicity)

    if (m_cycleCallback) m_cycleCallback(cycles);
    return cycles;
}

uint16_t CPU::fetchAddr(AddrMode mode) {
    switch (mode) {
        case AddrMode::IMMEDIATE:
            return m_pc - 1;  // Address of the immediate value

        case AddrMode::ZP: {
            uint8_t addr = read(m_pc++);
            return addr;  // Zero page address
        }

        case AddrMode::ZPX: {
            uint8_t addr = read(m_pc++);
            return (addr + m_x) & 0xFF;  // Zero page + X
        }

        case AddrMode::ZPY: {
            uint8_t addr = read(m_pc++);
            return (addr + m_y) & 0xFF;
        }

        case AddrMode::ABS: {
            uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
            return addr;
        }

        case AddrMode::ABX: {
            uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
            return addr + m_x;
        }

        case AddrMode::ABY: {
            uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
            return addr + m_y;
        }

        case AddrMode::IND: {
            uint16_t ptr = read(m_pc++) | (read(m_pc++) << 8);
            // 6502 bug: page wrap on indirect
            uint16_t addr = read(ptr & 0xFF00 | (ptr + 1) & 0x00FF);
            addr |= read(ptr & 0xFF00 | (ptr + 1) & 0x00FF) << 8;
            // Fix: read from (ptr) & 0x00FF, (ptr+1) & 0x00FF
            uint8_t lo = read(ptr & 0xFF00 | (ptr + 1) & 0x00FF);
            uint8_t hi = read(ptr & 0xFF00 | ((ptr + 1) & 0xFF));
            return lo | (hi << 8);
        }

        case AddrMode::IDX: {
            uint8_t ptr = read(m_pc++) & 0xFF;
            uint8_t lo = read((ptr + m_x) & 0xFF);
            uint8_t hi = read((ptr + m_x + 1) & 0xFF);
            return lo | (hi << 8);
        }

        case AddrMode::IDY: {
            uint8_t ptr = read(m_pc++) & 0xFF;
            uint8_t lo = read(ptr);
            uint8_t hi = read((ptr + 1) & 0xFF);
            uint16_t addr = lo | (hi << 8);
            return addr + m_y;
        }

        case AddrMode::REL: {
            int8_t rel = static_cast<int8_t>(read(m_pc++));
            return m_pc + rel;
        }

        case AddrMode::ABSIND: {
            uint16_t ptr = read(m_pc++) | (read(m_pc++) << 8);
            uint8_t lo = read(ptr & 0xFF00 | ((ptr + 1) & 0x00FF));
            uint8_t hi = read(ptr & 0xFF00 | ((ptr + 1) & 0x00FF));
            return lo | (hi << 8);
        }

        default:
            return 0;
    }
}

uint8_t CPU::read(uint16_t addr) const {
    if (m_readCallback) return m_readCallback(addr);
    return 0;
}

void CPU::write(uint16_t addr, uint8_t val) const {
    if (m_writeCallback) m_writeCallback(addr, val);
}

uint8_t CPU::readStack(uint16_t offset) {
    uint16_t addr = 0x0100 + m_sp + offset;
    return read(addr & 0xFFFF);
}

void CPU::writeStack(uint16_t offset, uint8_t val) {
    uint16_t addr = 0x0100 + m_sp + offset;
    write(addr & 0xFFFF, val);
}

void CPU::push(uint8_t val) {
    writeStack(0, val);
    m_sp--;
}

uint8_t CPU::pop() {
    m_sp++;
    return readStack(0);
}

void CPU::setFlag(Flag f, bool v) {
    uint8_t flag = static_cast<uint8_t>(f);
    if (v) {
        m_flags |= flag;
    } else {
        m_flags &= ~flag;
    }
}

void CPU::adc(uint8_t val) {
    uint16_t sum = m_a + val + getFlag(Flag::CARRY);
    setCarry(sum > 0xFF);
    setOverflow(((m_a ^ val) & 0x80) && ((m_a ^ static_cast<uint8_t>(sum)) & 0x80));
    m_a = sum & 0xFF;
    setZero(m_a == 0);
    setNegative(m_a & 0x80);
}

void CPU::sbc(uint8_t val) {
    uint16_t diff = m_a - val - (!getFlag(Flag::CARRY));
    setOverflow(((m_a ^ val) & 0x80) && ((m_a ^ static_cast<uint8_t>(diff & 0xFF)) & 0x80));
    m_a = diff & 0xFF;
    setCarry(diff <= 0xFF);
    setZero(m_a == 0);
    setNegative(m_a & 0x80);
}



void CPU::cpx_impl(uint8_t val) {
    uint16_t diff = static_cast<uint16_t>(m_x) - val;
    setCarry(diff <= 0xFF);
    setZero(diff & 0xFF);
    setNegative(diff & 0x80);
}

void CPU::cpy_impl(uint8_t val) {
    uint16_t diff = static_cast<uint16_t>(m_y) - val;
    setCarry(diff <= 0xFF);
    setZero(diff & 0xFF);
    setNegative(diff & 0x80);
}

void CPU::cpx(AddrMode mode) {
    switch (mode) {
        case AddrMode::IMMEDIATE: cpx_imm(); break;
        case AddrMode::ZP:        cpx_zp(); break;
        case AddrMode::ABS:       cpx_abs(); break;
        case AddrMode::ZPY:       cpx_zpy(); break;
        default: break;
    }
}

void CPU::cpy(AddrMode mode) {
    switch (mode) {
        case AddrMode::IMMEDIATE: cpy_imm(); break;
        case AddrMode::ZP:        cpy_zp(); break;
        case AddrMode::ABS:       cpy_abs(); break;
        case AddrMode::ZPX:       cpy_abx(); break;
        default: break;
    }
}

void CPU::bit(AddrMode mode) {
    switch (mode) {
        case AddrMode::ZP:  bit_zp(); break;
        case AddrMode::ABS: bit_abs(); break;
        default: break;
    }
}

void CPU::rol_val(uint8_t& val) {
    uint8_t carry = getFlag(Flag::CARRY);
    val = (val << 1) | carry;
    setCarry(val & 0x80);
    val &= 0xFF;
    setZero(val);
    setNegative(val & 0x80);
}

void CPU::ror_val(uint8_t& val) {
    uint8_t carry = getFlag(Flag::CARRY);
    uint8_t lsb = val & 0x01;
    val = (val >> 1) | (carry << 7);
    setCarry(lsb);
    setZero(val);
    setNegative(val & 0x80);
}

void CPU::asl_val(uint8_t& val) {
    setCarry(val & 0x80);
    val <<= 1;
    val &= 0xFF;
    setZero(val);
    setNegative(val & 0x80);
}

void CPU::lsr_val(uint8_t& val) {
    setCarry(val & 0x01);
    val >>= 1;
    setZero(val);
    setNegative(false);
}

void CPU::branch(bool condition, uint16_t rel) {
    if (condition) {
        // Check if branch crosses page boundary (extra cycle)
        uint16_t oldPage = m_pc & 0xFF00;
        uint16_t newPage = rel & 0xFF00;
        if (oldPage != newPage) {
            // Page crossed - extra cycle
            if (m_cycleCallback) m_cycleCallback(2);
        }
        if (m_cycleCallback) m_cycleCallback(1);
        m_pc = rel;
    } else {
        if (m_cycleCallback) m_cycleCallback(1);
    }
}

// ========== Load Instructions ==========

void CPU::lda(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr);
    m_a = val;
    setZero(val);
    setNegative(val & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::ldx(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr);
    m_x = val;
    setZero(val);
    setNegative(val & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::ldy(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr);
    m_y = val;
    setZero(val);
    setNegative(val & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== Store Instructions ==========

void CPU::sta(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    write(addr, m_a);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::stx(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    write(addr, m_x);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::sty(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    write(addr, m_y);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== Transfer Instructions ==========

void CPU::tax(AddrMode mode) {
    m_x = m_a;
    setZero(m_x);
    setNegative(m_x & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::tay(AddrMode mode) {
    m_y = m_a;
    setZero(m_y);
    setNegative(m_y & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::tsx(AddrMode mode) {
    m_x = m_sp;
    setZero(m_x);
    setNegative(m_x & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::txa(AddrMode mode) {
    m_a = m_x;
    setZero(m_a);
    setNegative(m_a & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::tya(AddrMode mode) {
    m_a = m_y;
    setZero(m_a);
    setNegative(m_a & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::txs(AddrMode mode) {
    m_sp = m_x;
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== Stack Instructions ==========

void CPU::pha(AddrMode mode) {
    push(m_a);
    if (m_cycleCallback) m_cycleCallback(2);
}

void CPU::pla(AddrMode mode) {
    m_a = pop();
    setZero(m_a);
    setNegative(m_a & 0x80);
    if (m_cycleCallback) m_cycleCallback(2);
}

void CPU::php(AddrMode mode) {
    push(m_flags | static_cast<uint8_t>(Flag::BREAK) | static_cast<uint8_t>(Flag::BREAK2));
    if (m_cycleCallback) m_cycleCallback(2);
}

void CPU::plp(AddrMode mode) {
    m_flags = pop();
    m_flags |= static_cast<uint8_t>(Flag::BREAK2);  // B2 always 1
    if (m_cycleCallback) m_cycleCallback(2);
}

// ========== AND, ORA, EOR Instructions ==========

void CPU::and_(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr);
    m_a &= val;
    setZero(m_a);
    setNegative(m_a & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::ora(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr);
    m_a |= val;
    setZero(m_a);
    setNegative(m_a & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::eor(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr);
    m_a ^= val;
    setZero(m_a);
    setNegative(m_a & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== INC, DEC Instructions ==========

void CPU::inc(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr) + 1;
    write(addr, val);
    setZero(val);
    setNegative(val & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::dec(AddrMode mode) {
    uint16_t addr = fetchAddr(mode);
    uint8_t val = read(addr) - 1;
    write(addr, val);
    setZero(val);
    setNegative(val & 0x80);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::incw(uint16_t addr) {
    uint8_t val = read(addr) + 1;
    write(addr, val);
    setZero(val);
    setNegative(val & 0x80);
}

void CPU::decw(uint16_t addr) {
    uint8_t val = read(addr) - 1;
    write(addr, val);
    setZero(val);
    setNegative(val & 0x80);
}

// ========== ADC/SBC ==========

void CPU::adc_imm() {
    uint8_t val = read(m_pc++);
    adc(val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::sbc_imm() {
    uint8_t val = read(m_pc++);
    sbc(val);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== CMP ==========

void CPU::cmp_impl(uint8_t a, uint8_t b) {
    uint16_t diff = static_cast<uint16_t>(a) - b;
    setCarry(diff <= 0xFF);
    setZero(diff & 0xFF);
    setNegative(diff & 0x80);
}

void CPU::cmp_imm() {
    uint8_t val = read(m_pc++);
    cmp_impl(m_a, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cmp_zp() {
    uint8_t addr = read(m_pc++);
    cmp_impl(m_a, read(addr));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cmp_zpx() {
    uint8_t addr = read(m_pc++);
    cmp_impl(m_a, read((addr + m_x) & 0xFF));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cmp_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    cmp_impl(m_a, read(addr));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cmp_abx() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    cmp_impl(m_a, read(addr + m_x));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cmp_aby() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    cmp_impl(m_a, read(addr + m_y));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cmp(AddrMode mode) {
    switch (mode) {
        case AddrMode::IMMEDIATE: cmp_imm(); break;
        case AddrMode::ZP:        cmp_zp(); break;
        case AddrMode::ZPX:       cmp_zpx(); break;
        case AddrMode::ABS:       cmp_abs(); break;
        case AddrMode::ABX:       cmp_abx(); break;
        case AddrMode::ABY:       cmp_aby(); break;
        default: break;
    }
}

void CPU::cpx_imm() {
    uint8_t val = read(m_pc++);
    cpx_impl(val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cpx_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    cpx_impl(read(addr));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cpx_zp() {
    uint8_t addr = read(m_pc++);
    cpx_impl(read(addr));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cpx_zpy() {
    uint8_t addr = read(m_pc++);
    cpx_impl(read((addr + m_y) & 0xFF));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cpy_imm() {
    uint8_t val = read(m_pc++);
    cpy_impl(val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cpy_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    cpy_impl(read(addr));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cpy_zp() {
    uint8_t addr = read(m_pc++);
    cpy_impl(read(addr));
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cpy_abx() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    cpy_impl(read(addr + m_x));
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== BIT ==========

void CPU::bit_zp() {
    uint8_t val = read(read(m_pc++));
    setZero(val & m_a);
    setFlag(Flag::NEGATIVE, val & 0x80);
    setOverflow(val & 0x40);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::bit_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr);
    setZero(val & m_a);
    setFlag(Flag::NEGATIVE, val & 0x80);
    setOverflow(val & 0x40);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== ASL ==========

void CPU::asl_acc() {
    asl_val(m_a);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::asl_zp() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read(addr);
    asl_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::asl_zpx() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read((addr + m_x) & 0xFF);
    asl_val(val);
    write((addr + m_x) & 0xFF, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::asl_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr);
    asl_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::asl_abx() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr + m_x);
    asl_val(val);
    write(addr + m_x, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== LSR ==========

void CPU::lsr_acc() {
    lsr_val(m_a);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::lsr_zp() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read(addr);
    lsr_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::lsr_zpx() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read((addr + m_x) & 0xFF);
    lsr_val(val);
    write((addr + m_x) & 0xFF, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::lsr_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr);
    lsr_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::lsr_abx() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr + m_x);
    lsr_val(val);
    write(addr + m_x, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== ROL ==========

void CPU::rol_acc() {
    rol_val(m_a);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::rol_zp() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read(addr);
    rol_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::rol_zpx() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read((addr + m_x) & 0xFF);
    rol_val(val);
    write((addr + m_x) & 0xFF, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::rol_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr);
    rol_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::rol_abx() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr + m_x);
    rol_val(val);
    write(addr + m_x, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== ROR ==========

void CPU::ror_acc() {
    ror_val(m_a);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::ror_zp() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read(addr);
    ror_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::ror_zpx() {
    uint8_t addr = read(m_pc++);
    uint8_t val = read((addr + m_x) & 0xFF);
    ror_val(val);
    write((addr + m_x) & 0xFF, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::ror_abs() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr);
    ror_val(val);
    write(addr, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::ror_abx() {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    uint8_t val = read(addr + m_x);
    ror_val(val);
    write(addr + m_x, val);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== Shift/Rotate Dispatchers ==========

void CPU::asl(AddrMode mode) {
    switch (mode) {
        case AddrMode::ZP:  asl_zp(); break;
        case AddrMode::ABS: asl_abs(); break;
        default: break;
    }
}

void CPU::lsr(AddrMode mode) {
    switch (mode) {
        case AddrMode::ZP:  lsr_zp(); break;
        case AddrMode::ABS: lsr_abs(); break;
        case AddrMode::ABX: lsr_abx(); break;
        default: break;
    }
}

void CPU::rol(AddrMode mode) {
    switch (mode) {
        case AddrMode::ZP:  rol_zp(); break;
        case AddrMode::ABS: rol_abs(); break;
        case AddrMode::ZPX: rol_zpx(); break;
        case AddrMode::ABX: rol_abx(); break;
        default: break;
    }
}

void CPU::ror(AddrMode mode) {
    switch (mode) {
        case AddrMode::ABX: ror_abx(); break;
        default: break;
    }
}

// ========== JMP ==========

void CPU::jmp_abs(AddrMode mode) {
    m_pc = read(m_pc++) | (read(m_pc++) << 8);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::jmp_ind(AddrMode mode) {
    uint16_t ptr = read(m_pc++) | (read(m_pc++) << 8);
    // 6502 page wrap bug
    uint8_t lo = read(ptr & 0xFF00 | ((ptr + 1) & 0x00FF));
    uint8_t hi = read(ptr & 0xFF00 | ((ptr + 1) & 0x00FF));
    m_pc = lo | (hi << 8);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== JSR ==========

void CPU::jsr(AddrMode mode) {
    uint16_t addr = read(m_pc++) | (read(m_pc++) << 8);
    push((addr >> 8) & 0xFF);
    push(addr & 0xFF);
    m_pc = addr;
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== RTS ==========

void CPU::rts(AddrMode mode) {
    uint16_t addr = pop();
    addr |= (pop() << 8);
    m_pc = addr + 1;
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== BRK ==========

void CPU::brk_impl(AddrMode mode) {
    // Push PC+2
    push((m_pc >> 8) & 0xFF);
    push(m_pc & 0xFF);
    // Push status with B flag set
    push(m_flags | static_cast<uint8_t>(Flag::BREAK) | static_cast<uint8_t>(Flag::BREAK2));
    // Read vector from $FFFE-$FFFF
    uint16_t vec = read(0xFFFF) | (read(0xFFFE) << 8);
    m_pc = vec;
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::irq_impl(AddrMode mode) {
    irq();
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::nmi_impl(AddrMode mode) {
    nmi();
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== Branch Instructions ==========

void CPU::bcc(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(!getFlag(Flag::CARRY), rel);
}

void CPU::bcs(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(getFlag(Flag::CARRY), rel);
}

void CPU::beq(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(getFlag(Flag::ZERO), rel);
}

void CPU::bne(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(!getFlag(Flag::ZERO), rel);
}

void CPU::bvc(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(!getFlag(Flag::V), rel);
}

void CPU::bvs(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(getFlag(Flag::V), rel);
}

void CPU::bpl(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(!getFlag(Flag::NEGATIVE), rel);
}

void CPU::bmi(AddrMode mode) {
    uint16_t rel = fetchAddr(AddrMode::REL);
    branch(getFlag(Flag::NEGATIVE), rel);
}

void CPU::cmp_branch(uint8_t a, uint8_t b, void (CPU::*branchFn)()) {
    cmp_impl(a, b);
    (this->*branchFn)();
}

// ========== Flag Instructions ==========

void CPU::clc(AddrMode mode) {
    setCarry(false);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cld(AddrMode mode) {
    setFlag(Flag::DECIMAL, false);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::cli(AddrMode mode) {
    setFlag(Flag::INTERRUPT, false);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::clv(AddrMode mode) {
    setOverflow(false);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::sec(AddrMode mode) {
    setCarry(true);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::sed(AddrMode mode) {
    setFlag(Flag::DECIMAL, true);
    if (m_cycleCallback) m_cycleCallback(1);
}

void CPU::sei(AddrMode mode) {
    setFlag(Flag::INTERRUPT, true);
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== Invalid Opcode ==========

void CPU::invalid_opcode(AddrMode mode) {
    // On 65C02, invalid opcodes are typically NOPs
    if (m_cycleCallback) m_cycleCallback(1);
}

// ========== Fetch ==========

uint8_t CPU::fetchByte() {
    return read(m_pc++);
}

uint16_t CPU::fetchWord() {
    uint8_t lo = read(m_pc++);
    uint8_t hi = read(m_pc++);
    return lo | (hi << 8);
}

// ========== Disassembly ==========

const std::string CPU::disassemble(uint16_t addr) const {
    uint8_t opcode = read(addr);
    char buf[64];

    const char* name = kOpcodeNames[opcode];

    // Get byte count
    uint8_t bytes = 0;
    // Look up instruction size
    switch (opcode) {
        case 0x00: case 0x08: case 0x10: case 0x18: case 0x20:
        case 0x28: case 0x30: case 0x38: case 0x40: case 0x48:
        case 0x58: case 0x60: case 0x68: case 0x78: case 0x88:
        case 0x90: case 0x98: case 0xA8: case 0xB8: case 0xC8:
        case 0xD0: case 0xD8: case 0xE8: case 0xF0: case 0xF8:
            bytes = 1; break;
        case 0x01: case 0x11: case 0x21: case 0x31: case 0x41:
        case 0x51: case 0x61: case 0x71: case 0x81: case 0x91:
        case 0xA1: case 0xB1: case 0xC1: case 0xD1: case 0xE1:
        case 0xF1:
            bytes = 2; break;
        case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24:
        case 0x2C: case 0x34: case 0x3C: case 0x44: case 0x4C:
        case 0x54: case 0x5C: case 0x64: case 0x6C: case 0x74:
        case 0x7C: case 0x84: case 0x8C: case 0x94: case 0x9C:
        case 0xA4: case 0xAC: case 0xB4: case 0xBC: case 0xC4:
        case 0xCC: case 0xD4: case 0xDC: case 0xE4: case 0xEC:
        case 0xF4: case 0xFC:
            bytes = 2; break;
        case 0x02: case 0x0A: case 0x0B: case 0x0F: case 0x12:
        case 0x1A: case 0x1B: case 0x1F: case 0x22: case 0x2A:
        case 0x2B: case 0x2F: case 0x32: case 0x3A: case 0x3B:
        case 0x3F: case 0x42: case 0x4A: case 0x4B: case 0x4F:
        case 0x52: case 0x5A: case 0x5B: case 0x5F: case 0x62:
        case 0x6A: case 0x6B: case 0x6F: case 0x72: case 0x7A:
        case 0x7B: case 0x7F: case 0x80: case 0x82: case 0x83:
        case 0x85: case 0x86: case 0x87: case 0x89: case 0x8A:
        case 0x8B: case 0x8D: case 0x8E: case 0x8F: case 0x92:
        case 0x93: case 0x95: case 0x96: case 0x97: case 0x9A:
        case 0x9B: case 0x9D: case 0x9E: case 0x9F: case 0xA0:
        case 0xA2: case 0xA3: case 0xA5: case 0xA6: case 0xA7:
        case 0xA9: case 0xAA: case 0xAB: case 0xAD: case 0xAE:
        case 0xAF: case 0xB2: case 0xB3: case 0xB5: case 0xB6:
        case 0xB7: case 0xBA: case 0xBB: case 0xBD: case 0xBE:
        case 0xBF: case 0xC0: case 0xC2: case 0xC3: case 0xC5:
        case 0xC6: case 0xC7: case 0xC9: case 0xCA: case 0xCB:
        case 0xCD: case 0xCE: case 0xCF: case 0xD2: case 0xD3:
        case 0xD9: case 0xDA: case 0xDB: case 0xE2: case 0xE3:
        case 0xE5: case 0xE6: case 0xE7: case 0xE9: case 0xEA:
        case 0xEB: case 0xED: case 0xEE: case 0xEF: case 0xF2:
        case 0xF3: case 0xF5: case 0xF6: case 0xF7: case 0xF9:
        case 0xFA: case 0xFB: case 0xFD: case 0xFE: case 0xFF:
            bytes = 2; break;
        default:
            bytes = 1; break;
    }

    snprintf(buf, sizeof(buf), "%04X: ", addr);
    for (uint8_t i = 0; i < bytes && i < 3; i++) {
        uint8_t b = read(addr + i);
        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%02X ", b);
    }
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", name);

    return std::string(buf);
}

} // namespace apple2e
