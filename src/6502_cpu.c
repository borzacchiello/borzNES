#include "6502_cpu.h"
#include "system.h"
#include "memory.h"
#include "logging.h"
#include "alloc.h"

#include <string.h>
#include <stdio.h>

// https://github.com/fogleman/nes/blob/master/nes/cpu.go

#define AMODE_UNUSED      0
#define AMODE_ABSOLUTE    1
#define AMODE_ABSOLUTEX   2
#define AMODE_ABSOLUTEY   3
#define AMODE_ACCUMULATOR 4
#define AMODE_IMMEDIATE   5
#define AMODE_IMPLIED     6
#define AMODE_XINDIRECT   7
#define AMODE_INDIRECT    8
#define AMODE_INDIRECTY   9
#define AMODE_RELATIVE    10
#define AMODE_ZEROPAGE    11
#define AMODE_ZEROPAGEX   12
#define AMODE_ZEROPAGEY   13

#define NMI_VECTOR_ADDR     0xFFFAu
#define RESET_VECTOR_ADDR   0xFFFCu
#define IRQ_BRK_VECTOR_ADDR 0xFFFEu

#define INT_NMI 1
#define INT_IRQ 2

// addressing mode for each instruction
static uint8_t instr_addr_mode[] = {
    6,  7,  6,  7,  11, 11, 11, 11, 6,  5,  4,  5,  1,  1,  1,  1,  10, 9,  6,
    9,  12, 12, 12, 12, 6,  3,  6,  3,  2,  2,  2,  2,  1,  7,  6,  7,  11, 11,
    11, 11, 6,  5,  4,  5,  1,  1,  1,  1,  10, 9,  6,  9,  12, 12, 12, 12, 6,
    3,  6,  3,  2,  2,  2,  2,  6,  7,  6,  7,  11, 11, 11, 11, 6,  5,  4,  5,
    1,  1,  1,  1,  10, 9,  6,  9,  12, 12, 12, 12, 6,  3,  6,  3,  2,  2,  2,
    2,  6,  7,  6,  7,  11, 11, 11, 11, 6,  5,  4,  5,  8,  1,  1,  1,  10, 9,
    6,  9,  12, 12, 12, 12, 6,  3,  6,  3,  2,  2,  2,  2,  5,  7,  5,  7,  11,
    11, 11, 11, 6,  5,  6,  5,  1,  1,  1,  1,  10, 9,  6,  9,  12, 12, 13, 13,
    6,  3,  6,  3,  2,  2,  3,  3,  5,  7,  5,  7,  11, 11, 11, 11, 6,  5,  6,
    5,  1,  1,  1,  1,  10, 9,  6,  9,  12, 12, 13, 13, 6,  3,  6,  3,  2,  2,
    3,  3,  5,  7,  5,  7,  11, 11, 11, 11, 6,  5,  6,  5,  1,  1,  1,  1,  10,
    9,  6,  9,  12, 12, 12, 12, 6,  3,  6,  3,  2,  2,  2,  2,  5,  7,  5,  7,
    11, 11, 11, 11, 6,  5,  6,  5,  1,  1,  1,  1,  10, 9,  6,  9,  12, 12, 12,
    12, 6,  3,  6,  3,  2,  2,  2,  2,
};

// size of each instruction in bytes
static uint8_t instr_size[] = {
    2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, 2, 2, 0, 0, 2, 2, 2, 0,
    1, 3, 1, 0, 3, 3, 3, 0, 3, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0, 1, 2, 0, 0, 2, 2, 2, 0,
    1, 2, 1, 0, 3, 3, 3, 0, 2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    1, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, 2, 2, 0, 0, 2, 2, 2, 0,
    1, 3, 1, 0, 3, 3, 3, 0, 2, 2, 0, 0, 2, 2, 2, 0, 1, 0, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 0, 3, 0, 0, 2, 2, 2, 0, 2, 2, 2, 0,
    1, 2, 1, 0, 3, 3, 3, 0, 2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0, 2, 2, 0, 0, 2, 2, 2, 0,
    1, 3, 1, 0, 3, 3, 3, 0, 2, 2, 0, 0, 2, 2, 2, 0, 1, 2, 1, 0, 3, 3, 3, 0,
    2, 2, 0, 0, 2, 2, 2, 0, 1, 3, 1, 0, 3, 3, 3, 0};

