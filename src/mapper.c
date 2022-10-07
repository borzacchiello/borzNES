#include "mapper.h"
#include "alloc.h"
#include "cartridge.h"
#include "logging.h"
#include "ppu.h"
#include "system.h"
#include "memory.h"
#include "6502_cpu.h"

#include <assert.h>
#include <string.h>

// http://tuxnes.sourceforge.net/mappers-0.80.txt

#define GEN_SERIALIZER(TYPE)                                                   \
    void TYPE##_serialize(void* _map, FILE* fout)                              \
    {                                                                          \
        Buffer res = {.buffer = (uint8_t*)_map, .size = sizeof(TYPE)};         \
        dump_buffer(&res, fout);                                               \
    }
#define GEN_DESERIALIZER(TYPE)                                                 \
    void TYPE##_deserialize(void* _map, FILE* fin)                             \
    {                                                                          \
        Buffer buf = read_buffer(fin);                                         \
        if (buf.size != sizeof(TYPE))                                          \
            panic(#TYPE "_deserialize(): invalid buffer");                     \
        TYPE* map      = (TYPE*)_map;                                          \
        void* tmp_cart = map->cart;                                            \
        memcpy(map, buf.buffer, buf.size);                                     \
        map->cart = tmp_cart;                                                  \
        free(buf.buffer);                                                      \
    }

static void generic_destroy(void* _map) { free(_map); }

static int32_t calc_prg_bank_offset(Cartridge* cart, int32_t idx,
                                    uint32_t bank_size)
{
    if (cart->PRG_size % bank_size != 0)
        panic("calc_prg_bank_offset(): incorrect PRG bank_size");

    idx %= cart->PRG_size / bank_size;
    int32_t off = idx * bank_size;
    if (off < 0)
        off += cart->PRG_size;
    return off;
}

static int32_t calc_chr_bank_offset(Cartridge* cart, int32_t idx,
                                    uint32_t bank_size)
{
    assert(cart->CHR_size % bank_size == 0 && "incorrect CHR bank_size");

    idx %= cart->CHR_size / bank_size;
    int32_t off = idx * bank_size;
    if (off < 0)
        off += cart->CHR_size;
    return off;
}

// MAPPER 000: NROM
typedef struct NROM {
    Cartridge* cart;
    int        prg_banks, prg_bank1, prg_bank2;
} NROM;

static NROM* NROM_build(Cartridge* cart)
{
    NROM* map      = malloc_or_fail(sizeof(NROM));
    map->cart      = cart;
    map->prg_banks = cart->PRG_size / 0x4000;
    map->prg_bank1 = 0;
    map->prg_bank2 = map->prg_banks - 1;

    return map;
}

static uint8_t NROM_read(void* _map, uint16_t addr)
{
    NROM* map = (NROM*)_map;
    if (addr < 0x2000)
        return map->cart->CHR[addr];
    if (addr >= 0xC000) {
        int index = map->prg_bank2 * 0x4000 + (addr - 0xC000);
        return map->cart->PRG[index];
    }
    if (addr >= 0x8000) {
        int index = map->prg_bank1 * 0x4000 + (addr - 0x8000);
        return map->cart->PRG[index];
    }
    if (addr >= 0x6000) {
        int index = addr - 0x6000;
        return map->cart->SRAM[index];
    }

    warning("unexpected read at address 0x%04x from NROM mapper", addr);
    return 0;
}

static void NROM_write(void* _map, uint16_t addr, uint8_t value)
{
    NROM* map = (NROM*)_map;
    if (addr < 0x2000) {
        map->cart->CHR[addr] = value;
    } else if (addr >= 0x8000) {
        map->prg_bank1 = (int)value % map->prg_banks;
    } else if (addr >= 0x6000) {
        int index              = addr - 0x6000;
        map->cart->SRAM[index] = value;
    } else {
        warning("unexpected write at address 0x%04x from NROM mapper [0x%02x]",
                addr, value);
    }
}

GEN_SERIALIZER(NROM)
GEN_DESERIALIZER(NROM)

// MAPPER 001: MMC1
typedef struct MMC1 {
    Cartridge* cart;
    uint8_t    shift_reg;
    uint8_t    control;
    uint8_t    prg_mode;
    uint8_t    chr_mode;
    uint8_t    prg_bank;
    uint8_t    chr_bank0;
    uint8_t    chr_bank1;
    int32_t    prg_offsets[2];
    int32_t    chr_offsets[2];
} MMC1;

static inline int32_t MMC1_calc_prg_bank_offset(MMC1* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x4000);
}

