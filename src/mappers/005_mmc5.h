#ifndef MMC5_H
#define MMC5_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct MMC5 {
    struct Cartridge* cart;
    uint8_t           ExRAM[1024];
    uint8_t           sprite_16_mode : 1;
    uint8_t           prg_mode : 2;
    uint8_t           chr_mode : 2;
    uint8_t           chr_A_mode : 1;
    uint8_t           read_only_ram : 1;
    uint8_t           exram_mode : 2;
    uint8_t           high_chr_reg : 2;
    uint8_t           irq_enable : 1;
    uint8_t           fill_attr : 2;
    uint8_t           fill_tile;
    uint16_t          chr_regs_A[8];
    uint16_t          chr_regs_B[8];
    uint8_t           prg_regs[5];
    uint8_t           scanline;
    uint8_t           irq_target;
} MMC5;

MMC5*   MMC5_build(struct Cartridge* cart);
uint8_t MMC5_read(void* _map, uint16_t addr);
void    MMC5_write(void* _map, uint16_t addr, uint8_t value);
void    MMC5_step(void* _map, struct System* sys);
void    MMC5_serialize(void* _map, FILE* fout);
void    MMC5_deserialize(void* _map, FILE* fin);

#endif