// number of cycles used by each instruction, not including conditional cycles
static uint8_t instr_cycle[] = {
    7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6, 2, 5, 2, 8, 4, 4, 6, 6,
    2, 4, 2, 7, 4, 4, 7, 7, 6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, 6, 6, 2, 8, 3, 3, 5, 5,
    3, 2, 2, 2, 3, 4, 6, 6, 2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7,
    6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6, 2, 5, 2, 8, 4, 4, 6, 6,
    2, 4, 2, 7, 4, 4, 7, 7, 2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,
    2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5, 2, 6, 2, 6, 3, 3, 3, 3,
    2, 2, 2, 2, 4, 4, 4, 4, 2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4,
    2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6, 2, 5, 2, 8, 4, 4, 6, 6,
    2, 4, 2, 7, 4, 4, 7, 7, 2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,
    2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7};

// number of cycles used by each instruction when a page is crossed
static uint8_t instr_page_cycle[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 0};

static const char* instr_name[] = {
    "BRK", "ORA", "KIL", "SLO", "NOP", "ORA", "ASL", "SLO", "PHP", "ORA", "ASL",
    "ANC", "NOP", "ORA", "ASL", "SLO", "BPL", "ORA", "KIL", "SLO", "NOP", "ORA",
    "ASL", "SLO", "CLC", "ORA", "NOP", "SLO", "NOP", "ORA", "ASL", "SLO", "JSR",
    "AND", "KIL", "RLA", "BIT", "AND", "ROL", "RLA", "PLP", "AND", "ROL", "ANC",
    "BIT", "AND", "ROL", "RLA", "BMI", "AND", "KIL", "RLA", "NOP", "AND", "ROL",
    "RLA", "SEC", "AND", "NOP", "RLA", "NOP", "AND", "ROL", "RLA", "RTI", "EOR",
    "KIL", "SRE", "NOP", "EOR", "LSR", "SRE", "PHA", "EOR", "LSR", "ALR", "JMP",
    "EOR", "LSR", "SRE", "BVC", "EOR", "KIL", "SRE", "NOP", "EOR", "LSR", "SRE",
    "CLI", "EOR", "NOP", "SRE", "NOP", "EOR", "LSR", "SRE", "RTS", "ADC", "KIL",
    "RRA", "NOP", "ADC", "ROR", "RRA", "PLA", "ADC", "ROR", "ARR", "JMP", "ADC",
    "ROR", "RRA", "BVS", "ADC", "KIL", "RRA", "NOP", "ADC", "ROR", "RRA", "SEI",
    "ADC", "NOP", "RRA", "NOP", "ADC", "ROR", "RRA", "NOP", "STA", "NOP", "SAX",
    "STY", "STA", "STX", "SAX", "DEY", "NOP", "TXA", "XAA", "STY", "STA", "STX",
    "SAX", "BCC", "STA", "KIL", "AHX", "STY", "STA", "STX", "SAX", "TYA", "STA",
    "TXS", "TAS", "SHY", "STA", "SHX", "AHX", "LDY", "LDA", "LDX", "LAX", "LDY",
    "LDA", "LDX", "LAX", "TAY", "LDA", "TAX", "LAX", "LDY", "LDA", "LDX", "LAX",
    "BCS", "LDA", "KIL", "LAX", "LDY", "LDA", "LDX", "LAX", "CLV", "LDA", "TSX",
    "LAS", "LDY", "LDA", "LDX", "LAX", "CPY", "CMP", "NOP", "DCP", "CPY", "CMP",
    "DEC", "DCP", "INY", "CMP", "DEX", "AXS", "CPY", "CMP", "DEC", "DCP", "BNE",
    "CMP", "KIL", "DCP", "NOP", "CMP", "DEC", "DCP", "CLD", "CMP", "NOP", "DCP",
    "NOP", "CMP", "DEC", "DCP", "CPX", "SBC", "NOP", "ISC", "CPX", "SBC", "INC",
    "ISC", "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "ISC", "BEQ", "SBC",
    "KIL", "ISC", "NOP", "SBC", "INC", "ISC", "SED", "SBC", "NOP", "ISC", "NOP",
    "SBC", "INC", "ISC"};

typedef struct {
    uint16_t addr;
    uint16_t PC;
    uint8_t  mode;
} HandlerData;

typedef void (*opcode_handler)(Cpu*, HandlerData*);

static uint8_t pack_flags(Cpu* cpu)
{
    uint8_t res = 0;
    res |= cpu->C;
    res |= cpu->Z << 1;
    res |= cpu->I << 2;
    res |= cpu->D << 3;
    res |= cpu->B << 4;
    res |= cpu->U << 5;
    res |= cpu->V << 6;
    res |= cpu->N << 7;
    return res;
}

static void unpack_flags(Cpu* cpu, uint8_t flags)
{
    cpu->C = flags & 1;
    cpu->Z = (flags >> 1) & 1;
    cpu->I = (flags >> 2) & 1;
    cpu->D = (flags >> 3) & 1;
    cpu->B = (flags >> 4) & 1;
    cpu->U = (flags >> 5) & 1;
    cpu->V = (flags >> 6) & 1;
    cpu->N = (flags >> 7) & 1;
}

