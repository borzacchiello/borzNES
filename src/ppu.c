#include "ppu.h"
#include "6502_cpu.h"
#include "system.h"
#include "memory.h"
#include "alloc.h"
#include "logging.h"
#include "game_window.h"
#include "mapper.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SKIP_BORDER_PIXELS 0
#define PRINT_PPU_STATE    0

#define IS_PIXEL_TRANSPARENT(p) ((p) % 4 == 0)

uint32_t palette_colors[] = {
    PACK_RGB(84, 84, 84),    // 0x00
    PACK_RGB(0, 30, 116),    // 0x01
    PACK_RGB(8, 16, 144),    // 0x02
    PACK_RGB(48, 0, 136),    // 0x03
    PACK_RGB(68, 0, 100),    // 0x04
    PACK_RGB(92, 0, 48),     // 0x05
    PACK_RGB(84, 4, 0),      // 0x06
    PACK_RGB(60, 24, 0),     // 0x07
    PACK_RGB(32, 42, 0),     // 0x08
    PACK_RGB(8, 58, 0),      // 0x09
    PACK_RGB(0, 64, 0),      // 0x0a
    PACK_RGB(0, 60, 0),      // 0x0b
    PACK_RGB(0, 50, 60),     // 0x0c
    PACK_RGB(0, 0, 0),       // 0x0d
    PACK_RGB(0, 0, 0),       // 0x0e
    PACK_RGB(0, 0, 0),       // 0x0f
    PACK_RGB(152, 150, 152), // 0x10
    PACK_RGB(8, 76, 196),    // 0x11
    PACK_RGB(48, 50, 236),   // 0x12
    PACK_RGB(92, 30, 228),   // 0x13
    PACK_RGB(136, 20, 176),  // 0x14
    PACK_RGB(160, 20, 100),  // 0x15
    PACK_RGB(152, 34, 32),   // 0x16
    PACK_RGB(120, 60, 0),    // 0x17
    PACK_RGB(84, 90, 0),     // 0x18
    PACK_RGB(40, 114, 0),    // 0x19
    PACK_RGB(8, 124, 0),     // 0x1a
    PACK_RGB(0, 118, 40),    // 0x1b
    PACK_RGB(0, 102, 120),   // 0x1c
    PACK_RGB(0, 0, 0),       // 0x1d
    PACK_RGB(0, 0, 0),       // 0x1e
    PACK_RGB(0, 0, 0),       // 0x1f
    PACK_RGB(236, 238, 236), // 0x20
    PACK_RGB(76, 154, 236),  // 0x21
    PACK_RGB(120, 124, 236), // 0x22
    PACK_RGB(176, 98, 236),  // 0x23
    PACK_RGB(228, 84, 236),  // 0x24
    PACK_RGB(236, 88, 180),  // 0x25
    PACK_RGB(236, 106, 100), // 0x26
    PACK_RGB(212, 136, 32),  // 0x27
    PACK_RGB(160, 170, 0),   // 0x28
    PACK_RGB(116, 196, 0),   // 0x29
    PACK_RGB(76, 208, 32),   // 0x2a
    PACK_RGB(56, 204, 108),  // 0x2b
    PACK_RGB(56, 180, 204),  // 0x2c
    PACK_RGB(60, 60, 60),    // 0x2d
    PACK_RGB(0, 0, 0),       // 0x2e
    PACK_RGB(0, 0, 0),       // 0x2f
    PACK_RGB(236, 238, 236), // 0x30
    PACK_RGB(168, 204, 236), // 0x31
    PACK_RGB(188, 188, 236), // 0x32
    PACK_RGB(212, 178, 236), // 0x33
    PACK_RGB(236, 174, 236), // 0x34
    PACK_RGB(236, 174, 212), // 0x35
    PACK_RGB(236, 180, 176), // 0x36
    PACK_RGB(228, 196, 144), // 0x37
    PACK_RGB(204, 210, 120), // 0x38
    PACK_RGB(180, 222, 120), // 0x39
    PACK_RGB(168, 226, 144), // 0x3a
    PACK_RGB(152, 226, 180), // 0x3b
    PACK_RGB(160, 214, 228), // 0x3c
    PACK_RGB(160, 162, 160), // 0x3d
    PACK_RGB(0, 0, 0),       // 0x3e
    PACK_RGB(0, 0, 0),       // 0x3f
};

