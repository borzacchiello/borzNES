#ifndef AXROM_H
#define AXROM_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct AxROM {
    struct Cartridge* cart;
    int32_t           prg_bank;
} AxROM;

AxROM*  AxROM_build(struct Cartridge* cart);
uint8_t AxROM_read(void* _map, uint16_t addr);
void    AxROM_write(void* _map, uint16_t addr, uint8_t value);
void    AxROM_serialize(void* _map, FILE* fout);
void    AxROM_deserialize(void* _map, FILE* fin);

#endif
