#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <functional>

// 65C02 CPU for Apple IIe
// 65C02 is a CMOS version of the 6502 with additional instructions

namespace apple2e {

// CPU flags (P register)
enum class Flag : uint8_t {
    CARRY     = 0x01,
    ZERO      = 0x02,
    INTERRUPT = 0x04,
    DECIMAL   = 0x08,
    BREAK     = 0x10,
    BREAK2    = 0x20,  // Always 1
    V         = 0x40,
    NEGATIVE  = 0x80,
};

// 65C02 CPU core
class CPU {
public:
    using MemoryCallback = std::function<uint8_t(uint16_t addr)>;
    using MemoryWriteCallback = std::function<void(uint16_t addr, uint8_t val)>;
    using CycleCallback = std::function<void(uint32_t cycles)>;

    CPU();

    // Set memory callbacks
    void setReadCallback(MemoryCallback callback);
    void setWriteCallback(MemoryWriteCallback callback);
    void setCycleCallback(CycleCallback callback);

    // Reset and interrupt
    void reset();
    void irq();          // Maskable IRQ
    void nmi();          // Non-maskable interrupt
    void brake();        // BRK instruction with B flag

    // Execute one instruction (returns cycles used)
    uint32_t step();

    // Status
    bool halted() const { return m_halted; }
    void setHalted(bool h) { m_halted = h; }

    // Debug
    uint16_t pc() const { return m_pc; }
    const std::string disassemble(uint16_t addr) const;

private:
    // Addressing modes
    enum class AddrMode {
        IMMEDIATE,  // #value
        ZP,         // addr (zero page)
        ZPX,        // addr,X
        ZPY,        // addr,Y
        ABS,        // addr
        ABX,        // addr,X
        ABY,        // addr,Y
        IND,        // (addr)
        IDX,        // (addr,X)
        IDY,        // (addr),Y
        REL,        // rel (branch)
        ABSIND,     // (addr) - indirect
    };

    // Fetch address using addressing mode
    uint16_t fetchAddr(AddrMode mode);

    // Read/write with callback
    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t val) const;

    // Stack operations
    uint8_t readStack(uint16_t offset);
    void writeStack(uint16_t offset, uint8_t val);

    // Push/pop
    void push(uint8_t val);
    uint8_t pop();

    // Flag operations
    void setCarry(bool v) { setFlag(Flag::CARRY, v); }
    void setZero(bool v) { setFlag(Flag::ZERO, v); }
    void setNegative(bool v) { setFlag(Flag::NEGATIVE, v); }
    void setOverflow(bool v) { setFlag(Flag::V, v); }
    void setFlag(Flag f, bool v);
    bool getFlag(Flag f) const { return m_flags & static_cast<uint8_t>(f); }

    // ALU operations
    void adc(uint8_t val);
    void sbc(uint8_t val);
    void cpx(AddrMode mode);
    void cpy(AddrMode mode);
    void bit(AddrMode mode);

    // Shift/rotate helpers
    void rol_val(uint8_t& val);
    void ror_val(uint8_t& val);
    void asl_val(uint8_t& val);
    void lsr_val(uint8_t& val);

    // Shift/rotate dispatchers
    void asl(AddrMode mode);
    void lsr(AddrMode mode);
    void rol(AddrMode mode);
    void ror(AddrMode mode);

    // Branch
    void branch(bool condition, uint16_t rel);

    // Instruction handlers
    // Load
    void lda(AddrMode mode);
    void ldx(AddrMode mode);
    void ldy(AddrMode mode);

    // Store
    void sta(AddrMode mode);
    void stx(AddrMode mode);
    void sty(AddrMode mode);

    // Transfer
    void tax(AddrMode mode);
    void tay(AddrMode mode);
    void tsx(AddrMode mode);
    void txa(AddrMode mode);
    void tya(AddrMode mode);
    void txs(AddrMode mode);

    // Stack
    void pha(AddrMode mode);
    void pla(AddrMode mode);
    void php(AddrMode mode);
    void plp(AddrMode mode);

    // ALU - Accumulator
    void and_(AddrMode mode);
    void ora(AddrMode mode);
    void eor(AddrMode mode);
    void inc(AddrMode mode);
    void dec(AddrMode mode);
    void incw(uint16_t addr);
    void decw(uint16_t addr);