Ppu* ppu_build(System* sys)
{
    Ppu* ppu = calloc_or_fail(sizeof(Ppu));
    ppu->sys = sys;
    ppu->gw  = NULL;
    ppu->mem = ppu_memory_build(sys);

    ppu_reset(ppu);
    return ppu;
}

void ppu_destroy(Ppu* ppu)
{
    memory_destroy(ppu->mem);
    free(ppu);
}

void ppu_set_game_window(Ppu* ppu, GameWindow* gw) { ppu->gw = gw; }

static void write_PPUCTRL(Ppu*, uint8_t);
static void write_PPUMASK(Ppu*, uint8_t);
static void write_OAMADDR(Ppu*, uint8_t);

void ppu_reset(Ppu* ppu)
{
    ppu->cycle    = 340;
    ppu->scanline = 240;
    ppu->frame    = 0;

    write_PPUCTRL(ppu, 0);
    write_PPUMASK(ppu, 0);
    write_OAMADDR(ppu, 0);
}

static void increment_x(Ppu* ppu)
{
    if ((ppu->v & 0x001F) == 31) { // if coarse X == 31
        ppu->v &= 0xFFE0;          // coarse X = 0
        ppu->v ^= 0x0400;          // switch horizontal nametable
    } else {
        ppu->v += 1; // increment coarse X
    }
}

static void increment_y(Ppu* ppu)
{
    if ((ppu->v & 0x7000) != (uint16_t)0x7000) { // if fine Y < 7
        ppu->v += 0x1000;                        // increment fine Y
        return;
    }

    ppu->v &= 0x8FFF;                    // fine Y = 0
    uint16_t y = (ppu->v & 0x03E0) >> 5; // let y = coarse Y
    if (y == 29) {
        y = 0;            // coarse Y = 0
        ppu->v ^= 0x0800; // switch vertical nametable
    } else if (y == 31) {
        y = 0; // coarse Y = 0, nametable not switched
    } else {
        y += 1; // increment coarse Y
    }
    ppu->v = (ppu->v & 0xFC1F) | (y << 5); // put coarse Y back into v
}

static void updated_nmi(Ppu* ppu)
{
    int trigger_nmi =
        (ppu->status_flags.in_vblank) && (ppu->ctrl_flags.trigger_nmi);
    if (trigger_nmi && !ppu->nmi_prev) {
        // Fixes Bomberman II
        ppu->nmi_delay = 13;
    }
    ppu->nmi_prev = trigger_nmi;
}

static void copy_x(Ppu* ppu)
{
    // v: .....F.. ...EDCBA = t: .....F.. ...EDCBA
    ppu->v = (ppu->v & 0xFBE0) | (ppu->t & 0x041F);
}

static void copy_y(Ppu* ppu)
{
    // v: .IHGF.ED CBA..... = t: .IHGF.ED CBA.....
    ppu->v = (ppu->v & 0x841F) | (ppu->t & 0x7BE0);
}

