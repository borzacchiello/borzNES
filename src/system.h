#ifndef SYSTEM_H
#define SYSTEM_H

#include "cartridge.h"
#include "6502_cpu.h"
#include "mapper.h"

#include <stdint.h>

typedef struct System {
    Cpu*       cpu;
    Cartridge* cart;
    Mapper*    mapper;
    uint8_t    RAM[2048];
} System;

System* system_build(const char* rom_path);
void    system_destroy(System* sys);

#endif