    // ALU - ADC/SBC
    void adc_imm();
    void sbc_imm();

    // Compare
    void cmp(AddrMode mode);
    void cmp_imm();
    void cmp_zp();
    void cmp_zpx();
    void cmp_abs();
    void cmp_abx();
    void cmp_aby();
    void cpx_impl(uint8_t val);
    void cpy_impl(uint8_t val);
    void cpx_imm();
    void cpx_abs();
    void cpx_zp();
    void cpx_zpy();
    void cpy_imm();
    void cpy_abs();
    void cpy_zp();
    void cpy_abx();

    // Bit
    void bit_zp();
    void bit_abs();

    // Shift/Rotate
    void asl_acc();
    void asl_zp();
    void asl_zpx();
    void asl_abs();
    void asl_abx();
    void lsr_acc();
    void lsr_zp();
    void lsr_zpx();
    void lsr_abs();
    void lsr_abx();
    void rol_acc();
    void rol_zp();
    void rol_zpx();
    void rol_abs();
    void rol_abx();
    void ror_acc();
    void ror_zp();
    void ror_zpx();
    void ror_abs();
    void ror_abx();

    // Compare (shared implementation)
    void cmp_impl(uint8_t a, uint8_t b);

    // Jump/Subroutine
    void jmp_abs(AddrMode mode);
    void jmp_ind(AddrMode mode);
    void jsr(AddrMode mode);

    // Return
    void rts(AddrMode mode);

    // Interrupt
    void brk_impl(AddrMode mode);
    void irq_impl(AddrMode mode);
    void nmi_impl(AddrMode mode);

    // Branch
    void bcc(AddrMode mode);
    void bcs(AddrMode mode);
    void beq(AddrMode mode);
    void bne(AddrMode mode);
    void bvc(AddrMode mode);
    void bvs(AddrMode mode);
    void bpl(AddrMode mode);
    void bmi(AddrMode mode);

    // Compare and branch
    void cmp_branch(uint8_t a, uint8_t b, void (CPU::*branchFn)());

    // Flags
    void clc(AddrMode mode);
    void cld(AddrMode mode);
    void cli(AddrMode mode);
    void clv(AddrMode mode);
    void sec(AddrMode mode);
    void sed(AddrMode mode);
    void sei(AddrMode mode);

    // Invalid opcode handler
    void invalid_opcode(AddrMode mode);

    // 65C02 additions
    void bra(AddrMode mode);
    void lax(AddrMode mode);

    // State
    uint8_t m_a;           // Accumulator
    uint8_t m_x;           // X register
    uint8_t m_y;           // Y register
    uint16_t m_pc;         // Program counter
    uint8_t m_sp;          // Stack pointer (0x00-0xFF maps to $0100-$01FF)
    uint8_t m_flags;       // Flags register

    bool m_halted = false;
    bool m_nmiPending = false;

    // Callbacks
    MemoryCallback m_readCallback = nullptr;
    MemoryWriteCallback m_writeCallback = nullptr;
    CycleCallback m_cycleCallback = nullptr;

    // Disassembly tables
    static constexpr const char* const kOpcodeNames[256] = {
        "SBC","ORA","SLO","NOP","NOP","LAX","SAX","SLO",
        "LDA","LAX","ALS","NOP","ASL","PHA","ASL","ROL",
        "PLP","BMI","SEC","NOP","NOP","LSR","PHX","NOP",
        "TAX","TSB","TRB","LSR","BCC","TYA","TSB","LSR",
        "BIT","ROL","PLA","BVC","CLI","NOP","RTS","NOP",
        "ADY","TAY","DEY","BPL","CLC","INC","NOP","NOP",
        "JSR","CMP","SRE","NOP","NOP","LAX","DEC","SRE",
        "LDA","LAX","NOP","NOP","DEC","NOP","DEC","CMP",
        "NOP","BCS","CLV","CLD","NOP","STY","STX","DEC",
        "LDA","LAX","LAX","NOP","BIT","NOP","BIT","CMP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
        "NOP","NOP","NOP","NOP","NOP","NOP","NOP","NOP",
    };

    // Instruction execution table
    using InstructionFn = void (CPU::*)(AddrMode);
    struct Instruction {
        InstructionFn fn = nullptr;
        AddrMode mode = AddrMode::IMMEDIATE;
        const uint8_t cycles;
        const uint8_t bytes;
    };