static void render_pixel(Ppu* ppu)
{
    int x = ppu->cycle - 1;
    int y = ppu->scanline;

    uint8_t bg_pixel = 0;
    if (ppu->mask_flags.show_background)
        bg_pixel =
            ((uint32_t)(ppu->tile_data >> 32) >> ((7 - ppu->x) * 4)) & 0x0F;

    uint8_t sprite_id    = 0;
    uint8_t sprite_pixel = 0;
    if (ppu->mask_flags.show_sprites) {
        for (uint8_t i = 0; i < ppu->sprite_count; ++i) {
            int off = ((int)ppu->cycle - 1) - ppu->sprites[i].position;
            if (off < 0 || off > 7)
                continue;
            off = 7 - off;

            uint8_t pixel = (ppu->sprites[i].pattern >> (off * 4)) & 0x0F;
            if (IS_PIXEL_TRANSPARENT(pixel))
                continue;

            sprite_pixel = pixel;
            sprite_id    = i;
            break;
        }
    }

    if (x < 8 && !ppu->mask_flags.show_left_background)
        bg_pixel = 0;
    if (x < 8 && !ppu->mask_flags.show_left_sprites)
        sprite_pixel = 0;

    uint8_t color = 0;
    if (!ppu->mask_flags.show_background && !ppu->mask_flags.show_sprites) {
        //  https://www.nesdev.org/wiki/PPU_palettes#The_background_palette_hack
        if (ppu->v >= 0x3F00 && ppu->v <= 0x3FFF)
            color = memory_read(ppu->mem, ppu->v);
    } else if (IS_PIXEL_TRANSPARENT(bg_pixel) &&
               IS_PIXEL_TRANSPARENT(sprite_pixel)) {
        color = 0;
    } else if (IS_PIXEL_TRANSPARENT(bg_pixel) &&
               !IS_PIXEL_TRANSPARENT(sprite_pixel)) {
        color = sprite_pixel | 0x10;
    } else if (!IS_PIXEL_TRANSPARENT(bg_pixel) &&
               IS_PIXEL_TRANSPARENT(sprite_pixel)) {
        color = bg_pixel;
    } else {
        /*
        Sprite 0 hit does not happen:
        1. If background or sprite rendering is disabled in PPUMASK ($2001)
        2. At x=0 to x=7 if the left-side clipping window is enabled (if bit 2
           or bit 1 of PPUMASK is 0).
        3. At x=255, for an obscure reason related to the pixel pipeline.
        4. At any pixel where the background or sprite pixel is transparent
           (2-bit color index from the CHR pattern is %00).
        5. If sprite 0 hit has already occurred this frame.
        */
        uint8_t must_set_zero_hit = ppu->sprites[sprite_id].index == 0;
        must_set_zero_hit =
            must_set_zero_hit &&
            (ppu->mask_flags.show_background && ppu->mask_flags.show_sprites);
        must_set_zero_hit =
            must_set_zero_hit &&
            !((x >= 0 && x <= 7) && (!ppu->mask_flags.show_left_sprites ||
                                     !ppu->mask_flags.show_left_background));
        must_set_zero_hit = must_set_zero_hit && x != 255;

        if (must_set_zero_hit)
            ppu->status_flags.sprite_zero_hit = 1;

        if (ppu->sprites[sprite_id].priority == 0)
            color = sprite_pixel | 0x10;
        else
            color = bg_pixel;
    }

#if SKIP_BORDER_PIXELS
    if (x < 10 || x >= 246)
        return;
    if (y < 10 || y >= 230)
        return;
#endif
    uint32_t rgb = palette_colors[memory_read(ppu->mem, 0x3F00u + color) % 64];
    if (ppu->gw)
        gamewindow_set_pixel(ppu->gw, x, y, rgb);
}

static void set_vertical_blank(Ppu* ppu)
{
    ppu->status_flags.in_vblank = 1;
    updated_nmi(ppu);
    if (ppu->gw)
        gamewindow_draw(ppu->gw);
}

static void clear_vertical_blank(Ppu* ppu)
{
    ppu->status_flags.in_vblank = 0;
    updated_nmi(ppu);
}

