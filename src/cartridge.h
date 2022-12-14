#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h>
#include <stdio.h>

struct Buffer;

typedef enum {
    MIRROR_HORIZONTAL = 0,
    MIRROR_VERTICAL   = 1,
    MIRROR_SINGLE0    = 2,
    MIRROR_SINGLE1    = 3,
    MIRROR_FOUR       = 4
} Mirroring;

typedef struct Cartridge {
    char*     fpath;
    char*     sav_path;
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

Cartridge* cartridge_load_from_buffer(struct Buffer raw);

Cartridge* cartridge_load(const char* path);
void       cartridge_unload(Cartridge* cart);

void cartridge_load_sav(Cartridge* cart);
void cartridge_save_sav(Cartridge* cart);

void cartridge_print(Cartridge* cart);

void cartridge_serialize(Cartridge* cart, FILE* ofile);
void cartridge_deserialize(Cartridge* cart, FILE* ifile);

#endif
