#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

struct System;

typedef struct Memory {
    struct System* sys;
} Memory;

Memory* memory_build(struct System* sys);
void    memory_destroy(Memory* mem);

uint8_t memory_read(Memory* mem, uint16_t addr);
void    memory_write(Memory* mem, uint16_t addr, uint8_t value);

#endif