static uint32_t fetch_sprite_pattern(Ppu* ppu, uint8_t sprite_id, uint8_t row)
{
    uint8_t tile       = ppu->oam_data[sprite_id * 4 + 1];
    uint8_t attributes = ppu->oam_data[sprite_id * 4 + 2];

    uint16_t addr;
    if (ppu->ctrl_flags.sprite_size) {
        assert(row < 16 && "fetch_sprite_pattern(): invalid row");

        if (attributes & 0x80)
            row = 15 - row;

        uint8_t table = tile & 1;
        tile &= 0xFE;
        if (row > 7) {
            tile += 1;
            row -= 8;
        }
        addr = (uint16_t)0x1000 * table + tile * 16 + row;
    } else {
        assert(row < 8 && "fetch_sprite_pattern(): invalid row");

        if (attributes & 0x80)
            row = 7 - row;
        uint8_t table = ppu->ctrl_flags.sprite_table;
        addr = (uint16_t)0x1000 * table + (uint16_t)tile * 16 + (uint16_t)row;
    }

    uint8_t a              = (attributes & 3) << 2;
    uint8_t low_tile_byte  = memory_read(ppu->mem, addr);
    uint8_t high_tile_byte = memory_read(ppu->mem, addr + 8);

    uint32_t data = 0;
    for (uint8_t i = 0; i < 8; ++i) {
        uint8_t p1, p2;
        if (attributes & 0x40) {
            p1 = (low_tile_byte & 1);
            p2 = (high_tile_byte & 1) << 1;
            low_tile_byte >>= 1;
            high_tile_byte >>= 1;
        } else {
            p1 = (low_tile_byte & 0x80) >> 7;
            p2 = (high_tile_byte & 0x80) >> 6;
            low_tile_byte <<= 1;
            high_tile_byte <<= 1;
        }
        data <<= 4;
        data |= (uint32_t)(a | p1 | p2);
    }
    return data;
}

static void update_cycle(Ppu* ppu)
{
    if (ppu->nmi_delay > 0) {
        int must_trigger_nmi =
            (ppu->status_flags.in_vblank) && (ppu->ctrl_flags.trigger_nmi);
        ppu->nmi_delay--;
        if (ppu->nmi_delay == 0 && must_trigger_nmi)
            cpu_trigger_nmi(ppu->sys->cpu);
    }

    if (ppu->mask_flags.show_background || ppu->mask_flags.show_sprites) {
        if (ppu->f && ppu->scanline == 261 && ppu->cycle == 339) {
            ppu->cycle    = 0;
            ppu->scanline = 0;
            ppu->frame++;
            ppu->f = !ppu->f;
            return;
        }
    }

    ppu->cycle++;
    if (ppu->cycle > 340) {
        ppu->cycle = 0;
        ppu->scanline++;
        if (ppu->scanline > 261) {
            ppu->scanline = 0;
            ppu->frame++;
            ppu->f = !ppu->f;
        }
    }
}

