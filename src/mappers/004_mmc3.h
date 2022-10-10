#ifndef MMC3_H
#define MMC3_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct MMC3 {
    struct Cartridge* cart;
    uint8_t           reg;
    uint8_t           regs[8];
    uint8_t           prg_mode;
    uint8_t           chr_mode;
    int32_t           prg_offsets[4];
    int32_t           chr_offsets[8];
    uint8_t           reload;
    uint8_t           irq_counter;
    uint8_t           irq_enable;
} MMC3;

MMC3*   MMC3_build(struct Cartridge* cart);
uint8_t MMC3_read(void* _map, uint16_t addr);
void    MMC3_write(void* _map, uint16_t addr, uint8_t value);
void    MMC3_step(void* _map, struct System* sys);
void    MMC3_serialize(void* _map, FILE* fout);
void    MMC3_deserialize(void* _map, FILE* fin);

#endif
