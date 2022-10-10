#ifndef FC001_H
#define FC001_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct FC_001 {
    struct Cartridge* cart;
    uint8_t           laststrobe, trigger;
    uint8_t           reg0, reg1, reg2, reg3;
    int32_t           bank_chr[2];
    int32_t           bank_prg;
} FC_001;

FC_001* FC_001_build(struct Cartridge* cart);
uint8_t FC_001_read(void* _map, uint16_t addr);
void    FC_001_write(void* _map, uint16_t addr, uint8_t value);
void    FC_001_step(void* _map, struct System* sys);
void    FC_001_serialize(void* _map, FILE* fout);
void    FC_001_deserialize(void* _map, FILE* fin);

#endif
