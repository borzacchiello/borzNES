#ifndef PPU_H
#define PPU_H

#include <stdint.h>

struct System;
struct Memory;
typedef struct Ppu {
    struct Memory* mem;

    uint16_t v; // current VRAM address
    uint16_t t; // temporary VRAM address
    uint8_t  x; // fine X scroll
    uint8_t  w; // write toggle
    uint8_t  f; // even/odd frame flag

    uint32_t frame;
    uint16_t cycle;    // 0-340
    uint16_t scanline; // 0-261
} Ppu;

Ppu* ppu_build(struct System* sys);
void ppu_destroy(Ppu* ppu);

void ppu_reset(Ppu* ppu);

#endif
