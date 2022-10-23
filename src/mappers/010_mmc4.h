#ifndef MMC4_H
#define MMC4_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct MMC4 {
    struct Cartridge* cart;
    uint8_t           prg_bank;
    uint8_t           chr_r1, chr_r2, chr_r3, chr_r4;
    uint8_t           latch_0, latch_1;
} MMC4;

MMC4*   MMC4_build(struct Cartridge* cart);
uint8_t MMC4_read(void* _map, uint16_t addr);
void    MMC4_write(void* _map, uint16_t addr, uint8_t value);
void    MMC4_serialize(void* _map, FILE* fout);
void    MMC4_deserialize(void* _map, FILE* fin);

#endif
