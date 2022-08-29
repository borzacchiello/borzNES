#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

struct System;

typedef struct Memory {
    void* obj;
    void (*destroy)(void*);
    uint8_t (*read)(void*, uint16_t);
    void (*write)(void*, uint16_t, uint8_t);
} Memory;

Memory* cpu_memory_build(struct System* sys);
Memory* ppu_memory_build(struct System* sys);
Memory* standalone_memory_build();

void    memory_destroy(Memory* mem);
uint8_t memory_read(Memory* mem, uint16_t addr);
void    memory_write(Memory* mem, uint16_t addr, uint8_t value);

#endif
