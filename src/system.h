#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

struct Cpu;
struct Ppu;
struct Cartridge;
struct Mapper;

typedef struct System {
    struct Cpu*       cpu;
    struct Ppu*       ppu;
    struct Cartridge* cart;
    struct Mapper*    mapper;
    uint8_t           RAM[2048];
} System;

System* system_build(const char* rom_path);
void    system_destroy(System* sys);

// It returns a local buffer that will be invalidated
// if the function is called again
const char* system_tostring(System* sys);

#endif