void ppu_step(Ppu* ppu)
{
#if PRINT_PPU_STATE
    info("%s", ppu_tostring_short(ppu));
#endif

    update_cycle(ppu);

    int rendering_enabled =
        ppu->mask_flags.show_background || ppu->mask_flags.show_sprites;
    int pre_line        = ppu->scanline == 261;
    int visible_line    = ppu->scanline < 240;
    int render_line     = pre_line || visible_line;
    int pre_fetch_cycle = ppu->cycle >= 321 && ppu->cycle <= 336;
    int visible_cycle   = ppu->cycle >= 1 && ppu->cycle <= 256;
    int fetch_cycle     = pre_fetch_cycle || visible_cycle;

    // RENDERING
    if (visible_line && visible_cycle) {
        render_pixel(ppu);
    }

    // BACKGROUND
    if (rendering_enabled) {
        if (render_line && fetch_cycle) {
            mapper_notify_fetching(ppu->sys->mapper, ppu, FETCHING_BACKGROUND);

            ppu->tile_data <<= 4;
            switch (ppu->cycle % 8) {
                case 0: {
                    uint32_t data = 0;
                    for (uint8_t i = 0; i < 8; ++i) {
                        uint8_t a  = ppu->attribute_table_byte;
                        uint8_t p1 = (ppu->low_tile_byte & 0x80) >> 7;
                        uint8_t p2 = (ppu->high_tile_byte & 0x80) >> 6;
                        ppu->low_tile_byte <<= 1;
                        ppu->high_tile_byte <<= 1;
                        data <<= 4;
                        data |= (uint32_t)(a | p1 | p2);
                    }
                    ppu->tile_data |= data;
                    break;
                }
                case 1: {
                    uint16_t addr        = 0x2000 | (ppu->v & 0x0FFF);
                    ppu->name_table_byte = memory_read(ppu->mem, addr);
                    break;
                }
                case 3: {
                    uint16_t addr = 0x23C0 | (ppu->v & 0x0C00) |
                                    ((ppu->v >> 4) & 0x38) |
                                    ((ppu->v >> 2) & 0x07);
                    uint16_t shift = ((ppu->v >> 4) & 4) | (ppu->v & 2);
                    ppu->attribute_table_byte =
                        ((memory_read(ppu->mem, addr) >> shift) & 3) << 2;
                    break;
                }
                case 5: {
                    uint16_t fine_y = (ppu->v >> 12) & 7;
                    uint16_t addr   = ppu->name_table_byte * 16 + fine_y;
                    if (ppu->ctrl_flags.background_table)
                        addr += 0x1000;
                    ppu->low_tile_byte = memory_read(ppu->mem, addr);
                    break;
                }
                case 7: {
                    uint16_t fine_y = (ppu->v >> 12) & 7;
                    uint16_t addr   = ppu->name_table_byte * 16 + fine_y;
                    if (ppu->ctrl_flags.background_table)
                        addr += 0x1000;
                    ppu->high_tile_byte = memory_read(ppu->mem, addr + 8);
                    break;
                }
            }
        }
        if (pre_line && ppu->cycle >= 280 && ppu->cycle <= 304) {
            copy_y(ppu);
        }
        if (render_line) {
            if (fetch_cycle && ppu->cycle % 8 == 0) {
                increment_x(ppu);
            }
            if (ppu->cycle == 256) {
                increment_y(ppu);
            }
            if (ppu->cycle == 257) {
                copy_x(ppu);
            }
        }
    }

    // SPRITE
    if (rendering_enabled) {
        if (ppu->cycle == 257) {
            if (visible_line) {
                mapper_notify_fetching(ppu->sys->mapper, ppu, FETCHING_SPRITE);

                int32_t h = 8;
                if (ppu->ctrl_flags.sprite_size)
                    h = 16;

                int32_t count = 0;
                for (uint8_t i = 0; i < 64; ++i) {
                    uint8_t y   = ppu->oam_data[i * 4];
                    uint8_t a   = ppu->oam_data[i * 4 + 2];
                    uint8_t x   = ppu->oam_data[i * 4 + 3];
                    int32_t row = (int32_t)ppu->scanline - (int32_t)y;
                    if (row < 0 || row >= h)
                        continue;
                    if (count < MAX_SPRITES) {
                        ppu->sprites[count].pattern =
                            fetch_sprite_pattern(ppu, i, (uint8_t)row);
                        ppu->sprites[count].position = x;
                        ppu->sprites[count].priority = (a >> 5) & 1;
                        ppu->sprites[count].index    = i;
                    }
                    count += 1;
                }
                if (count > MAX_SPRITES) {
                    count                             = MAX_SPRITES;
                    ppu->status_flags.sprite_overflow = 1;
                }
                ppu->sprite_count = count;

            } else {
                ppu->sprite_count = 0;
            }
        }
    }

    // VBLANK
    if (ppu->scanline == 241 && ppu->cycle == 1) {
        set_vertical_blank(ppu);
    } else if (pre_line && ppu->cycle == 1) {
        clear_vertical_blank(ppu);
        ppu->status_flags.sprite_zero_hit = 0;
        ppu->status_flags.sprite_overflow = 0;
    }
}