static uint16_t read_16(Memory* mem, uint16_t addr)
{
    uint16_t l = memory_read(mem, addr);
    uint16_t h = memory_read(mem, addr + 1);
    return l | (h << 8);
}

// emulates a 6502 bug that caused the low byte
// to wrap without incrementing the high byte
static uint16_t read_16_bug(Memory* mem, uint16_t addr)
{
    uint16_t l = memory_read(mem, addr);
    uint16_t h = memory_read(mem, (addr & 0xff00u) | ((addr + 1) & 0x00ffu));
    return l | (h << 8);
}

static void stack_push(Cpu* cpu, uint8_t v)
{
    uint16_t addr = 0x100u | (uint16_t)cpu->SP;
    memory_write(cpu->mem, addr, v);
    cpu->SP--;
}

static uint8_t stack_pop(Cpu* cpu)
{
    cpu->SP++;
    uint16_t addr = 0x100u | (uint16_t)cpu->SP;
    uint8_t  res  = memory_read(cpu->mem, addr);
    return res;
}

static uint16_t stack_pop16(Cpu* cpu);
static void     stack_push16(Cpu* cpu, uint16_t v)
{
    uint8_t h = v >> 8;
    uint8_t l = v & 0xff;
    stack_push(cpu, h);
    stack_push(cpu, l);
}

static uint16_t stack_pop16(Cpu* cpu)
{
    uint16_t l = stack_pop(cpu);
    uint16_t h = stack_pop(cpu);
    return (h << 8) | l;
}

static inline void setZ(Cpu* cpu, uint8_t v) { cpu->Z = v == 0 ? 1 : 0; }
static inline void setN(Cpu* cpu, uint8_t v)
{
    cpu->N = (v & 0x80) != 0 ? 1 : 0;
}
static void compare(Cpu* cpu, uint8_t a, uint8_t b)
{
    setZ(cpu, a - b);
    setN(cpu, a - b);
    cpu->C = a >= b ? 1 : 0;
}

static inline int different_page(uint16_t a1, uint16_t a2)
{
    return (a1 >> 8) != (a2 >> 8);
}

static void add_branch_cycles(Cpu* cpu, HandlerData* hd)
{
    cpu->cycles++;

    if (different_page(hd->PC, hd->addr))
        cpu->cycles++;
}

static void handler_dey(Cpu* cpu, HandlerData* hd)
{
    cpu->Y--;
    setZ(cpu, cpu->Y);
    setN(cpu, cpu->Y);
}

