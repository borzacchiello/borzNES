#ifndef MMC2_H
#define MMC2_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct MMC2 {
    struct Cartridge* cart;
    uint8_t           prg_bank;
    uint8_t           chr_r1, chr_r2, chr_r3, chr_r4;
    uint8_t           latch_0, latch_1;
} MMC2;

MMC2*   MMC2_build(struct Cartridge* cart);
uint8_t MMC2_read(void* _map, uint16_t addr);
void    MMC2_write(void* _map, uint16_t addr, uint8_t value);
void    MMC2_serialize(void* _map, FILE* fout);
void    MMC2_deserialize(void* _map, FILE* fin);

#endif
