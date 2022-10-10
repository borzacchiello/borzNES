#ifndef CNROM_H
#define CNROM_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct CNROM {
    struct Cartridge* cart;
    int32_t           chr_bank;
} CNROM;

CNROM*  CNROM_build(struct Cartridge* cart);
uint8_t CNROM_read(void* _map, uint16_t addr);
void    CNROM_write(void* _map, uint16_t addr, uint8_t value);
void    CNROM_serialize(void* _map, FILE* fout);
void    CNROM_deserialize(void* _map, FILE* fin);

#endif