    static constexpr Instruction kInstructions[256] = {
        // Row 0 ($00-0F)
        {&CPU::brk_impl,    AddrMode::IMMEDIATE, 7, 2},  // 00: BRK
        {&CPU::ora,         AddrMode::IDX,        6, 2},  // 01: ORA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 02: SLO (invalid on 6502)
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 03: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 3, 2}, // 04: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 3, 2}, // 05: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 3, 2}, // 06: SAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 07: SLO
        {&CPU::lda,         AddrMode::IDX,        5, 2},  // 08: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 2}, // 09: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 0A: ALS
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 0B: NOP
        {&CPU::asl,         AddrMode::ZP,         5, 2},  // 0C: ASL (invalid)
        {&CPU::pha,         AddrMode::IMMEDIATE, 3, 1},   // 0D: PHA
        {&CPU::asl,         AddrMode::ABS,        6, 3},  // 0E: ASL
        {&CPU::rol,         AddrMode::ZP,         5, 2},  // 0F: ROL

        // Row 1 ($10-1F)
        {&CPU::plp,         AddrMode::IMMEDIATE, 4, 1},   // 10: PLP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 11: BMI
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 12: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 13: NOP
        {&CPU::lsr,         AddrMode::ZP,         5, 2},  // 14: LSR
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 15: PHX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 16: NOP
        {&CPU::tax,         AddrMode::IMMEDIATE, 2, 1},   // 17: TAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 2}, // 18: TSB
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 2}, // 19: TRB
        {&CPU::lsr,         AddrMode::ABS,        6, 3},  // 1A: LSR
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 1B: BCC
        {&CPU::tya,         AddrMode::IMMEDIATE, 2, 1},   // 1C: TYA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 2}, // 1D: TSB
        {&CPU::lsr,         AddrMode::ABX,        7, 3},  // 1E: LSR
        {&CPU::rol,         AddrMode::ABS,        6, 3},  // 1F: ROL

        // Row 2 ($20-2F)
        {&CPU::jsr,         AddrMode::ABS,        6, 3},  // 20: JSR
        {&CPU::cmp,         AddrMode::IDX,        6, 2},  // 21: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 22: SRE
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 23: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // 24: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // 25: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // 26: DEC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 27: SRE
        {&CPU::lda,         AddrMode::ABS,        4, 3},  // 28: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 3}, // 29: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 2A: ALI
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 2B: NOP
        {&CPU::dec,         AddrMode::ABS,        6, 3},  // 2C: DEC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 2D: ISX
        {&CPU::dec,         AddrMode::ABX,        7, 3},  // 2E: DEC
        {&CPU::cmp,         AddrMode::ABX,        4, 3},  // 2F: CMP

        // Row 3 ($30-3F)
        {&CPU::pla,         AddrMode::IMMEDIATE, 6, 1},   // 30: PLA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 31: BVC
        {&CPU::cli,         AddrMode::IMMEDIATE, 2, 1},   // 32: CLI
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 33: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 34: RTS
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 35: AYU
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 3}, // 36: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 3}, // 37: ADY
        {&CPU::tay,         AddrMode::IMMEDIATE, 2, 1},   // 38: TAY
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 39: DEY (invalid)
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 3A: BPL
        {&CPU::clc,         AddrMode::IMMEDIATE, 2, 1},   // 3B: CLC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 3C: INC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 3D: TOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // 3E: NOP
        {&CPU::cmp,         AddrMode::ABY,        4, 3},  // 3F: CMP

        // Row 4 ($40-4F)
        {&CPU::jsr,         AddrMode::ABS,        6, 3},  // 40: JSR (actually RTI on 6502, but JSR at 0x40 is wrong)
        {&CPU::cmp,         AddrMode::ZP,         3, 2},  // 41: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 42: SRE
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 43: NOP
        {&CPU::bit,         AddrMode::ZP,         3, 2},  // 44: BIT
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 3, 2}, // 45: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 3, 2}, // 46: DEC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 47: SRE
        {&CPU::lda,         AddrMode::ZP,         3, 2},  // 48: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 3, 2}, // 49: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 4A: ALI
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 4B: NOP
        {&CPU::bit,         AddrMode::ABS,        4, 3},  // 4C: BIT
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 3}, // 4D: ISX
        {&CPU::dec,         AddrMode::ABS,        6, 3},  // 4E: DEC
        {&CPU::cmp,         AddrMode::ABS,        4, 3},  // 4F: CMP

        // Row 5 ($50-5F)
        {&CPU::rol,         AddrMode::ZPX,        6, 2},  // 50: ROL
        {&CPU::cmp,         AddrMode::ABY,        4, 3},  // 51: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 52: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 53: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 54: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 55: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 56: DEC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 57: SRE
        {&CPU::lda,         AddrMode::ZPX,        4, 2},  // 58: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 3}, // 59: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 5A: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 5B: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 5C: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 5D: ISX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 7, 3}, // 5E: DEC
        {&CPU::cmp,         AddrMode::ABX,        4, 3},  // 5F: CMP

        // Row 6 ($60-6F)
        {&CPU::rts,         AddrMode::IMMEDIATE, 6, 1},   // 60: RTS
        {&CPU::cmp,         AddrMode::IDX,        6, 2},  // 61: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 62: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 63: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 64: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // 65: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // 66: DEC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 67: SRE
        {&CPU::lda,         AddrMode::ABY,        4, 3},  // 68: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 3}, // 69: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 6A: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 6B: NOP
        {&CPU::bit,         AddrMode::ABS,        4, 3},  // 6C: BIT
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // 6D: ISY
        {&CPU::bit,         AddrMode::ABS,        4, 3},  // 6E: BIT
        {&CPU::cmp,         AddrMode::ABY,        4, 3},  // 6F: CMP

        // Row 7 ($70-7F)
        {&CPU::ror,         AddrMode::ABX,        7, 3},  // 70: ROR
        {&CPU::cmp,         AddrMode::ABY,        4, 3},  // 71: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 72: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 73: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 74: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 75: LAX
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 76: DEC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 77: CMP
        {&CPU::lda,         AddrMode::ABX,        4, 3},  // 78: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 79: TCB
        {&CPU::tya,         AddrMode::IMMEDIATE, 2, 1},   // 7A: TBY
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 7B: BVC
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 7C: TCB
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // 7D: BDN
        {&CPU::tya,         AddrMode::IMMEDIATE, 2, 1},   // 7E: TBY
        {&CPU::cmp,         AddrMode::ABX,        4, 3},  // 7F: CMP

        // Row 8 ($80-8F)
        {&CPU::lda,         AddrMode::IMMEDIATE, 2, 2},   // 80: LDA (actually BRA)
        {&CPU::lda,         AddrMode::IDY,        5, 2},  // 81: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 82: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 83: NOP
        {&CPU::ldy,         AddrMode::IMMEDIATE, 2, 2},   // 84: LDY
        {&CPU::lda,         AddrMode::ABS,        4, 3},  // 85: LDA
        {&CPU::ldx,         AddrMode::IMMEDIATE, 2, 2},   // 86: LDX
        {&CPU::lda,         AddrMode::ABY,        4, 3},  // 87: LDA
        {&CPU::ldy,         AddrMode::ABS,        4, 3},  // 88: LDY
        {&CPU::sta,         AddrMode::ZP,         3, 2},  // 89: STA
        {&CPU::ldx,         AddrMode::ABS,        4, 3},  // 8A: LDX
        {&CPU::lda,         AddrMode::ABSIND,     6, 3},  // 8B: LDA
        {&CPU::ldy,         AddrMode::ZPX,        4, 2},  // 8C: LDY
        {&CPU::sta,         AddrMode::ABS,        4, 3},  // 8D: STA
        {&CPU::ldx,         AddrMode::ZPY,        4, 2},  // 8E: LDX
        {&CPU::lda,         AddrMode::ABSIND,     6, 3},  // 8F: LDA

        // Row 9 ($90-9F)
        {&CPU::bcc,         AddrMode::REL,          2, 2}, // 90: BCC
        {&CPU::sta,         AddrMode::IDX,        6, 2},  // 91: STA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 92: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // 93: NOP
        {&CPU::sty,         AddrMode::ZP,         3, 2},  // 94: STY
        {&CPU::sta,         AddrMode::ZPX,        6, 2},  // 95: STA
        {&CPU::stx,         AddrMode::ZPY,        4, 2},  // 96: STX
        {&CPU::sta,         AddrMode::ZP,         3, 2},  // 97: STA
        {&CPU::bcc,         AddrMode::REL,          2, 2}, // 98: BCC
        {&CPU::sta,         AddrMode::IDY,        5, 2},  // 99: STA
        {&CPU::tax,         AddrMode::IMMEDIATE, 2, 1},   // 9A: TAX
        {&CPU::sta,         AddrMode::ABSIND,     6, 3},  // 9B: STA
        {&CPU::stx,         AddrMode::ABS,        4, 3},  // 9C: STX
        {&CPU::sta,         AddrMode::ABX,        5, 3},  // 9D: STA
        {&CPU::stx,         AddrMode::ZP,         3, 2},  // 9E: STX
        {&CPU::sta,         AddrMode::ABX,        5, 3},  // 9F: STA

        // Row A ($A0-AF)
        {&CPU::ldy,         AddrMode::IMMEDIATE, 2, 2},   // A0: LDY
        {&CPU::lda,         AddrMode::IDX,        6, 2},  // A1: LDA
        {&CPU::ldx,         AddrMode::IMMEDIATE, 2, 2},   // A2: LDX
        {&CPU::lda,         AddrMode::ABS,        4, 3},  // A3: LDA
        {&CPU::ldy,         AddrMode::IMMEDIATE, 2, 2},   // A4: LDY
        {&CPU::lda,         AddrMode::ZP,         3, 2},  // A5: LDA
        {&CPU::ldx,         AddrMode::ZP,         3, 2},  // A6: LDX
        {&CPU::lda,         AddrMode::ZP,         3, 2},  // A7: LDA
        {&CPU::ldy,         AddrMode::ABS,        4, 3},  // A8: LDY
        {&CPU::lda,         AddrMode::IMMEDIATE, 2, 2},   // A9: LDA
        {&CPU::ldx,         AddrMode::ABS,        4, 3},  // AA: LDX
        {&CPU::lda,         AddrMode::ABS,        4, 3},  // AB: LDA
        {&CPU::ldy,         AddrMode::ZPX,        4, 2},  // AC: LDY
        {&CPU::lda,         AddrMode::ABS,        4, 3},  // AD: LDA
        {&CPU::ldx,         AddrMode::ZPY,        4, 2},  // AE: LDX
        {&CPU::lda,         AddrMode::ABS,        4, 3},  // AF: LDA

        // Row B ($B0-BF)
        {&CPU::bcs,         AddrMode::REL,          2, 2}, // B0: BCS
        {&CPU::lda,         AddrMode::IDX,        6, 2},  // B1: LDA
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // B2: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // B3: NOP
        {&CPU::ldy,         AddrMode::ZPX,        4, 2},  // B4: LDY
        {&CPU::lda,         AddrMode::ZPX,        4, 2},  // B5: LDA
        {&CPU::ldx,         AddrMode::ZPY,        4, 2},  // B6: LDX
        {&CPU::lda,         AddrMode::ZPY,        4, 2},  // B7: LDA
        {&CPU::ldy,         AddrMode::ABX,        4, 3},  // B8: LDY
        {&CPU::lda,         AddrMode::ABY,        4, 3},  // B9: LDA
        {&CPU::tax,         AddrMode::IMMEDIATE, 2, 1},   // BA: TAX
        {&CPU::lda,         AddrMode::ABY,        4, 3},  // BB: LDA
        {&CPU::ldy,         AddrMode::ABX,        5, 3},  // BC: LDY
        {&CPU::lda,         AddrMode::ABX,        4, 3},  // BD: LDA
        {&CPU::ldx,         AddrMode::ABY,        4, 3},  // BE: LDX
        {&CPU::lda,         AddrMode::ABY,        4, 3},  // BF: LDA

        // Row C ($C0-CF)
        {&CPU::cpy,         AddrMode::IMMEDIATE, 2, 2},   // C0: CPY
        {&CPU::cmp,         AddrMode::IDX,        6, 2},  // C1: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // C2: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // C3: NOP
        {&CPU::cpy,         AddrMode::ZP,         3, 2},  // C4: CPY
        {&CPU::cmp,         AddrMode::ZP,         3, 2},  // C5: CMP
        {&CPU::cpx,         AddrMode::ZP,         3, 2},  // C6: CPX
        {&CPU::cmp,         AddrMode::ZP,         3, 2},  // C7: CMP
        {&CPU::cpy,         AddrMode::ABS,        4, 3},  // C8: CPY
        {&CPU::cmp,         AddrMode::IMMEDIATE, 2, 2},   // C9: CMP
        {&CPU::cpx,         AddrMode::ABS,        4, 3},  // CA: CPX
        {&CPU::cmp,         AddrMode::ABS,        4, 3},  // CB: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 4, 3}, // CC: NOP
        {&CPU::cmp,         AddrMode::ABS,        4, 3},  // CD: CMP
        {&CPU::cpx,         AddrMode::ZPY,        4, 2},  // CE: CPX
        {&CPU::cmp,         AddrMode::ABS,        4, 3},  // CF: CMP

        // Row D ($D0-DF)
        {&CPU::bne,         AddrMode::IMMEDIATE, 2, 2},   // D0: BNE
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // D1: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // D2: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // D3: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // D4: NOP
        {&CPU::cmp,         AddrMode::ZPX,        4, 2},  // D5: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 5, 3}, // D6: NOP
        {&CPU::cmp,         AddrMode::ZPX,        4, 2},  // D7: CMP
        {&CPU::cld,         AddrMode::IMMEDIATE, 2, 1},   // D8: CLD
        {&CPU::cmp,         AddrMode::IDY,        5, 2},  // D9: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // DA: NOP
        {&CPU::cmp,         AddrMode::ABY,        4, 3},  // DB: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 6, 3}, // DC: NOP
        {&CPU::cmp,         AddrMode::ABX,        4, 3},  // DD: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 7, 3}, // DE: NOP
        {&CPU::cmp,         AddrMode::ABX,        4, 3},  // DF: CMP

        // Row E ($E0-EF)
        {&CPU::cpx,         AddrMode::IMMEDIATE, 2, 2},   // E0: CPX
        {&CPU::cmp,         AddrMode::IDX,        6, 2},  // E1: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // E2: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // E3: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // E4: NOP
        {&CPU::cmp,         AddrMode::ZP,         3, 2},  // E5: CMP
        {&CPU::cpx,         AddrMode::ZP,         3, 2},  // E6: CPX
        {&CPU::cmp,         AddrMode::ZP,         3, 2},  // E7: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // E8: INX
        {&CPU::cmp,         AddrMode::IMMEDIATE, 2, 2},   // E9: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // EA: NOP
        {&CPU::cmp,         AddrMode::IMMEDIATE, 2, 2},   // EB: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // EC: NOP
        {&CPU::cmp,         AddrMode::ABS,        4, 3},  // ED: CMP
        {&CPU::cpx,         AddrMode::ABS,        4, 3},  // EE: CPX
        {&CPU::cmp,         AddrMode::ABS,        4, 3},  // EF: CMP

        // Row F ($F0-FF)
        {&CPU::beq,         AddrMode::IMMEDIATE, 2, 2},   // F0: BEQ
        {&CPU::cmp,         AddrMode::IDX,        6, 2},  // F1: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // F2: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // F3: NOP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // F4: NOP
        {&CPU::cmp,         AddrMode::ZPX,        4, 2},  // F5: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // F6: NOP
        {&CPU::cmp,         AddrMode::ZPX,        4, 2},  // F7: CMP
        {&CPU::sei,         AddrMode::IMMEDIATE, 2, 1},   // F8: SEI
        {&CPU::cmp,         AddrMode::IDY,        5, 2},  // F9: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // FA: NOP
        {&CPU::cmp,         AddrMode::ABY,        4, 3},  // FB: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // FC: NOP
        {&CPU::cmp,         AddrMode::ABX,        4, 3},  // FD: CMP
        {&CPU::invalid_opcode, AddrMode::IMMEDIATE, 2, 2}, // FE: NOP
        {&CPU::cmp,         AddrMode::ABX,        4, 3},  // FF: CMP
    };

    // Fetch next byte from memory
    uint8_t fetchByte();
    // Fetch 16-bit value (little-endian)
    uint16_t fetchWord();
};

} // namespace apple2e