static inline int32_t MMC1_calc_chr_bank_offset(MMC1* map, int32_t idx)
{
    return calc_chr_bank_offset(map->cart, idx, 0x1000);
}

static MMC1* MMC1_build(Cartridge* cart)
{
    MMC1* map           = calloc_or_fail(sizeof(MMC1));
    map->cart           = cart;
    map->shift_reg      = 0x10;
    map->prg_offsets[1] = MMC1_calc_prg_bank_offset(map, -1);

    return map;
}

static void MMC1_update_offsets(MMC1* map)
{
    // PRG ROM bank mode
    //    0, 1: switch 32 KB at $8000, ignoring low bit of bank number
    //    2:    fix first bank at $8000 and switch 16 KB bank at $C000
    //    3:    fix last bank at $C000 and switch 16 KB bank
    //                    at $8000)
    // CHR ROM bank mode
    //    0: switch 8 KB at a time
    //    1: switch two separate 4 KB banks

    switch (map->prg_mode) {
        case 0:
        case 1:
            map->prg_offsets[0] =
                MMC1_calc_prg_bank_offset(map, map->prg_bank & 0xFE);
            map->prg_offsets[1] =
                MMC1_calc_prg_bank_offset(map, map->prg_bank | 0x01);
            break;
        case 2:
            map->prg_offsets[0] = 0;
            map->prg_offsets[1] = MMC1_calc_prg_bank_offset(map, map->prg_bank);
            break;
        case 3:
            map->prg_offsets[0] = MMC1_calc_prg_bank_offset(map, map->prg_bank);
            map->prg_offsets[1] = MMC1_calc_prg_bank_offset(map, -1);
            break;
        default:
            panic("MMC1_update_offsets(): unexpected prg_mode %u",
                  map->prg_mode);
    }

    switch (map->chr_mode) {
        case 0:
            map->chr_offsets[0] =
                MMC1_calc_chr_bank_offset(map, map->chr_bank0 & 0xFE);
            map->chr_offsets[1] =
                MMC1_calc_chr_bank_offset(map, map->chr_bank0 | 0x01);
            break;
        case 1:
            map->chr_offsets[0] =
                MMC1_calc_chr_bank_offset(map, map->chr_bank0);
            map->chr_offsets[1] =
                MMC1_calc_chr_bank_offset(map, map->chr_bank1);
            break;
        default:
            panic("MMC1_update_offsets(): unexpected chr_mode %u",
                  map->prg_mode);
    }
}

static void MMC1_write_control(MMC1* map, uint8_t value)
{
    map->control  = value;
    map->chr_mode = (value >> 4) & 1;
    map->prg_mode = (value >> 2) & 3;
    switch (value & 3) {
        case 0:
            map->cart->mirror = MIRROR_SINGLE0;
            break;
        case 1:
            map->cart->mirror = MIRROR_SINGLE1;
            break;
        case 2:
            map->cart->mirror = MIRROR_VERTICAL;
            break;
        case 3:
            map->cart->mirror = MIRROR_HORIZONTAL;
            break;
    }
}

static void MMC1_write_register(MMC1* map, uint16_t addr, uint8_t value)
{
    if (addr <= 0x9FFF) {
        MMC1_write_control(map, value);
    } else if (addr <= 0xBFFF) {
        map->chr_bank0 = value;
    } else if (addr <= 0xDFFF) {
        map->chr_bank1 = value;
    } else if (addr <= 0xFFFF) {
        map->prg_bank = value & 0x0F;
    } else {
        panic("MMC1_write_register(): unexpected address: 0x%04x", addr);
    }

    MMC1_update_offsets(map);
}

static void MMC1_load_reg(MMC1* map, uint16_t addr, uint8_t value)
{
    if (value & 0x80) {
        map->shift_reg = 0x10;
        MMC1_write_control(map, map->control | 0x0C);
        MMC1_update_offsets(map);
    } else {
        int load_complete = map->shift_reg & 1;
        map->shift_reg >>= 1;
        map->shift_reg |= (value & 1) << 4;
        if (load_complete) {
            MMC1_write_register(map, addr, map->shift_reg);
            map->shift_reg = 0x10;
        }
    }
}

