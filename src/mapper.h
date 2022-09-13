#ifndef MAPPERS_H
#define MAPPERS_H

#include <stdint.h>
#include <stdio.h>

struct Cartridge;
struct System;
struct Buffer;

typedef struct Mapper {
    void*       obj;
    const char* name;
    void (*step)(void* map, struct System* ppu);
    void (*destroy)(void* map);
    uint8_t (*read)(void* map, uint16_t addr);
    void (*write)(void* map, uint16_t addr, uint8_t value);
    void (*serialize)(void* map, FILE* fout);
    void (*deserialize)(void* map, FILE* fin);
} Mapper;

Mapper* mapper_build(struct Cartridge* cart);
void    mapper_destroy(Mapper* map);

uint8_t mapper_read(Mapper* map, uint16_t addr);
void    mapper_write(Mapper* map, uint16_t addr, uint8_t value);
void    mapper_step(Mapper* map, struct System* ppu);
void    mapper_serialize(Mapper* map, FILE* fout);
void    mapper_deserialize(Mapper* map, FILE* fin);

#endif
