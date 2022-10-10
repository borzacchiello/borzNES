#ifndef CAMERICA_H
#define CAMERICA_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct Map071 {
    struct Cartridge* cart;
    int32_t           prg_bank;
} Map071;

Map071* Map071_build(struct Cartridge* cart);
uint8_t Map071_read(void* _map, uint16_t addr);
void    Map071_write(void* _map, uint16_t addr, uint8_t value);
void    Map071_serialize(void* _map, FILE* fout);
void    Map071_deserialize(void* _map, FILE* fin);

#endif
