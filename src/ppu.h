#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdio.h>

#define MAX_SPRITES 8

struct GameWindow;
struct System;
struct Memory;
struct Buffer;

typedef struct PpuStatusFlags {
    union {
        struct {
            uint8_t sprite_overflow : 1;
            uint8_t sprite_zero_hit : 1;
            uint8_t in_vblank : 1;
        };
        uint8_t flags;
    };
} PpuStatusFlags;

typedef struct PpuCtrlFlags {
    union {
        struct {
            uint8_t name_table : 3;
            uint8_t increment : 1;
            uint8_t sprite_table : 1;
            uint8_t background_table : 1;
            uint8_t sprite_size : 1;
            uint8_t master_slave : 1;
            uint8_t trigger_nmi : 1;
        };
        uint8_t flags;
    };
} PpuCtrlFlags;

typedef struct PpuMaskFlags {
    union {
        struct {
            uint8_t grayscale : 1;
            uint8_t show_left_background : 1;
            uint8_t show_left_sprites : 1;
            uint8_t show_background : 1;
            uint8_t show_sprites : 1;
            uint8_t red_tint : 1;
            uint8_t green_tint : 1;
            uint8_t blue_tint : 1;
        };
        uint8_t flags;
    };
} PpuMaskFlags;

typedef struct Sprite {
    uint32_t pattern;
    uint8_t  position;
    uint8_t  priority;
    uint8_t  index;
} Sprite;

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

    uint8_t sprite_count;
    Sprite  sprites[MAX_SPRITES];

    int32_t nmi_prev;
    int32_t nmi_delay;

    PpuStatusFlags status_flags;
    PpuCtrlFlags   ctrl_flags;
    PpuMaskFlags   mask_flags;

    uint8_t bus_content;
    uint8_t buffered_ppudata;
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

void ppu_serialize(Ppu* ppu, FILE* ofile);
void ppu_deserialize(Ppu* ppu, FILE* ifile);

#endif