static uint8_t read_PPUSTATUS(Ppu* ppu)
{
    /*
    7  bit  0
    ---- ----
    VSO. ....
    |||| ||||
    |||+-++++- PPU open bus. Returns stale PPU bus contents.
    ||+------- Sprite overflow. The intent was for this flag to be set
    ||         whenever more than eight sprites appear on a scanline, but a
    ||         hardware bug causes the actual behavior to be more complicated
    ||         and generate false positives as well as false negatives; see
    ||         PPU sprite evaluation. This flag is set during sprite
    ||         evaluation and cleared at dot 1 (the second dot) of the
    ||         pre-render line.
    |+-------- Sprite 0 Hit.  Set when a nonzero pixel of sprite 0 overlaps
    |          a nonzero background pixel; cleared at dot 1 of the pre-render
    |          line.  Used for raster timing.
    +--------- Vertical blank has started (0: not in vblank; 1: in vblank).
            Set at dot 1 of line 241 (the line *after* the post-render
            line); cleared after reading $2002 and at dot 1 of the
            pre-render line.
     */

    uint8_t res = ppu->bus_content & 0x1F;
    if (ppu->status_flags.sprite_overflow)
        res |= (1u << 5);
    if (ppu->status_flags.sprite_zero_hit)
        res |= (1u << 6);
    if (ppu->status_flags.in_vblank)
        res |= (1u << 7);
    ppu->status_flags.in_vblank = 0;
    ppu->w                      = 0;

    updated_nmi(ppu);
    return res;
}

static uint8_t read_OAMDATA(Ppu* ppu)
{
    uint8_t res = ppu->oam_data[ppu->oam_addr];
    // if ((ppu->oam_addr & 3) == 2)
    //     res &= 0xE3;
    return res;
}

static uint8_t read_PPUDATA(Ppu* ppu)
{
    uint8_t res = memory_read(ppu->mem, ppu->v);
    if (ppu->v % 0x4000 < 0x3F00) {
        // When reading while the VRAM address is in the range 0-$3EFF (i.e.,
        // before the palettes), the read will return the contents of an
        // internal read buffer. This internal buffer is updated only when
        // reading PPUDATA, and so is preserved across frames. After the CPU
        // reads and gets the contents of the internal buffer, the PPU will
        // immediately update the internal buffer with the byte at the current
        // VRAM address.
        uint8_t buffered      = ppu->buffered_ppudata;
        ppu->buffered_ppudata = res;
        res                   = buffered;
    } else {
        // Reading palette data from $3F00-$3FFF works differently. The palette
        // data is placed immediately on the data bus, and hence no priming read
        // is required. Reading the palettes still updates the internal buffer
        // though, but the data placed in it is the mirrored nametable data that
        // would appear "underneath" the palette
        ppu->buffered_ppudata = memory_read(ppu->mem, ppu->v - 0x1000u);
    }

    if (ppu->ctrl_flags.increment)
        ppu->v += 32u;
    else
        ppu->v += 1u;
    return res;
}

static void write_PPUCTRL(Ppu* ppu, uint8_t value)
{
    /*
    7  bit  0
    ---- ----
    VPHB SINN
    |||| ||||
    |||| ||++- Base nametable address
    |||| ||    (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
    |||| |+--- VRAM address increment per CPU read/write of PPUDATA
    |||| |     (0: add 1, going across; 1: add 32, going down)
    |||| +---- Sprite pattern table address for 8x8 sprites
    ||||       (0: $0000; 1: $1000; ignored in 8x16 mode)
    |||+------ Background pattern table address (0: $0000; 1: $1000)
    ||+------- Sprite size (0: 8x8 pixels; 1: 8x16 pixels – see PPU OAM#Byte 1)
    |+-------- PPU master/slave select
    |          (0: read backdrop from EXT pins; 1: output color on EXT pins)
    +--------- Generate an NMI at the start of the
            vertical blanking interval (0: off; 1: on)
    */

    ppu->ctrl_flags.name_table       = value & 3;
    ppu->ctrl_flags.increment        = (value >> 2) & 1;
    ppu->ctrl_flags.sprite_table     = (value >> 3) & 1;
    ppu->ctrl_flags.background_table = (value >> 4) & 1;
    ppu->ctrl_flags.sprite_size      = (value >> 5) & 1;
    ppu->ctrl_flags.master_slave     = (value >> 6) & 1;
    ppu->ctrl_flags.trigger_nmi      = (value >> 7) & 1;

    updated_nmi(ppu);

    // t: ....BA.. ........ = d: ......BA
    ppu->t &= (uint16_t)0b1111001111111111;
    ppu->t |= ((uint16_t)value & (uint16_t)0b11) << 10;
}

