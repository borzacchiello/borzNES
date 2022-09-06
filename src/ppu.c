#include "ppu.h"
#include "6502_cpu.h"
#include "system.h"
#include "memory.h"
#include "alloc.h"
#include "logging.h"
#include "game_window.h"

#include <stdio.h>
#include <string.h>

#define PRINT_PPU_STATE 0

#define MASK_NAME_TABLE           (uint32_t)(3u)
#define FLAG_SPRITE_OVERFLOW      (uint32_t)(1u << 2)
#define FLAG_SPRITE_HIT_ZERO      (uint32_t)(1u << 3)
#define FLAG_IN_VBLANK            (uint32_t)(1u << 4)
#define FLAG_INCREMENT            (uint32_t)(1u << 5)
#define FLAG_SPRITE_TABLE         (uint32_t)(1u << 6)
#define FLAG_BACKGROUND_TABLE     (uint32_t)(1u << 7)
#define FLAG_SPRITE_SIZE          (uint32_t)(1u << 8)
#define FLAG_MASTER_SLAVE         (uint32_t)(1u << 9)
#define FLAG_GRAYSCALE            (uint32_t)(1u << 10)
#define FLAG_SHOW_LEFT_BACKGROUND (uint32_t)(1u << 11)
#define FLAG_SHOW_LEFT_SPRITES    (uint32_t)(1u << 12)
#define FLAG_SHOW_BACKGROUND      (uint32_t)(1u << 13)
#define FLAG_SHOW_SPRITES         (uint32_t)(1u << 14)
#define FLAG_RED_TINT             (uint32_t)(1u << 15)
#define FLAG_GREEN_TINT           (uint32_t)(1u << 16)
#define FLAG_BLUE_TINT            (uint32_t)(1u << 17)
#define FLAG_TRIGGER_NMI          (uint32_t)(1u << 18)

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
    if ((ppu->v & (uint16_t)0x001F) == 31u) { // if coarse X == 31
        ppu->v &= ~(uint16_t)0x001F;          // coarse X = 0
        ppu->v ^= (uint16_t)0x0400;           // switch horizontal nametable
    } else {
        ppu->v += 1u; // increment coarse X
    }
}

static void increment_y(Ppu* ppu)
{
    if ((ppu->v & (uint16_t)0x7000u) != (uint16_t)0x7000) { // if fine Y < 7
        ppu->v += 0x1000u;                                  // increment fine Y
        return;
    }

    ppu->v &= ~(uint16_t)0x7000;                   // fine Y = 0
    uint16_t y = (ppu->v & (uint16_t)0x03E0) >> 5; // let y = coarse Y
    if (y == 29u) {
        y = 0u;                     // coarse Y = 0
        ppu->v ^= (uint16_t)0x0800; // switch vertical nametable
    } else if (y == 31u) {
        y = 0u; // coarse Y = 0, nametable not switched
    } else {
        y += 1u; // increment coarse Y
    }
    ppu->v =
        (ppu->v & ~(uint16_t)0x03E0) | (y << 5); // put coarse Y back into v
}

static void updated_nmi(Ppu* ppu)
{
    int trigger_nmi =
        (ppu->flags & FLAG_IN_VBLANK) && (ppu->flags & FLAG_TRIGGER_NMI);
    if (trigger_nmi && !ppu->nmi_prev) {
        ppu->nmi_delay = 15;
    }
    ppu->nmi_prev = trigger_nmi;
}

static void copy_x(Ppu* ppu)
{
    // v: .....F.. ...EDCBA = t: .....F.. ...EDCBA
    ppu->v = (ppu->v & (uint16_t)0b1111101111100000);
    ppu->v |= (ppu->t & (uint16_t)0b0000010000011111);
}

static void copy_y(Ppu* ppu)
{
    // v: .IHGF.ED CBA..... = t: .IHGF.ED CBA.....
    ppu->v = (ppu->v & (uint16_t)0b100010000011111);
    ppu->v |= (ppu->t & (uint16_t)0b011101111100000);
}