static uint8_t MMC1_read(void* _map, uint16_t addr)
{
    MMC1* map = (MMC1*)_map;
    if (addr < 0x2000) {
        uint16_t bank = addr / 0x1000;
        uint16_t off  = addr % 0x1000;
        return map->cart->CHR[map->chr_offsets[bank] + off];
    }
    if (addr >= 0x8000) {
        addr -= 0x8000;
        uint16_t bank = addr / 0x4000;
        uint16_t off  = addr % 0x4000;
        return map->cart->PRG[map->prg_offsets[bank] + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read at address 0x%04x from MMC1 mapper", addr);
    return 0;
}

static void MMC1_write(void* _map, uint16_t addr, uint8_t value)
{
    MMC1* map = (MMC1*)_map;
    if (addr < 0x2000) {
        uint16_t bank                                = addr / 0x1000;
        uint16_t off                                 = addr % 0x1000;
        map->cart->CHR[map->chr_offsets[bank] + off] = value;
        return;
    }
    if (addr >= 0x8000) {
        MMC1_load_reg(map, addr, value);
        return;
    }
    if (addr >= 0x6000) {
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write at address 0x%04x from MMC1 mapper [0x%02x]",
            addr, value);
}

GEN_SERIALIZER(MMC1)
GEN_DESERIALIZER(MMC1)

// MAPPER 004: MMC3

typedef struct MMC3 {
    Cartridge* cart;
    uint8_t    reg;
    uint8_t    regs[8];
    uint8_t    prg_mode;
    uint8_t    chr_mode;
    int32_t    prg_offsets[4];
    int32_t    chr_offsets[8];
    uint8_t    reload;
    uint8_t    irq_counter;
    uint8_t    irq_enable;
} MMC3;

static inline int32_t MMC3_calc_prg_bank_offset(MMC3* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x2000);
}

static inline int32_t MMC3_calc_chr_bank_offset(MMC3* map, int32_t idx)
{
    return calc_chr_bank_offset(map->cart, idx, 0x0400);
}

static MMC3* MMC3_build(Cartridge* cart)
{
    MMC3* map           = calloc_or_fail(sizeof(MMC3));
    map->cart           = cart;
    map->prg_offsets[0] = MMC3_calc_prg_bank_offset(map, 0);
    map->prg_offsets[1] = MMC3_calc_prg_bank_offset(map, 1);
    map->prg_offsets[2] = MMC3_calc_prg_bank_offset(map, -2);
    map->prg_offsets[3] = MMC3_calc_prg_bank_offset(map, -1);

    return map;
}

static void MMC3_update_offsets(MMC3* map)
{
    switch (map->prg_mode) {
        case 0:
            map->prg_offsets[0] = MMC3_calc_prg_bank_offset(map, map->regs[6]);
            map->prg_offsets[1] = MMC3_calc_prg_bank_offset(map, map->regs[7]);
            map->prg_offsets[2] = MMC3_calc_prg_bank_offset(map, -2);
            map->prg_offsets[3] = MMC3_calc_prg_bank_offset(map, -1);
            break;
        case 1:
            map->prg_offsets[0] = MMC3_calc_prg_bank_offset(map, -2);
            map->prg_offsets[1] = MMC3_calc_prg_bank_offset(map, map->regs[7]);
            map->prg_offsets[2] = MMC3_calc_prg_bank_offset(map, map->regs[6]);
            map->prg_offsets[3] = MMC3_calc_prg_bank_offset(map, -1);
            break;
        default:
            panic("MMC3_update_offsets(): unexpected prg_mode (%u)",
                  map->prg_mode);
    }

    switch (map->chr_mode) {
        case 0:
            map->chr_offsets[0] =
                MMC3_calc_chr_bank_offset(map, map->regs[0] & 0xFE);
            map->chr_offsets[1] =
                MMC3_calc_chr_bank_offset(map, map->regs[0] | 0x01);
            map->chr_offsets[2] =
                MMC3_calc_chr_bank_offset(map, map->regs[1] & 0xFE);
            map->chr_offsets[3] =
                MMC3_calc_chr_bank_offset(map, map->regs[1] | 0x01);
            map->chr_offsets[4] = MMC3_calc_chr_bank_offset(map, map->regs[2]);
            map->chr_offsets[5] = MMC3_calc_chr_bank_offset(map, map->regs[3]);
            map->chr_offsets[6] = MMC3_calc_chr_bank_offset(map, map->regs[4]);
            map->chr_offsets[7] = MMC3_calc_chr_bank_offset(map, map->regs[5]);
            break;
        case 1:
            map->chr_offsets[0] = MMC3_calc_chr_bank_offset(map, map->regs[2]);
            map->chr_offsets[1] = MMC3_calc_chr_bank_offset(map, map->regs[3]);
            map->chr_offsets[2] = MMC3_calc_chr_bank_offset(map, map->regs[4]);
            map->chr_offsets[3] = MMC3_calc_chr_bank_offset(map, map->regs[5]);
            map->chr_offsets[4] =
                MMC3_calc_chr_bank_offset(map, map->regs[0] & 0xFE);
            map->chr_offsets[5] =
                MMC3_calc_chr_bank_offset(map, map->regs[0] | 0x01);
            map->chr_offsets[6] =
                MMC3_calc_chr_bank_offset(map, map->regs[1] & 0xFE);
            map->chr_offsets[7] =
                MMC3_calc_chr_bank_offset(map, map->regs[1] | 0x01);
            break;
        default:
            panic("MMC3_update_offsets(): unexpected chr_mode (%u)",
                  map->chr_mode);
    }
}

static void MMC3_write_reg(MMC3* map, uint16_t addr, uint8_t value)
{
    if (addr <= 0x9FFF && addr % 2 == 0) {
        map->prg_mode = (value >> 6) & 1;
        map->chr_mode = (value >> 7) & 1;
        map->reg      = value & 7;
        MMC3_update_offsets(map);
    } else if (addr <= 0x9FFF && addr % 2 == 1) {
        map->regs[map->reg] = value;
        MMC3_update_offsets(map);
    } else if (addr <= 0xBFFF && addr % 2 == 0) {
        if (value & 1)
            map->cart->mirror = MIRROR_HORIZONTAL;
        else
            map->cart->mirror = MIRROR_VERTICAL;
    } else if (addr <= 0xBFFF && addr % 2 == 1) {
        // Do nothing
    } else if (addr <= 0xDFFF && addr % 2 == 0) {
        map->reload = value;
    } else if (addr <= 0xDFFF && addr % 2 == 1) {
        map->irq_counter = 0;
    } else if (addr <= 0xFFFF && addr % 2 == 0) {
        map->irq_enable = 0;
    } else if (addr <= 0xFFFF && addr % 2 == 1) {
        map->irq_enable = 1;
    } else
        panic("invalid write in MMC3 @ 0x%04x [0x%02x]", addr, value);
}

static void MMC3_step(void* _map, System* sys)
{
    MMC3* map = (MMC3*)_map;

    if (sys->ppu->cycle != 260)
        return;
    if (sys->ppu->scanline >= 240)
        return;
    if (!sys->ppu->mask_flags.show_background &&
        !sys->ppu->mask_flags.show_sprites)
        return;

    if (map->irq_counter == 0)
        map->irq_counter = map->reload;
    else {
        map->irq_counter--;
        if (map->irq_counter == 0 && map->irq_enable)
            cpu_trigger_irq(sys->cpu);
    }
}

static uint8_t MMC3_read(void* _map, uint16_t addr)
{
    MMC3* map = (MMC3*)_map;
    if (addr < 0x2000) {
        uint16_t bank = addr / 0x0400;
        uint16_t off  = addr % 0x0400;
        return map->cart->CHR[map->chr_offsets[bank] + off];
    }
    if (addr >= 0x8000) {
        addr -= 0x8000;
        uint16_t bank = addr / 0x2000;
        uint16_t off  = addr % 0x2000;
        return map->cart->PRG[map->prg_offsets[bank] + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read at address 0x%04x from MMC3 mapper", addr);
    return 0;
}

static void MMC3_write(void* _map, uint16_t addr, uint8_t value)
{
    MMC3* map = (MMC3*)_map;
    if (addr < 0x2000) {
        uint16_t bank                                = addr / 0x0400;
        uint16_t off                                 = addr % 0x0400;
        map->cart->CHR[map->chr_offsets[bank] + off] = value;
        return;
    }
    if (addr >= 0x8000) {
        MMC3_write_reg(map, addr, value);
        return;
    }
    if (addr >= 0x6000) {
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write at address 0x%04x from MMC3 mapper [0x%02x]",
            addr, value);
}

GEN_SERIALIZER(MMC3)
GEN_DESERIALIZER(MMC3)

// MAPPER 003: CNROM

typedef struct CNROM {
    Cartridge* cart;
    int32_t    chr_bank;
} CNROM;

static CNROM* CNROM_build(Cartridge* cart)
{
    CNROM* map = calloc_or_fail(sizeof(CNROM));
    map->cart  = cart;
    return map;
}

static inline int32_t CNROM_calc_prg_bank_offset(CNROM* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x4000);
}

static inline int32_t CNROM_calc_chr_bank_offset(CNROM* map, int32_t idx)
{
    return calc_chr_bank_offset(map->cart, idx, 0x2000);
}

static uint8_t CNROM_read(void* _map, uint16_t addr)
{
    CNROM* map = (CNROM*)_map;
    if (addr < 0x2000) {
        int32_t base = CNROM_calc_chr_bank_offset(map, map->chr_bank);
        int32_t off  = addr;
        return map->cart->CHR[base + off];
    }
    if (addr >= 0xC000) {
        int32_t base = CNROM_calc_prg_bank_offset(map, -1);
        int32_t off  = addr - 0xC000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x8000) {
        int32_t base = 0;
        int32_t off  = addr - 0x8000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in CNROM mapper", addr);
    return 0;
}

static void CNROM_write(void* _map, uint16_t addr, uint8_t value)
{
    CNROM* map = (CNROM*)_map;
    if (addr < 0x2000) {
        int32_t base = CNROM_calc_chr_bank_offset(map, map->chr_bank);
        int32_t off  = addr;
        map->cart->CHR[base + off] = value;
        return;
    }
    if (addr >= 0x8000) {
        map->chr_bank = value & 3;
        return;
    }
    if (addr >= 0x6000) {
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in CNROM mapper [0x%02x]", addr, value);
}

GEN_SERIALIZER(CNROM)
GEN_DESERIALIZER(CNROM)

// MAPPER 007: AxROM

typedef struct AxROM {
    Cartridge* cart;
    int32_t    prg_bank;
} AxROM;

static AxROM* AxROM_build(Cartridge* cart)
{
    AxROM* map = calloc_or_fail(sizeof(AxROM));
    map->cart  = cart;
    return map;
}

static uint8_t AxROM_read(void* _map, uint16_t addr)
{
    AxROM* map = (AxROM*)_map;
    if (addr < 0x2000) {
        return map->cart->CHR[addr];
    }
    if (addr >= 0x8000) {
        int32_t idx = map->prg_bank * 0x8000 + (addr - 0x8000);
        return map->cart->PRG[idx];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in AxROM mapper", addr);
    return 0;
}

static void AxROM_write(void* _map, uint16_t addr, uint8_t value)
{
    AxROM* map = (AxROM*)_map;
    if (addr < 0x2000) {
        map->cart->CHR[addr] = value;
        return;
    }
    if (addr >= 0x8000) {
        map->prg_bank = value & 7;
        if (value & 0x10)
            map->cart->mirror = MIRROR_SINGLE1;
        else
            map->cart->mirror = MIRROR_SINGLE0;
        return;
    }
    if (addr >= 0x6000) {
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in AxROM mapper [0x%02x]", addr, value);
}

GEN_SERIALIZER(AxROM)
GEN_DESERIALIZER(AxROM)

// MAPPER 009: MMC2

typedef struct MMC2 {
    Cartridge* cart;
    uint8_t    prg_bank;
    uint8_t    chr_r1, chr_r2, chr_r3, chr_r4;
    uint8_t    latch_0, latch_1;
} MMC2;

static MMC2* MMC2_build(Cartridge* cart)
{
    MMC2* map = calloc_or_fail(sizeof(MMC2));
    map->cart = cart;
    return map;
}

static inline int32_t MMC2_calc_prg_bank_offset(MMC2* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x2000);
}

static inline int32_t MMC2_calc_chr_bank_offset(MMC2* map, int32_t idx)
{
    return calc_chr_bank_offset(map->cart, idx, 0x1000);
}

static uint8_t MMC2_read(void* _map, uint16_t addr)
{
    MMC2* map = (MMC2*)_map;
    if (addr < 0x1000) {
        int32_t base = MMC2_calc_chr_bank_offset(
            map, map->latch_0 ? map->chr_r2 : map->chr_r1);
        int32_t off = addr;
        if (addr == 0x0FD8)
            map->latch_0 = 0;
        if (addr == 0x0FE8)
            map->latch_0 = 1;
        return map->cart->CHR[base + off];
    }
    if (addr < 0x2000) {
        int32_t base = MMC2_calc_chr_bank_offset(
            map, map->latch_1 ? map->chr_r4 : map->chr_r3);
        int32_t off = addr - 0x1000;
        if (addr >= 0x1FD8 && addr <= 0x1FDF)
            map->latch_1 = 0;
        if (addr >= 0x1FE8 && addr <= 0x1FEF)
            map->latch_1 = 1;
        return map->cart->CHR[base + off];
    }
    if (addr >= 0xE000) {
        int32_t base = MMC2_calc_prg_bank_offset(map, -1);
        int32_t off  = addr - 0xE000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0xC000) {
        int32_t base = MMC2_calc_prg_bank_offset(map, -2);
        int32_t off  = addr - 0xC000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0xA000) {
        int32_t base = MMC2_calc_prg_bank_offset(map, -3);
        int32_t off  = addr - 0xA000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x8000) {
        int32_t base = MMC2_calc_prg_bank_offset(map, map->prg_bank);
        int32_t off  = addr - 0x8000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in MMC2 mapper", addr);
    return 0;
}

static void MMC2_write(void* _map, uint16_t addr, uint8_t value)
{
    MMC2* map = (MMC2*)_map;
    if (addr < 0x2000) {
        map->cart->CHR[addr] = value;
        return;
    }
    if (addr >= 0xF000) {
        if (value & 1)
            map->cart->mirror = MIRROR_HORIZONTAL;
        else
            map->cart->mirror = MIRROR_VERTICAL;
        return;
    }
    if (addr >= 0xE000) {
        map->chr_r4 = value & 0x1f;
        return;
    }
    if (addr >= 0xD000) {
        map->chr_r3 = value & 0x1f;
        return;
    }
    if (addr >= 0xC000) {
        map->chr_r2 = value & 0x1f;
        return;
    }
    if (addr >= 0xB000) {
        map->chr_r1 = value & 0x1f;
        return;
    }
    if (addr >= 0xA000) {
        map->prg_bank = value & 0xf;
        return;
    }
    if (addr >= 0x6000 && addr < 0x8000) {
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in MMC2 mapper [0x%02x]", addr, value);
}

GEN_SERIALIZER(MMC2)
GEN_DESERIALIZER(MMC2)

// MAPPER 071: Camerica

typedef struct Map071 {
    Cartridge* cart;
    int32_t    prg_bank;
} Map071;

static Map071* Map071_build(Cartridge* cart)
{
    Map071* map   = malloc_or_fail(sizeof(Map071));
    map->cart     = cart;
    map->prg_bank = 0;
    return map;
}

static inline int32_t Map071_calc_prg_bank_offset(Map071* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x4000);
}

static uint8_t Map071_read(void* _map, uint16_t addr)
{
    Map071* map = (Map071*)_map;
    if (addr < 0x2000) {
        return map->cart->CHR[addr];
    }
    if (addr >= 0xC000) {
        uint32_t base = Map071_calc_prg_bank_offset(map, -1);
        uint32_t off  = addr - 0xC000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x8000) {
        uint32_t base = Map071_calc_prg_bank_offset(map, map->prg_bank);
        uint32_t off  = addr - 0x8000;
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in Map071 mapper", addr);
    return 0;
}

static void Map071_write(void* _map, uint16_t addr, uint8_t value)
{
    Map071* map = (Map071*)_map;
    if (addr < 0x2000) {
        map->cart->CHR[addr] = value;
        return;
    }
    if (addr >= 0xC000) {
        map->prg_bank = value;
        return;
    }
    if (addr >= 0x8000) {
        if ((addr & 0xF000) == 0x9000) {
            if ((value >> 4) & 1)
                map->cart->mirror = MIRROR_SINGLE1;
            else
                map->cart->mirror = MIRROR_SINGLE0;
        }
        return;
    }
    if (addr >= 0x6000) {
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in Map071 mapper [0x%02x]", addr, value);
}

GEN_SERIALIZER(Map071)
GEN_DESERIALIZER(Map071)

// MAPPER 163: FC_001
// Thank you fceux...

typedef struct FC_001 {
    Cartridge* cart;
    uint8_t    laststrobe, trigger;
    uint8_t    reg0, reg1, reg2, reg3;
    int32_t    bank_chr[2];
    int32_t    bank_prg;
} FC_001;

static inline int32_t FC_001_calc_prg_bank_offset(FC_001* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x8000);
}

static inline int32_t FC_001_calc_chr_bank_offset(FC_001* map, int32_t idx)
{
    return calc_chr_bank_offset(map->cart, idx, 0x1000);
}

static void FC_001_sync(FC_001* map)
{
    map->bank_chr[0] = FC_001_calc_chr_bank_offset(map, 0);
    map->bank_chr[1] = FC_001_calc_chr_bank_offset(map, 1);

    map->bank_prg =
        FC_001_calc_prg_bank_offset(map, (map->reg0 << 4) | (map->reg1 & 0xF));
}

static FC_001* FC_001_build(Cartridge* cart)
{
    FC_001* map = calloc_or_fail(sizeof(FC_001));
    map->cart   = cart;

    map->laststrobe = 1;
    map->reg0       = 3;
    map->reg3       = 7;
    FC_001_sync(map);
    return map;
}

static uint8_t FC_001_read(void* _map, uint16_t addr)
{
    FC_001* map = (FC_001*)_map;
    if (addr < 0x2000) {
        uint16_t bank = addr / 0x1000;
        uint16_t off  = addr % 0x1000;
        return map->cart->CHR[map->bank_chr[bank] + off];
    }
    if (addr >= 0x8000) {
        uint16_t off = addr - 0x8000;
        return map->cart->PRG[map->bank_prg + off];
    }
    if (addr >= 0x5000 && addr <= 0x50FF) {
        return (map->reg2 | map->reg0 | map->reg1 | map->reg3) ^ 0xff;
    }
    if (addr >= 0x5100 && addr <= 0x51FF) {
        return 4;
    }
    if (addr >= 0x5200 && addr <= 0x52FF) {
        return 4;
    }
    if (addr >= 0x5300 && addr <= 0x53FF) {
        return 4;
    }
    if (addr >= 0x5500 && addr <= 0x55FF) {
        if (map->trigger)
            return map->reg2 | map->reg1;
        return 0;
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in FC_001 mapper", addr);
    return 0;
}

static void FC_001_write(void* _map, uint16_t addr, uint8_t value)
{
    FC_001* map = (FC_001*)_map;
    if (addr < 0x2000) {
        map->cart->CHR[addr] = value;
        return;
    }
    if (addr == 0x5101) {
        if (map->laststrobe && !value)
            map->trigger ^= 1;
        map->laststrobe = value;
        return;
    }
    if (addr == 0x5100 && value == 6) {
        map->bank_prg = FC_001_calc_prg_bank_offset(map, 3);
        return;
    }
    if (addr >= 0x5000 && addr <= 0x50FF) {
        map->reg1 = value;
        FC_001_sync(map);
        if (!(map->reg1 & 0x80)) {
            map->bank_chr[0] = FC_001_calc_chr_bank_offset(map, 0);
            map->bank_chr[1] = FC_001_calc_chr_bank_offset(map, 1);
        }
        return;
    }
    if (addr >= 0x5100 && addr <= 0x51FF) {
        map->reg3 = value;
        FC_001_sync(map);
        return;
    }
    if (addr >= 0x5200 && addr <= 0x52FF) {
        map->reg0 = value;
        FC_001_sync(map);
        return;
    }
    if (addr >= 0x5300 && addr <= 0x53FF) {
        map->reg2 = value;
        return;
    }
    if (addr >= 0x6000) {
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in FC_001 mapper [0x%02x]", addr, value);
}

static void FC_001_step(void* _map, System* sys)
{
    FC_001* map = (FC_001*)_map;
    Ppu*    ppu = sys->ppu;

    if (sys->ppu->cycle != 260)
        return;
    if (!ppu->mask_flags.show_background && !ppu->mask_flags.show_sprites)
        return;
    if (!(map->reg1 & 0x80))
        return;

    if (ppu->scanline == 239) {
        map->bank_chr[0] = FC_001_calc_chr_bank_offset(map, 0);
        map->bank_chr[1] = FC_001_calc_chr_bank_offset(map, 0);
    } else if (ppu->scanline == 127) {
        map->bank_chr[0] = FC_001_calc_chr_bank_offset(map, 1);
        map->bank_chr[1] = FC_001_calc_chr_bank_offset(map, 1);
    }
}

GEN_SERIALIZER(FC_001)
GEN_DESERIALIZER(FC_001)

// Polymorphic Mapper
Mapper* mapper_build(Cartridge* cart)
{
    Mapper* map = malloc_or_fail(sizeof(Mapper));
    switch (cart->mapper) {
        case 0:
        case 2: {
            NROM* nrom       = NROM_build(cart);
            map->obj         = nrom;
            map->name        = "NROM";
            map->step        = NULL;
            map->read        = &NROM_read;
            map->write       = &NROM_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &NROM_serialize;
            map->deserialize = &NROM_deserialize;
            break;
        }
        case 1: {
            MMC1* mmc1       = MMC1_build(cart);
            map->obj         = mmc1;
            map->name        = "MMC1";
            map->step        = NULL;
            map->read        = &MMC1_read;
            map->write       = &MMC1_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &MMC1_serialize;
            map->deserialize = &MMC1_deserialize;
            break;
        }
        case 3: {
            CNROM* cnrom     = CNROM_build(cart);
            map->obj         = cnrom;
            map->name        = "CNROM";
            map->step        = NULL;
            map->read        = &CNROM_read;
            map->write       = &CNROM_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &CNROM_serialize;
            map->deserialize = &CNROM_deserialize;
            break;
        }
        case 4: {
            MMC3* mmc3       = MMC3_build(cart);
            map->obj         = mmc3;
            map->name        = "MMC3";
            map->step        = &MMC3_step;
            map->read        = &MMC3_read;
            map->write       = &MMC3_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &MMC3_serialize;
            map->deserialize = &MMC3_deserialize;
            break;
        }
        case 7: {
            AxROM* axrom     = AxROM_build(cart);
            map->obj         = axrom;
            map->name        = "AxROM";
            map->step        = NULL;
            map->read        = &AxROM_read;
            map->write       = &AxROM_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &AxROM_serialize;
            map->deserialize = &AxROM_deserialize;
            break;
        }
        case 9: {
            MMC2* mmc2       = MMC2_build(cart);
            map->obj         = mmc2;
            map->name        = "MMC2";
            map->step        = NULL;
            map->read        = &MMC2_read;
            map->write       = &MMC2_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &MMC2_serialize;
            map->deserialize = &MMC2_deserialize;
            break;
        }
        case 71: {
            Map071* map071   = Map071_build(cart);
            map->obj         = map071;
            map->name        = "Map071";
            map->step        = NULL;
            map->read        = &Map071_read;
            map->write       = &Map071_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &Map071_serialize;
            map->deserialize = &Map071_deserialize;
            break;
        }
        case 163: {
            FC_001* fc_001   = FC_001_build(cart);
            map->obj         = fc_001;
            map->name        = "FC_001";
            map->step        = &FC_001_step;
            map->read        = &FC_001_read;
            map->write       = &FC_001_write;
            map->destroy     = &generic_destroy;
            map->serialize   = &FC_001_serialize;
            map->deserialize = &FC_001_deserialize;
            break;
        }
        default:
            panic("unsupported mapper %u", cart->mapper);
    }
    return map;
}

void mapper_destroy(Mapper* map)
{
    map->destroy(map->obj);
    free(map);
}

uint8_t mapper_read(Mapper* map, uint16_t addr)
{
    return map->read(map->obj, addr);
}

void mapper_write(Mapper* map, uint16_t addr, uint8_t value)
{
    return map->write(map->obj, addr, value);
}

void mapper_step(Mapper* map, System* sys)
{
    if (map->step)
        map->step(map->obj, sys);
}

void mapper_serialize(Mapper* map, FILE* fout)
{
    return map->serialize(map->obj, fout);
}

void mapper_deserialize(Mapper* map, FILE* fin)
{
    map->deserialize(map->obj, fin);
}
