#ifndef MAPPERS_H
#define MAPPERS_H

#include <stdint.h>
#include <stdio.h>

struct Cartridge;
struct System;
struct Buffer;
struct Ppu;

typedef enum FetchingTarget {
    FETCHING_BACKGROUND = 0,
    FETCHING_SPRITE     = 1
} FetchingTarget;

typedef struct Mapper {
    void*       obj;
    const char* name;
    void (*step)(void* map, struct System* sys);
    void (*destroy)(void* map);
    uint8_t (*read)(void* map, uint16_t addr);
    void (*write)(void* map, uint16_t addr, uint8_t value);
    uint8_t (*nametable_read)(void* map, struct Ppu* ppu, uint16_t addr);
    void (*nametable_write)(void* map, struct Ppu* ppu, uint16_t addr,
                            uint8_t value);
    void (*notify_fetching)(void* map, struct Ppu* ppu, FetchingTarget ft);
    void (*serialize)(void* map, FILE* fout);
    void (*deserialize)(void* map, FILE* fin);
} Mapper;

Mapper* mapper_build(struct Cartridge* cart);
void    mapper_destroy(Mapper* map);

void    mapper_notify_fetching(Mapper* map, struct Ppu* ppu, FetchingTarget ft);
uint8_t mapper_nametable_read(Mapper* map, struct Ppu* ppu, uint16_t addr);
void    mapper_nametable_write(Mapper* map, struct Ppu* ppu, uint16_t addr,
                               uint8_t value);
uint8_t mapper_read(Mapper* map, uint16_t addr);
void    mapper_write(Mapper* map, uint16_t addr, uint8_t value);
void    mapper_step(Mapper* map, struct System* sys);
void    mapper_serialize(Mapper* map, FILE* fout);
void    mapper_deserialize(Mapper* map, FILE* fin);

#endif
