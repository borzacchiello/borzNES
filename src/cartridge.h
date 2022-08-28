#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>

typedef enum { VERTICAL, HORIZONTAL } Mirroring;

typedef struct Cartridge {
    char*     fpath;
    uint32_t  PRG_size;
    uint32_t  CHR_size;
    uint32_t  SRAM_size;
    uint8_t*  PRG;
    uint8_t*  CHR;
    uint8_t*  SRAM;
    uint8_t*  trainer;
    uint16_t  mapper;
    Mirroring mirror;
    uint8_t   battery;
} Cartridge;

Cartridge* cartridge_load(const char* path);
void       cartridge_unload(Cartridge* cart);

void cartridge_print(Cartridge* cart);

#endif
