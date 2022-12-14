#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdio.h>

struct Memory;
struct System;
struct Buffer;

typedef struct Cpu {
    struct System* sys;
    struct Memory* mem;
    uint16_t       PC; // program counter
    uint8_t        SP; // stack pointer

    // 8bit regs
    uint8_t A, X, Y;

    // flags
    union {
        struct {
            uint8_t C : 1; // carry
            uint8_t Z : 1; // zero
            uint8_t I : 1; // interrupt disable
            uint8_t D : 1; // decimal mode
            uint8_t B : 1; // break command
            uint8_t U : 1; // unused
            uint8_t V : 1; // overflow
            uint8_t N : 1; // negative
        };
        uint8_t flags;
    };

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
const char* cpu_tostring(Cpu* cpu);
const char* cpu_tostring_short(Cpu* cpu);

void cpu_serialize(Cpu* cpu, FILE* ofile);
void cpu_deserialize(Cpu* cpu, FILE* ifile);

#endif
