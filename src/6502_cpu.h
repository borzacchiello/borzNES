#ifndef CPU_H
#define CPU_H

#include <stdint.h>

struct Memory;
struct System;

typedef struct Cpu {
    struct Memory* mem;
    uint16_t       PC; // program counter
    uint8_t        SP; // stack pointer
    // 8bit regs
    uint8_t A, X, Y;
    // flags
    uint8_t C; // carry
    uint8_t Z; // zero
    uint8_t I; // interrupt disable
    uint8_t D; // decimal mode
    uint8_t B; // break command
    uint8_t U; // unused
    uint8_t V; // overflow
    uint8_t N; // negative
    // utils
    uint64_t cycles;
    uint8_t  interrupt;
    uint32_t stall;
} Cpu;

Cpu* cpu_build(struct System* sys);
Cpu* cpu_standalone_build();
void cpu_destroy(Cpu* cpu);

void     cpu_reset(Cpu* cpu);
uint64_t cpu_step(Cpu* cpu);
void     cpu_trigger_nmi(Cpu* cpu);
void     cpu_trigger_irq(Cpu* cpu);

uint16_t cpu_next_instr_address(Cpu* cpu, uint16_t addr);

// NB: the following functions will return a temporary buffer, every new call to
// them will invalidate old buffers
const char* cpu_disassemble(Cpu* cpu, uint16_t addr);
const char* cpu_state(Cpu* cpu);

#endif
