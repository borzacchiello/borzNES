#include "mapper.h"
#include "alloc.h"
#include "cartridge.h"
#include "logging.h"
#include "ppu.h"
#include "system.h"
#include "6502_cpu.h"

#include <string.h>

static void generic_destroy(void* _map) { free(_map); }

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

    panic("unable to read at address 0x%04x from NROM mapper", addr);
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
        panic("unable to write at address 0x%04x from NROM mapper [0x%02x]",
              addr, value);
    }
}

void NROM_serialize(void* _map, FILE* fout)
{
    NROM* map = (NROM*)_map;

    Buffer res = buf_malloc(sizeof(NROM));
    memcpy(res.buffer, map, res.size);
    NROM* map_cpy = (NROM*)res.buffer;
    map_cpy->cart = NULL;

    dump_buffer(&res, fout);
    free(res.buffer);
}

void NROM_deserialize(void* _map, FILE* fin)
{
    Buffer buf = read_buffer(fin);
    if (buf.size != sizeof(NROM))
        panic("NROM_deserialize(): invalid buffer");

    NROM* map = (NROM*)_map;

    void* tmp_cart = map->cart;

    memcpy(map, buf.buffer, buf.size);
    map->cart = tmp_cart;
    free(buf.buffer);
}

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

static int32_t MMC_calc_prg_bank_offset(Cartridge* cart, int32_t idx,
                                        uint32_t page_size)
{
    if (idx >= 0x80)
        idx -= 0x100;

    idx %= cart->PRG_size / page_size;
    int32_t off = idx * page_size;
    if (off < 0)
        off += cart->PRG_size;
    return off;
}

static int32_t MMC_calc_chr_bank_offset(Cartridge* cart, int32_t idx,
                                        uint32_t page_size)
{
    if (idx >= 0x80)
        idx -= 0x100;

    idx %= cart->CHR_size / page_size;
    int32_t off = idx * page_size;
    if (off < 0)
        off += cart->CHR_size;
    return off;
}

static inline int32_t MMC1_calc_prg_bank_offset(MMC1* map, int32_t idx)
{
    return MMC_calc_prg_bank_offset(map->cart, idx, 0x4000);
}

static inline int32_t MMC1_calc_chr_bank_offset(MMC1* map, int32_t idx)
{
    return MMC_calc_chr_bank_offset(map->cart, idx, 0x1000);
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

    panic("unable to read at address 0x%04x from MMC1 mapper", addr);
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

    panic("unable to write at address 0x%04x from MMC1 mapper [0x%02x]", addr,
          value);
}

void MMC1_serialize(void* _map, FILE* fout)
{
    MMC1* map = (MMC1*)_map;

    Buffer res = buf_malloc(sizeof(MMC1));
    memcpy(res.buffer, map, res.size);
    MMC1* map_cpy = (MMC1*)res.buffer;
    map_cpy->cart = NULL;

    dump_buffer(&res, fout);
    free(res.buffer);
}

void MMC1_deserialize(void* _map, FILE* fin)
{
    Buffer buf = read_buffer(fin);
    if (buf.size != sizeof(MMC1))
        panic("MMC1_deserialize(): invalid buffer");

    MMC1* map = (MMC1*)_map;

    void* tmp_cart = map->cart;

    memcpy(map, buf.buffer, buf.size);
    map->cart = tmp_cart;
    free(buf.buffer);
}

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
    return MMC_calc_prg_bank_offset(map->cart, idx, 0x2000);
}

static inline int32_t MMC3_calc_chr_bank_offset(MMC3* map, int32_t idx)
{
    return MMC_calc_chr_bank_offset(map->cart, idx, 0x0400);
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

    panic("unable to read at address 0x%04x from MMC3 mapper", addr);
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

    panic("unable to write at address 0x%04x from MMC3 mapper [0x%02x]", addr,
          value);
}

void MMC3_serialize(void* _map, FILE* fout)
{
    MMC3* map = (MMC3*)_map;

    Buffer res = buf_malloc(sizeof(MMC3));
    memcpy(res.buffer, map, res.size);
    MMC3* map_cpy = (MMC3*)res.buffer;
    map_cpy->cart = NULL;

    dump_buffer(&res, fout);
    free(res.buffer);
}

void MMC3_deserialize(void* _map, FILE* fin)
{
    Buffer buf = read_buffer(fin);
    if (buf.size != sizeof(MMC3))
        panic("MMC3_deserialize(): invalid buffer");

    MMC3* map = (MMC3*)_map;

    void* tmp_cart = map->cart;
    memcpy(map, buf.buffer, buf.size);
    map->cart = tmp_cart;
    free(buf.buffer);
}

// Polymorphic Mapper
Mapper* mapper_build(Cartridge* cart)
{
    Mapper* map = malloc_or_fail(sizeof(Mapper));
    switch (cart->mapper) {
        case 0: {
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
