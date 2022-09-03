#include "ppu.h"
#include "6502_cpu.h"
#include "system.h"
#include "memory.h"
#include "alloc.h"
#include "logging.h"

#include <stdio.h>
#include <string.h>

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

Ppu* ppu_build(System* sys)
{
    Ppu* ppu = calloc_or_fail(sizeof(Ppu));
    ppu->sys = sys;
    ppu->mem = ppu_memory_build(sys);

    ppu_reset(ppu);
    return ppu;
}

void ppu_destroy(Ppu* ppu)
{
    memory_destroy(ppu->mem);
    free(ppu);
}

void ppu_reset(Ppu* ppu)
{
    ppu->cycle    = 340;
    ppu->scanline = 240;
    ppu->frame    = 0;
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

static void set_vertical_blank(Ppu* ppu) { ppu->flags |= FLAG_IN_VBLANK; }
static void clear_vertical_blank(Ppu* ppu) { ppu->flags &= ~FLAG_IN_VBLANK; }

static void update_cycle(Ppu* ppu)
{
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
    if ((ppu->flags & FLAG_IN_VBLANK) && (ppu->flags & FLAG_TRIGGER_NMI)) {
        ppu->flags &= ~FLAG_TRIGGER_NMI;
        cpu_trigger_nmi(ppu->sys->cpu);
    }

    update_cycle(ppu);

    int rendering_enabled =
        ppu->flags & (FLAG_SHOW_BACKGROUND | FLAG_SHOW_SPRITES);
    int pre_line        = ppu->scanline == 261u;
    int visible_line    = ppu->scanline < 240u;
    int render_line     = pre_line || visible_line;
    int pre_fetch_cycle = ppu->cycle >= 321u && ppu->cycle <= 336u;
    int visible_cycle   = ppu->cycle >= 1u && ppu->cycle <= 256u;
    int fetch_cycle     = pre_fetch_cycle || visible_cycle;

    // BACKGROUND
    if (rendering_enabled) {
        if (render_line && fetch_cycle) {
            switch (ppu->cycle % 8) {
                case 0:
                    break;
                case 1:
                    break;
                case 3:
                    break;
                case 5:
                    break;
                case 7:
                    break;
            }
        }
        if (visible_line && visible_cycle) {
            // RENDER PIXEL
        }
        if (pre_line && ppu->cycle >= 280u && ppu->cycle <= 304u) {
            // COPY Y
        }
        if (render_line) {
            if (fetch_cycle && ppu->cycle % 8 == 0) {
                increment_x(ppu);
            }
            if (ppu->cycle == 256u) {
                increment_y(ppu);
            }
            if (ppu->cycle == 257u) {
                // COPY X
            }
        }
    }

    // VBLANK
    if (ppu->scanline == 241 && ppu->cycle == 1) {
        set_vertical_blank(ppu);
    }
    if (ppu->scanline == 261 && ppu->cycle == 1) {
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
    if ((value >> 2) & 1)
        ppu->flags |= FLAG_INCREMENT;
    if ((value >> 3) & 1)
        ppu->flags |= FLAG_SPRITE_TABLE;
    if ((value >> 4) & 1)
        ppu->flags |= FLAG_BACKGROUND_TABLE;
    if ((value >> 5) & 1)
        ppu->flags |= FLAG_SPRITE_SIZE;
    if ((value >> 6) & 1)
        ppu->flags |= FLAG_MASTER_SLAVE;
    if ((value >> 7) & 1)
        ppu->flags |= FLAG_TRIGGER_NMI;

    // t: ....BA.. ........ = d: ......BA
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
    if ((value >> 1) & 1)
        ppu->flags |= FLAG_SHOW_LEFT_BACKGROUND;
    if ((value >> 2) & 1)
        ppu->flags |= FLAG_SHOW_LEFT_SPRITES;
    if ((value >> 3) & 1)
        ppu->flags |= FLAG_SHOW_BACKGROUND;
    if ((value >> 4) & 1)
        ppu->flags |= FLAG_SHOW_SPRITES;
    if ((value >> 5) & 1)
        ppu->flags |= FLAG_RED_TINT;
    if ((value >> 6) & 1)
        ppu->flags |= FLAG_GREEN_TINT;
    if ((value >> 6) & 1)
        ppu->flags |= FLAG_BLUE_TINT;
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
        ppu->t |= ((uint16_t)value & 0x07u) << 12;
        ppu->t |= ((uint16_t)value & 0xF8u) << 2;
        ppu->w = 0;
    } else {
        // t: ........ ...HGFED = d: HGFED...
        // x:               CBA = d: .....CBA
        // w:                   = 1
        ppu->t |= ((uint16_t)value >> 3) & 0x1F;
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
    static char res[32];
    memset(res, 0, sizeof(res));

    sprintf(res, "F: %u, S: %u, C: %u", ppu->frame, ppu->scanline, ppu->cycle);
    return res;
}
