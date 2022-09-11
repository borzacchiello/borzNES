#ifndef PPU_H
#define PPU_H

#include <stdint.h>

struct GameWindow;
struct System;
struct Memory;
typedef struct Ppu {
    struct Memory*     mem;
    struct System*     sys;
    struct GameWindow* gw;

    uint8_t palette_data[32];
    uint8_t nametable_data[2048];
    uint8_t oam_data[256];

    uint8_t oam_addr;

    uint16_t v; // current VRAM address
    uint16_t t; // temporary VRAM address
    uint8_t  x; // fine X scroll
    uint8_t  w; // write toggle
    uint8_t  f; // even/odd frame flag

    uint8_t  name_table_byte;
    uint8_t  attribute_table_byte;
    uint8_t  low_tile_byte;
    uint8_t  high_tile_byte;
    uint64_t tile_data;

    uint32_t frame;
    uint16_t cycle;    // 0-340
    uint16_t scanline; // 0-261

    uint8_t  sprite_count;
    uint32_t sprite_patterns[8];
    uint8_t  sprite_positions[8];
    uint8_t  sprite_priorities[8];
    uint8_t  sprite_indexes[8];

    int32_t nmi_prev;
    int32_t nmi_delay;

    uint32_t flags;
    uint8_t  bus_content;
    uint8_t  buffered_ppudata;
} Ppu;

extern uint32_t palette_colors[64];

Ppu* ppu_build(struct System* sys);
void ppu_destroy(Ppu* ppu);

void    ppu_set_game_window(Ppu* ppu, struct GameWindow* gw);
void    ppu_step(Ppu* ppu);
uint8_t ppu_read_register(Ppu* ppu, uint16_t addr);
void    ppu_write_register(Ppu* ppu, uint16_t addr, uint8_t value);

void ppu_reset(Ppu* ppu);

const char* ppu_tostring(Ppu* ppu);
const char* ppu_tostring_short(Ppu* ppu);

#endif
