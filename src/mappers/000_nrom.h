#ifndef NROM_H
#define NROM_H

#include <stdio.h>
#include <stdint.h>

struct Cartridge;
struct System;

typedef struct NROM {
    struct Cartridge* cart;
    int               prg_banks, prg_bank1, prg_bank2;
} NROM;

NROM*   NROM_build(struct Cartridge* cart);
uint8_t NROM_read(void* _map, uint16_t addr);
void    NROM_write(void* _map, uint16_t addr, uint8_t value);
void    NROM_serialize(void* _map, FILE* fout);
void    NROM_deserialize(void* _map, FILE* fin);

#endif