static void write_PPUMASK(Ppu* ppu, uint8_t value)
{
    /*
    7  bit  0
    ---- ----
    BGRs bMmG
    |||| ||||
    |||| |||+- Greyscale (0: normal color, 1: produce a greyscale display)
    |||| ||+-- 1: Show background in leftmost 8 pixels of screen, 0: Hide
    |||| |+--- 1: Show sprites in leftmost 8 pixels of screen, 0: Hide
    |||| +---- 1: Show background
    |||+------ 1: Show sprites
    ||+------- Emphasize red (green on PAL/Dendy)
    |+-------- Emphasize green (red on PAL/Dendy)
    +--------- Emphasize blue
    */

    ppu->mask_flags.grayscale            = value & 1;
    ppu->mask_flags.show_left_background = (value >> 1) & 1;
    ppu->mask_flags.show_left_sprites    = (value >> 2) & 1;
    ppu->mask_flags.show_background      = (value >> 3) & 1;
    ppu->mask_flags.show_sprites         = (value >> 4) & 1;
    ppu->mask_flags.red_tint             = (value >> 5) & 1;
    ppu->mask_flags.green_tint           = (value >> 6) & 1;
    ppu->mask_flags.blue_tint            = (value >> 7) & 1;
}

static void write_OAMADDR(Ppu* ppu, uint8_t value) { ppu->oam_addr = value; }

static void write_OAMDATA(Ppu* ppu, uint8_t value)
{
    ppu->oam_data[ppu->oam_addr++] = value;
}

static void write_PPUSCROLL(Ppu* ppu, uint8_t value)
{
    if (ppu->w) {
        // t: .CBA..HG FED..... = d: HGFEDCBA
        // w:                   = 0
        ppu->t = (ppu->t & 0x8FFF) | (((uint16_t)value & 0x07) << 12);
        ppu->t = (ppu->t & 0xFC1F) | (((uint16_t)value & 0xF8) << 2);
        ppu->w = 0;
    } else {
        // t: ........ ...HGFED = d: HGFED...
        // x:               CBA = d: .....CBA
        // w:                   = 1
        ppu->t = (ppu->t & 0xFFE0) | ((uint16_t)value >> 3);
        ppu->x = value & 0x07;
        ppu->w = 1;
    }
}

static void write_PPUADDR(Ppu* ppu, uint8_t value)
{
    if (ppu->w) {
        // t: ........ HGFEDCBA = d: HGFEDCBA
        // v                    = t
        // w:                   = 0
        ppu->t &= (uint16_t)0b1111111100000000;
        ppu->t |= (uint16_t)value;
        ppu->v = ppu->t;
        ppu->w = 0;
    } else {
        // t: ..FEDCBA ........ = d: ..FEDCBA
        // t: .X...... ........ = 0
        // w:                   = 1
        ppu->t &= 0x80FFu;
        ppu->t |= (value & 0x3Fu) << 8;
        ppu->w = 1;
    }
}

static void write_PPUDATA(Ppu* ppu, uint8_t value)
{
    memory_write(ppu->mem, ppu->v, value);

    if (ppu->ctrl_flags.increment)
        ppu->v += 32;
    else
        ppu->v += 1;
}