static void render_pixel(Ppu* ppu)
{
    int x = ppu->cycle - 1;
    int y = ppu->scanline;

    uint8_t bg_pixel = 0;
    if (ppu->flags & FLAG_SHOW_BACKGROUND)
        bg_pixel =
            ((uint32_t)(ppu->tile_data >> 32) >> ((7 - ppu->x) * 4)) & 0x0F;

    ppu->flags |= FLAG_SPRITE_HIT_ZERO;

    uint8_t  color = bg_pixel;
    uint32_t rgb   = palette_colors[memory_read(ppu->mem, 0x3F00u + color)];
    gamewindow_set_pixel(ppu->gw, x, y, rgb);
}

static void set_vertical_blank(Ppu* ppu)
{
    ppu->flags |= FLAG_IN_VBLANK;
    updated_nmi(ppu);
}
static void clear_vertical_blank(Ppu* ppu)
{
    ppu->flags &= ~FLAG_IN_VBLANK;
    updated_nmi(ppu);
}

static void update_cycle(Ppu* ppu)
{
    if (ppu->nmi_delay > 0) {
        int must_trigger_nmi =
            (ppu->flags & FLAG_IN_VBLANK) && (ppu->flags & FLAG_TRIGGER_NMI);
        ppu->nmi_delay--;
        if (ppu->nmi_delay == 0 && must_trigger_nmi)
            cpu_trigger_nmi(ppu->sys->cpu);
    }

    if ((ppu->flags & FLAG_SHOW_BACKGROUND) ||
        (ppu->flags & FLAG_SHOW_SPRITES)) {
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
    if (ppu->gw == NULL)
        panic("ppu_step(): gw is NULL");

#if PRINT_PPU_STATE
    info("%s", ppu_tostring_short(ppu));
#endif

    update_cycle(ppu);

    int rendering_enabled =
        ppu->flags & (FLAG_SHOW_BACKGROUND | FLAG_SHOW_SPRITES);
    int pre_line        = ppu->scanline == 261;
    int visible_line    = ppu->scanline < 240;
    int render_line     = pre_line || visible_line;
    int pre_fetch_cycle = ppu->cycle >= 321 && ppu->cycle <= 336;
    int visible_cycle   = ppu->cycle >= 1 && ppu->cycle <= 256;
    int fetch_cycle     = pre_fetch_cycle || visible_cycle;

    // BACKGROUND
    if (rendering_enabled) {
        if (visible_line && visible_cycle) {
            render_pixel(ppu);
        }
        if (render_line && fetch_cycle) {
            ppu->tile_data <<= 4;
            switch (ppu->cycle % 8) {
                case 0: {
                    uint32_t data = 0;
                    for (int i = 0; i < 8; ++i) {
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
                    if (ppu->flags & FLAG_BACKGROUND_TABLE)
                        addr += 0x1000;
                    ppu->low_tile_byte = memory_read(ppu->mem, addr);
                    break;
                }
                case 7: {
                    uint16_t fine_y = (ppu->v >> 12) & 7;
                    uint16_t addr   = ppu->name_table_byte * 16 + fine_y;
                    if (ppu->flags & FLAG_BACKGROUND_TABLE)
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

    // VBLANK
    if (ppu->scanline == 241 && ppu->cycle == 1) {
        set_vertical_blank(ppu);
    } else if (pre_line && ppu->cycle == 1) {
        clear_vertical_blank(ppu);
        ppu->flags &= ~FLAG_SPRITE_HIT_ZERO;
        ppu->flags &= ~FLAG_SPRITE_OVERFLOW;
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
    if (ppu->flags & FLAG_SPRITE_OVERFLOW)
        res |= (1u << 5);
    if (ppu->flags & FLAG_SPRITE_HIT_ZERO)
        res |= (1u << 6);
    if (ppu->flags & FLAG_IN_VBLANK)
        res |= (1u << 7);
    ppu->flags &= ~FLAG_IN_VBLANK;
    ppu->w = 0;

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

    if (ppu->flags & FLAG_INCREMENT)
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

    ppu->flags |= (value & 3);
    if ((value >> 2) & 1ul)
        ppu->flags |= FLAG_INCREMENT;
    else
        ppu->flags &= ~FLAG_INCREMENT;
    if ((value >> 3) & 1ul)
        ppu->flags |= FLAG_SPRITE_TABLE;
    else
        ppu->flags &= ~FLAG_SPRITE_TABLE;
    if ((value >> 4) & 1ul)
        ppu->flags |= FLAG_BACKGROUND_TABLE;
    else
        ppu->flags &= ~FLAG_BACKGROUND_TABLE;
    if ((value >> 5) & 1ul)
        ppu->flags |= FLAG_SPRITE_SIZE;
    else
        ppu->flags &= ~FLAG_SPRITE_SIZE;
    if ((value >> 6) & 1ul)
        ppu->flags |= FLAG_MASTER_SLAVE;
    else
        ppu->flags &= ~FLAG_MASTER_SLAVE;
    if ((value >> 7) & 1ul)
        ppu->flags |= FLAG_TRIGGER_NMI;
    else
        ppu->flags &= ~FLAG_TRIGGER_NMI;

    updated_nmi(ppu);

    // t: ....BA.. ........ = d: ......BA
    ppu->t &= (uint16_t)0x03 << 10;
    ppu->t |= ((uint16_t)value & 0x03u) << 10;
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

    if (value & 1)
        ppu->flags |= FLAG_GRAYSCALE;
    else
        ppu->flags &= ~FLAG_GRAYSCALE;
    if ((value >> 1) & 1)
        ppu->flags |= FLAG_SHOW_LEFT_BACKGROUND;
    else
        ppu->flags &= ~FLAG_SHOW_LEFT_BACKGROUND;
    if ((value >> 2) & 1)
        ppu->flags |= FLAG_SHOW_LEFT_SPRITES;
    else
        ppu->flags &= ~FLAG_SHOW_LEFT_SPRITES;
    if ((value >> 3) & 1)
        ppu->flags |= FLAG_SHOW_BACKGROUND;
    else
        ppu->flags &= ~FLAG_SHOW_BACKGROUND;
    if ((value >> 4) & 1)
        ppu->flags |= FLAG_SHOW_SPRITES;
    else
        ppu->flags &= ~FLAG_SHOW_SPRITES;
    if ((value >> 5) & 1)
        ppu->flags |= FLAG_RED_TINT;
    else
        ppu->flags &= ~FLAG_RED_TINT;
    if ((value >> 6) & 1)
        ppu->flags |= FLAG_GREEN_TINT;
    else
        ppu->flags &= ~FLAG_GREEN_TINT;
    if ((value >> 6) & 1)
        ppu->flags |= FLAG_BLUE_TINT;
    else
        ppu->flags &= ~FLAG_BLUE_TINT;
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
        ppu->t &= (uint16_t)0b0111001111100000;
        ppu->t |= ((uint16_t)value & 0x07u) << 12;
        ppu->t |= ((uint16_t)value & 0xF8u) << 2;
        ppu->w = 0;
    } else {
        // t: ........ ...HGFED = d: HGFED...
        // x:               CBA = d: .....CBA
        // w:                   = 1
        ppu->t &= (uint16_t)0b1111111111100000;
        ppu->t |= ((uint16_t)value >> 3) & 0x1F;
        ppu->x &= 0x07;
        ppu->x |= value & 0x07;
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

    if (ppu->flags & FLAG_INCREMENT)
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

    panic("ppu: invalid write to 0x%04x [0x%02x]", addr, value);
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
    static char res[64];
    memset(res, 0, sizeof(res));

    sprintf(
        res,
        "PPU: SL=%03u CYC=%03u DL=%02d PREV_NMI=%d NMI_OUT=%d V=%04x T=%04x",
        ppu->scanline, ppu->cycle, ppu->nmi_delay, ppu->nmi_prev,
        (ppu->flags & FLAG_TRIGGER_NMI) ? 1 : 0, ppu->v, ppu->t);
    return res;
}
