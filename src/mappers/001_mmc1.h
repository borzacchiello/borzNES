#ifndef MMC1_H
#define MMC1_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct MMC1 {
    struct Cartridge* cart;
    uint8_t           shift_reg;
    uint8_t           control;
    uint8_t           prg_mode;
    uint8_t           chr_mode;
    uint8_t           prg_bank;
    uint8_t           chr_bank0;
    uint8_t           chr_bank1;
    int32_t           prg_offsets[2];
    int32_t           chr_offsets[2];
} MMC1;

MMC1*   MMC1_build(struct Cartridge* cart);
uint8_t MMC1_read(void* _map, uint16_t addr);
void    MMC1_write(void* _map, uint16_t addr, uint8_t value);
void    MMC1_serialize(void* _map, FILE* fout);
void    MMC1_deserialize(void* _map, FILE* fin);

#endif