static void write_OAMDMA(Ppu* ppu, uint8_t value)
{
    Cpu* cpu = ppu->sys->cpu;

    uint16_t addr = (uint16_t)value << 8;
    for (int i = 0; i < 256; ++i) {
        ppu->oam_data[ppu->oam_addr] = memory_read(cpu->mem, addr);
        ppu->oam_addr++;
        addr++;
    }

    cpu->stall += 513u;
    if (cpu->cycles % 2 == 1)
        cpu->stall++;
}

uint8_t ppu_read_register(Ppu* ppu, uint16_t addr)
{
    if (addr == 0x2002) {
        return read_PPUSTATUS(ppu);
    }
    if (addr == 0x2004) {
        return read_OAMDATA(ppu);
    }
    if (addr == 0x2007) {
        return read_PPUDATA(ppu);
    }

    warning("ppu: invalid read address: 0x%04x", addr);
    return 0;
}

void ppu_write_register(Ppu* ppu, uint16_t addr, uint8_t value)
{
    ppu->bus_content = value;

    if (addr == 0x2000) {
        write_PPUCTRL(ppu, value);
        return;
    }
    if (addr == 0x2001) {
        write_PPUMASK(ppu, value);
        return;
    }
    if (addr == 0x2003) {
        write_OAMADDR(ppu, value);
        return;
    }
    if (addr == 0x2004) {
        write_OAMDATA(ppu, value);
        return;
    }
    if (addr == 0x2005) {
        write_PPUSCROLL(ppu, value);
        return;
    }
    if (addr == 0x2006) {
        write_PPUADDR(ppu, value);
        return;
    }
    if (addr == 0x2007) {
        write_PPUDATA(ppu, value);
        return;
    }
    if (addr == 0x4014) {
        write_OAMDMA(ppu, value);
        return;
    }

    warning("ppu: invalid write to 0x%04x [0x%02x]", addr, value);
}

const char* ppu_tostring(Ppu* ppu)
{
    static char res[256];
    memset(res, 0, sizeof(res));

    sprintf(res,
            "PPU_F:    %u\n"
            "PPU_SL:   %u\n"
            "PPU_C:    %u\n"
            "PPU_ADDR: 0x%04x\n",
            ppu->frame, ppu->scanline, ppu->cycle, ppu->v);
    return res;
}

const char* ppu_tostring_short(Ppu* ppu)
{
    static char res[256];
    memset(res, 0, sizeof(res));

    sprintf(res,
            "PPU: SL=%03u CYC=%03u DL=%02d PREV_NMI=%d NMI_OUT=%d V=%04x "
            "T=%04x SB=%d SS=%d VB=%d SZH=%d X=%03u TD=%016llx LTB=%02x "
            "HTB=%02x NTB=%02x",
            ppu->scanline, ppu->cycle, ppu->nmi_delay, ppu->nmi_prev,
            ppu->ctrl_flags.trigger_nmi, ppu->v, ppu->t,
            ppu->mask_flags.show_background, ppu->mask_flags.show_sprites,
            ppu->status_flags.in_vblank, ppu->status_flags.sprite_zero_hit,
            ppu->x, (unsigned long long)ppu->tile_data, ppu->low_tile_byte,
            ppu->high_tile_byte, ppu->name_table_byte);
    return res;
}

void ppu_serialize(Ppu* ppu, FILE* ofile)
{
    Buffer res = {.buffer = (uint8_t*)ppu, .size = sizeof(Ppu)};
    dump_buffer(&res, ofile);
}

void ppu_deserialize(Ppu* ppu, FILE* ifile)
{
    Buffer buf = read_buffer(ifile);
    if (buf.size != sizeof(Ppu))
        panic("ppu_deserialize(): invalid buffer");

    void* tmp_sys = ppu->sys;
    void* tmp_mem = ppu->mem;
    void* tmp_gw  = ppu->gw;

    memcpy(ppu, buf.buffer, buf.size);
    ppu->sys = tmp_sys;
    ppu->mem = tmp_mem;
    ppu->gw  = tmp_gw;
    free(buf.buffer);
}