static void handler_beq(Cpu* cpu, HandlerData* hd)
{
    if (cpu->Z) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_plp(Cpu* cpu, HandlerData* hd)
{
    unpack_flags(cpu, stack_pop(cpu) & 0xEF | 0x20 /* unused */);
}

static void handler_rts(Cpu* cpu, HandlerData* hd)
{
    cpu->PC = stack_pop16(cpu) + 1;
}

static void handler_ldx(Cpu* cpu, HandlerData* hd)
{
    cpu->X = memory_read(cpu->mem, hd->addr);
    setZ(cpu, cpu->X);
    setN(cpu, cpu->X);
}

static void handler_sta(Cpu* cpu, HandlerData* hd)
{
    memory_write(cpu->mem, hd->addr, cpu->A);
}

static void handler_txs(Cpu* cpu, HandlerData* hd) { cpu->SP = cpu->X; }

static void handler_iny(Cpu* cpu, HandlerData* hd)
{
    cpu->Y++;
    setZ(cpu, cpu->Y);
    setN(cpu, cpu->Y);
}

static void handler_clv(Cpu* cpu, HandlerData* hd) { cpu->V = 0; }

static void handler_ora(Cpu* cpu, HandlerData* hd)
{
    cpu->A = cpu->A | memory_read(cpu->mem, hd->addr);
    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
}

static void handler_sei(Cpu* cpu, HandlerData* hd) { cpu->I = 1; }

static void handler_cli(Cpu* cpu, HandlerData* hd) { cpu->I = 0; }

static void handler_rti(Cpu* cpu, HandlerData* hd)
{
    handler_plp(cpu, hd);
    cpu->PC = stack_pop16(cpu);
}

static void handler_bit(Cpu* cpu, HandlerData* hd)
{
    uint8_t v = memory_read(cpu->mem, hd->addr);
    cpu->V    = (v >> 6) & 1;
    setZ(cpu, v & cpu->A);
    setN(cpu, v);
}

static void handler_pha(Cpu* cpu, HandlerData* hd) { stack_push(cpu, cpu->A); }

static void handler_tay(Cpu* cpu, HandlerData* hd)
{
    cpu->Y = cpu->A;
    setZ(cpu, cpu->Y);
    setN(cpu, cpu->Y);
}

static void handler_tax(Cpu* cpu, HandlerData* hd)
{
    cpu->X = cpu->A;
    setZ(cpu, cpu->X);
    setN(cpu, cpu->X);
}

static void handler_inc(Cpu* cpu, HandlerData* hd)
{
    uint8_t v = memory_read(cpu->mem, hd->addr) + 1;
    memory_write(cpu->mem, hd->addr, v);
    setZ(cpu, v);
    setN(cpu, v);
}

static void handler_adc(Cpu* cpu, HandlerData* hd)
{
    uint8_t a = cpu->A;
    uint8_t b = memory_read(cpu->mem, hd->addr);
    uint8_t c = cpu->C;
    cpu->A    = a + b + c;

    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
    cpu->C = ((int32_t)a + (int32_t)b + (int32_t)c > 0xFF) ? 1 : 0;
    cpu->V = (((a ^ b) & 0x80) == 0 && ((a ^ cpu->A) & 0x80) == 0) ? 1 : 0;
}

static void handler_nop(Cpu* cpu, HandlerData* hd) {}

static void handler_bvc(Cpu* cpu, HandlerData* hd)
{
    if (!cpu->V) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_cld(Cpu* cpu, HandlerData* hd) { cpu->D = 0; }

static void handler_asl(Cpu* cpu, HandlerData* hd)
{
    if (hd->mode == AMODE_ACCUMULATOR) {
        cpu->C = (cpu->A >> 7) & 1;
        cpu->A = cpu->A << 1;
        setZ(cpu, cpu->A);
        setN(cpu, cpu->A);
    } else {
        uint8_t v = memory_read(cpu->mem, hd->addr);
        cpu->C    = (v >> 7) & 1;
        v         = v << 1;
        memory_write(cpu->mem, hd->addr, v);
        setZ(cpu, v);
        setN(cpu, v);
    }
}

static void handler_inx(Cpu* cpu, HandlerData* hd)
{
    cpu->X++;
    setZ(cpu, cpu->X);
    setN(cpu, cpu->X);
}

static void handler_bvs(Cpu* cpu, HandlerData* hd)
{
    if (cpu->V) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_dex(Cpu* cpu, HandlerData* hd)
{
    cpu->X--;
    setZ(cpu, cpu->X);
    setN(cpu, cpu->X);
}

static void handler_ldy(Cpu* cpu, HandlerData* hd)
{
    cpu->Y = memory_read(cpu->mem, hd->addr);
    setZ(cpu, cpu->Y);
    setN(cpu, cpu->Y);
}

static void handler_rol(Cpu* cpu, HandlerData* hd)
{
    uint8_t c = cpu->C;
    if (hd->mode == AMODE_ACCUMULATOR) {
        cpu->C = (cpu->A >> 7) & 1;
        cpu->A = (cpu->A << 1) | c;
        setZ(cpu, cpu->A);
        setN(cpu, cpu->A);
    } else {
        uint8_t v = memory_read(cpu->mem, hd->addr);
        cpu->C    = (v >> 7) & 1;
        v         = (v << 1) | c;
        memory_write(cpu->mem, hd->addr, v);
        setZ(cpu, v);
        setN(cpu, v);
    }
}

static void handler_cpx(Cpu* cpu, HandlerData* hd)
{
    compare(cpu, cpu->X, memory_read(cpu->mem, hd->addr));
}

static void handler_stx(Cpu* cpu, HandlerData* hd)
{
    memory_write(cpu->mem, hd->addr, cpu->X);
}

static void handler_txa(Cpu* cpu, HandlerData* hd)
{
    cpu->A = cpu->X;
    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
}

static void handler_bpl(Cpu* cpu, HandlerData* hd)
{
    if (!cpu->N) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_sed(Cpu* cpu, HandlerData* hd) { cpu->D = 1; }

static void handler_pla(Cpu* cpu, HandlerData* hd)
{
    cpu->A = stack_pop(cpu);
    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
}

static void handler_lsr(Cpu* cpu, HandlerData* hd)
{
    if (hd->mode == AMODE_ACCUMULATOR) {
        cpu->C = (cpu->A >> 7) & 1;
        cpu->A = cpu->A >> 1;
        setZ(cpu, cpu->A);
        setN(cpu, cpu->A);
    } else {
        uint8_t v = memory_read(cpu->mem, hd->addr);
        cpu->C    = (v >> 7) & 1;
        v         = v >> 1;
        memory_write(cpu->mem, hd->addr, v);
        setZ(cpu, v);
        setN(cpu, v);
    }
}

static void handler_tsx(Cpu* cpu, HandlerData* hd)
{
    cpu->X = cpu->SP;
    setZ(cpu, cpu->X);
    setN(cpu, cpu->X);
}

static void handler_bne(Cpu* cpu, HandlerData* hd)
{
    if (!cpu->Z) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_php(Cpu* cpu, HandlerData* hd)
{
    stack_push(cpu, pack_flags(cpu) | 0x10 /* break flag */);
}

static void handler_cpy(Cpu* cpu, HandlerData* hd)
{
    compare(cpu, cpu->Y, memory_read(cpu->mem, hd->addr));
}

static void handler_bcc(Cpu* cpu, HandlerData* hd)
{
    if (!cpu->C) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_lda(Cpu* cpu, HandlerData* hd)
{
    cpu->A = memory_read(cpu->mem, hd->addr);
    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
}

static void handler_jsr(Cpu* cpu, HandlerData* hd)
{
    stack_push16(cpu, cpu->PC - 1);
    cpu->PC = hd->addr;
}

static void handler_eor(Cpu* cpu, HandlerData* hd)
{
    cpu->A = cpu->A ^ memory_read(cpu->mem, hd->addr);
    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
}

static void handler_sbc(Cpu* cpu, HandlerData* hd)
{
    uint8_t a = cpu->A;
    uint8_t b = memory_read(cpu->mem, hd->addr);
    uint8_t c = cpu->C;
    cpu->A    = a - b - (1 - c);

    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
    cpu->C = ((int32_t)a - (int32_t)b - (int32_t)(1 - c) > 0) ? 1 : 0;
    cpu->V = (((a ^ b) & 0x80) != 0 && ((a ^ cpu->A) & 0x80) != 0) ? 1 : 0;
}

static void handler_bcs(Cpu* cpu, HandlerData* hd)
{
    if (cpu->C) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_bmi(Cpu* cpu, HandlerData* hd)
{
    if (cpu->N) {
        cpu->PC = hd->addr;
        add_branch_cycles(cpu, hd);
    }
}

static void handler_dec(Cpu* cpu, HandlerData* hd)
{
    uint8_t v = memory_read(cpu->mem, hd->addr) - 1;
    memory_write(cpu->mem, hd->addr, v);
    setZ(cpu, v);
    setN(cpu, v);
}

static void handler_and(Cpu* cpu, HandlerData* hd)
{
    cpu->A = cpu->A & memory_read(cpu->mem, hd->addr);
    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
}

static void handler_ror(Cpu* cpu, HandlerData* hd)
{
    uint8_t c = cpu->C;
    if (hd->mode == AMODE_ACCUMULATOR) {
        cpu->C = cpu->A & 1;
        cpu->A = (cpu->A >> 1) | (c << 7);
        setZ(cpu, cpu->A);
        setN(cpu, cpu->A);
    } else {
        uint8_t v = memory_read(cpu->mem, hd->addr);
        cpu->C    = v & 1;
        v         = (v >> 1) | (c << 7);
        memory_write(cpu->mem, hd->addr, v);
        setZ(cpu, v);
        setN(cpu, v);
    }
}

static void handler_jmp(Cpu* cpu, HandlerData* hd) { cpu->PC = hd->addr; }

static void handler_sec(Cpu* cpu, HandlerData* hd) { cpu->C = 1; }

static void handler_sty(Cpu* cpu, HandlerData* hd)
{
    memory_write(cpu->mem, hd->addr, cpu->Y);
}

static void handler_clc(Cpu* cpu, HandlerData* hd) { cpu->C = 0; }

static void handler_cmp(Cpu* cpu, HandlerData* hd)
{
    compare(cpu, cpu->A, memory_read(cpu->mem, hd->addr));
}

static void handler_tya(Cpu* cpu, HandlerData* hd)
{
    cpu->A = cpu->Y;
    setZ(cpu, cpu->A);
    setN(cpu, cpu->A);
}

static void handler_brk(Cpu* cpu, HandlerData* hd)
{
    stack_push16(cpu, cpu->PC);
    handler_php(cpu, hd);
    handler_sei(cpu, hd);

    cpu->PC = read_16(cpu->mem, IRQ_BRK_VECTOR_ADDR);
}

static void handler_sre(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode sre");
}

static void handler_shx(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode shx");
}

static void handler_lax(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode lax");
}

static void handler_kil(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode kil");
}

static void handler_sax(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode sax");
}

static void handler_shy(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode shy");
}

static void handler_xaa(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode xaa");
}

static void handler_anc(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode anc");
}

static void handler_isc(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode isc");
}

static void handler_tas(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode tas");
}

static void handler_rra(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode rra");
}

static void handler_ahx(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode ahx");
}

static void handler_rla(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode rla");
}

static void handler_alr(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode alr");
}

static void handler_axs(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode axs");
}

static void handler_slo(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode slo");
}

static void handler_dcp(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode dcp");
}

static void handler_arr(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode arr");
}

static void handler_las(Cpu* cpu, HandlerData* hd)
{
    panic("illegal opcode las");
}

static opcode_handler opcode_handlers[] = {
    &handler_brk, &handler_ora, &handler_kil, &handler_slo, &handler_nop,
    &handler_ora, &handler_asl, &handler_slo, &handler_php, &handler_ora,
    &handler_asl, &handler_anc, &handler_nop, &handler_ora, &handler_asl,
    &handler_slo, &handler_bpl, &handler_ora, &handler_kil, &handler_slo,
    &handler_nop, &handler_ora, &handler_asl, &handler_slo, &handler_clc,
    &handler_ora, &handler_nop, &handler_slo, &handler_nop, &handler_ora,
    &handler_asl, &handler_slo, &handler_jsr, &handler_and, &handler_kil,
    &handler_rla, &handler_bit, &handler_and, &handler_rol, &handler_rla,
    &handler_plp, &handler_and, &handler_rol, &handler_anc, &handler_bit,
    &handler_and, &handler_rol, &handler_rla, &handler_bmi, &handler_and,
    &handler_kil, &handler_rla, &handler_nop, &handler_and, &handler_rol,
    &handler_rla, &handler_sec, &handler_and, &handler_nop, &handler_rla,
    &handler_nop, &handler_and, &handler_rol, &handler_rla, &handler_rti,
    &handler_eor, &handler_kil, &handler_sre, &handler_nop, &handler_eor,
    &handler_lsr, &handler_sre, &handler_pha, &handler_eor, &handler_lsr,
    &handler_alr, &handler_jmp, &handler_eor, &handler_lsr, &handler_sre,
    &handler_bvc, &handler_eor, &handler_kil, &handler_sre, &handler_nop,
    &handler_eor, &handler_lsr, &handler_sre, &handler_cli, &handler_eor,
    &handler_nop, &handler_sre, &handler_nop, &handler_eor, &handler_lsr,
    &handler_sre, &handler_rts, &handler_adc, &handler_kil, &handler_rra,
    &handler_nop, &handler_adc, &handler_ror, &handler_rra, &handler_pla,
    &handler_adc, &handler_ror, &handler_arr, &handler_jmp, &handler_adc,
    &handler_ror, &handler_rra, &handler_bvs, &handler_adc, &handler_kil,
    &handler_rra, &handler_nop, &handler_adc, &handler_ror, &handler_rra,
    &handler_sei, &handler_adc, &handler_nop, &handler_rra, &handler_nop,
    &handler_adc, &handler_ror, &handler_rra, &handler_nop, &handler_sta,
    &handler_nop, &handler_sax, &handler_sty, &handler_sta, &handler_stx,
    &handler_sax, &handler_dey, &handler_nop, &handler_txa, &handler_xaa,
    &handler_sty, &handler_sta, &handler_stx, &handler_sax, &handler_bcc,
    &handler_sta, &handler_kil, &handler_ahx, &handler_sty, &handler_sta,
    &handler_stx, &handler_sax, &handler_tya, &handler_sta, &handler_txs,
    &handler_tas, &handler_shy, &handler_sta, &handler_shx, &handler_ahx,
    &handler_ldy, &handler_lda, &handler_ldx, &handler_lax, &handler_ldy,
    &handler_lda, &handler_ldx, &handler_lax, &handler_tay, &handler_lda,
    &handler_tax, &handler_lax, &handler_ldy, &handler_lda, &handler_ldx,
    &handler_lax, &handler_bcs, &handler_lda, &handler_kil, &handler_lax,
    &handler_ldy, &handler_lda, &handler_ldx, &handler_lax, &handler_clv,
    &handler_lda, &handler_tsx, &handler_las, &handler_ldy, &handler_lda,
    &handler_ldx, &handler_lax, &handler_cpy, &handler_cmp, &handler_nop,
    &handler_dcp, &handler_cpy, &handler_cmp, &handler_dec, &handler_dcp,
    &handler_iny, &handler_cmp, &handler_dex, &handler_axs, &handler_cpy,
    &handler_cmp, &handler_dec, &handler_dcp, &handler_bne, &handler_cmp,
    &handler_kil, &handler_dcp, &handler_nop, &handler_cmp, &handler_dec,
    &handler_dcp, &handler_cld, &handler_cmp, &handler_nop, &handler_dcp,
    &handler_nop, &handler_cmp, &handler_dec, &handler_dcp, &handler_cpx,
    &handler_sbc, &handler_nop, &handler_isc, &handler_cpx, &handler_sbc,
    &handler_inc, &handler_isc, &handler_inx, &handler_sbc, &handler_nop,
    &handler_sbc, &handler_cpx, &handler_sbc, &handler_inc, &handler_isc,
    &handler_beq, &handler_sbc, &handler_kil, &handler_isc, &handler_nop,
    &handler_sbc, &handler_inc, &handler_isc, &handler_sed, &handler_sbc,
    &handler_nop, &handler_isc, &handler_nop, &handler_sbc, &handler_inc,
    &handler_isc};

Cpu* cpu_build(System* sys)
{
    Cpu* cpu = calloc_or_fail(sizeof(Cpu));
    cpu->mem = cpu_memory_build(sys);

    return cpu;
}

Cpu* cpu_standalone_build()
{
    Cpu* cpu = calloc_or_fail(sizeof(Cpu));
    cpu->mem = standalone_memory_build();

    return cpu;
}

void cpu_destroy(Cpu* cpu)
{
    memory_destroy(cpu->mem);
    free(cpu);
}

void cpu_reset(Cpu* cpu)
{
    cpu->PC     = read_16(cpu->mem, RESET_VECTOR_ADDR);
    cpu->SP     = 0xFDu;
    cpu->cycles = 0;

    cpu->I = 1;
    cpu->U = 1;
}

static void handle_nmi(Cpu* cpu)
{
    stack_push16(cpu, cpu->PC);
    handler_php(cpu, NULL);
    cpu->PC = read_16(cpu->mem, NMI_VECTOR_ADDR);
    cpu->I  = 1;
    cpu->cycles += 7;
}

static void handle_irq(Cpu* cpu)
{
    stack_push16(cpu, cpu->PC);
    handler_php(cpu, NULL);
    cpu->PC = read_16(cpu->mem, IRQ_BRK_VECTOR_ADDR);
    cpu->I  = 1;
    cpu->cycles += 7;
}

void cpu_trigger_nmi(Cpu* cpu) { cpu->interrupt = INT_NMI; }
void cpu_trigger_irq(Cpu* cpu)
{
    if (!cpu->I)
        cpu->interrupt = INT_IRQ;
}

uint64_t cpu_step(Cpu* cpu)
{
    if (cpu->stall > 0) {
        cpu->stall--;
        return 1;
    }

    uint64_t prev_cycles = cpu->cycles;

    if (cpu->interrupt == INT_NMI)
        handle_nmi(cpu);
    else if (cpu->interrupt == INT_IRQ)
        handle_irq(cpu);
    cpu->interrupt = 0;

    uint8_t opcode = memory_read(cpu->mem, cpu->PC);
    uint8_t amode  = instr_addr_mode[opcode];

    if (instr_size[opcode] == 0)
        panic("cpu_step: invalid opcode 0x%0x", opcode);

    uint16_t addr         = 0;
    int      page_crossed = 0;

    switch (amode) {
        case AMODE_ACCUMULATOR:
        case AMODE_IMPLIED:
            break;
        case AMODE_IMMEDIATE:
            addr = cpu->PC + 1;
            break;
        case AMODE_ABSOLUTE:
            addr = read_16(cpu->mem, cpu->PC + 1);
            break;
        case AMODE_ABSOLUTEX: {
            uint16_t op  = read_16(cpu->mem, cpu->PC + 1);
            addr         = op + cpu->X;
            page_crossed = different_page(addr, op);
            break;
        }
        case AMODE_ABSOLUTEY: {
            uint16_t op  = read_16(cpu->mem, cpu->PC + 1);
            addr         = op + cpu->Y;
            page_crossed = different_page(addr, op);
            break;
        }
        case AMODE_INDIRECT: {
            uint16_t op = read_16(cpu->mem, cpu->PC + 1);
            addr        = read_16_bug(cpu->mem, op);
            break;
        }
        case AMODE_XINDIRECT: {
            uint16_t op = read_16(cpu->mem, cpu->PC + 1);
            addr        = read_16_bug(cpu->mem, op + cpu->X);
            break;
        }
        case AMODE_INDIRECTY: {
            uint16_t op  = read_16(cpu->mem, cpu->PC + 1);
            addr         = read_16_bug(cpu->mem, op) + cpu->Y;
            page_crossed = different_page(addr, op);
            break;
        }
        case AMODE_RELATIVE: {
            uint16_t op = memory_read(cpu->mem, cpu->PC + 1);
            if (op < 0x80u)
                addr = cpu->PC + 2 + op;
            else
                addr = cpu->PC + 2 + op - 0x100u;
            break;
        }
        case AMODE_ZEROPAGE:
            addr = memory_read(cpu->mem, cpu->PC + 1);
            break;
        case AMODE_ZEROPAGEX:
            addr = memory_read(cpu->mem, cpu->PC + 1) + cpu->X;
            addr &= 0xff;
            break;
        case AMODE_ZEROPAGEY:
            addr = memory_read(cpu->mem, cpu->PC + 1) + cpu->Y;
            addr &= 0xff;
            break;
        default:
            panic("unknown addressing mode %u", amode);
    }

    cpu->PC += instr_size[opcode];
    cpu->cycles += instr_cycle[opcode];
    if (page_crossed)
        cpu->cycles += instr_page_cycle[opcode];

    HandlerData hd = {.addr = addr, .PC = cpu->PC, .mode = amode};
    opcode_handlers[opcode](cpu, &hd);
    return cpu->cycles - prev_cycles;
}

uint16_t cpu_next_instr_address(Cpu* cpu, uint16_t addr)
{
    uint8_t opcode = memory_read(cpu->mem, addr);
    uint8_t size   = instr_size[opcode];
    if (size == 0)
        panic("not a valid opcode 0x%02x", opcode);

    return addr + size;
}

const char* cpu_disassemble(Cpu* cpu, uint16_t addr)
{
#undef STR_SIZE
#define STR_SIZE                                                               \
    (6 /* addr */ + 2 /* space */ + 3 /* instr name */ + 1 /* space */ +       \
     32 /* ops */ + 1 /* terminator */)

    static char res[STR_SIZE];
    static char tmp[STR_SIZE];
    memset(res, 0, STR_SIZE);
    memset(tmp, 0, STR_SIZE);

    uint8_t     opcode = memory_read(cpu->mem, addr);
    uint8_t     size   = instr_size[opcode];
    uint8_t     amode  = instr_addr_mode[opcode];
    const char* name   = instr_name[opcode];

    if (size == 0)
        panic("not a valid opcode 0x%02x", opcode);

    sprintf(res, "0x%04x: %3s", addr, name);

    uint16_t op1 = 0, op2 = 0;
    if (size >= 1)
        op1 = memory_read(cpu->mem, addr + 1);
    if (size >= 2)
        op2 = memory_read(cpu->mem, addr + 2);

    switch (amode) {
        case AMODE_ACCUMULATOR:
            sprintf(tmp, " A");
            break;
        case AMODE_ABSOLUTE: {
            uint16_t abs = op1 | (op2 << 8);
            sprintf(tmp, " $%04x", abs);
            break;
        }
        case AMODE_ABSOLUTEX: {
            uint16_t abs = op1 | (op2 << 8);
            sprintf(tmp, " $%04x,x", abs);
            break;
        }
        case AMODE_ABSOLUTEY: {
            uint16_t abs = op1 | (op2 << 8);
            sprintf(tmp, " $%04x,y", abs);
            break;
        }
        case AMODE_IMMEDIATE:
            sprintf(tmp, " #$%02x", op1);
            break;
        case AMODE_IMPLIED:
            break;
        case AMODE_INDIRECT: {
            uint16_t v = op1 | (op2 << 8);
            sprintf(tmp, " ($%04x)", v);
            break;
        }
        case AMODE_XINDIRECT:
            sprintf(tmp, " ($%02x, x)", op1);
            break;
        case AMODE_INDIRECTY:
            sprintf(tmp, " ($%02x), y", op1);
            break;
        case AMODE_RELATIVE: {
            uint16_t v;
            if (op1 < 0x80)
                v = addr + 2 + op1;
            else
                v = addr + 2 + op1 - 0x100;

            sprintf(tmp, " $%04x", v);
            break;
        }
        case AMODE_ZEROPAGE:
            sprintf(tmp, " $%02x", op1);
            break;
        case AMODE_ZEROPAGEX:
            sprintf(tmp, " $%02x, x", op1);
            break;
        case AMODE_ZEROPAGEY:
            sprintf(tmp, " $%02x, y", op1);
            break;
        default:
            panic("unknown addressing mode %u", amode);
    }
    strcat(res, tmp);

    return (const char*)res;
}

const char* cpu_tostring(Cpu* cpu)
{
#undef STR_SIZE
#define STR_SIZE 256

    static char res[STR_SIZE];
    memset(res, 0, STR_SIZE);

    sprintf(res,
            "Flags: 0x%04x\n"
            "PC:    0x%04x\n"
            "SP:    0x%02x\n"
            "A:     0x%02x [ %u ]\n"
            "X:     0x%02x [ %u ]\n"
            "Y:     0x%02x [ %u ]\n",
            pack_flags(cpu), cpu->PC, cpu->SP, cpu->A, cpu->A, cpu->X, cpu->X,
            cpu->Y, cpu->Y);
    return res;
}
