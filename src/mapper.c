#include "mapper.h"
#include "alloc.h"
#include "cartridge.h"
#include "logging.h"

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
static void NROM_destroy(void* _map) { free(_map); }

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

static MMC1* MMC1_build(Cartridge* cart)
{
    MMC1* map = calloc_or_fail(sizeof(MMC1));
    map->cart = cart;

    return map;
}

static void MMC1_destroy(void* _map) { free(_map); }

static int32_t MMC1_calc_prg_bank_offset(MMC1* map, int32_t idx)
{
    if (idx >= 0x80)
        idx -= 0x100;

    idx %= map->cart->PRG_size / 0x4000;
    int32_t off = idx * 0x4000;
    if (off < 0)
        off += map->cart->PRG_size;
    return off;
}

static int32_t MMC1_calc_chr_bank_offset(MMC1* map, int32_t idx)
{
    if (idx >= 0x80)
        idx -= 0x100;

    idx %= map->cart->CHR_size / 0x1000;
    int32_t off = idx * 0x1000;
    if (off < 0)
        off += map->cart->CHR_size;
    return off;
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
            map->prg_offsets[1] = 0;
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
        case 1:
            map->cart->mirror = MIRROR_ALL;
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
            MMC1_write_register(map, addr, value);
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

// Polymorphic Mapper
Mapper* mapper_build(Cartridge* cart)
{
    Mapper* map = malloc_or_fail(sizeof(Mapper));
    switch (cart->mapper) {
        case 0: {
            NROM* nrom   = NROM_build(cart);
            map->obj     = nrom;
            map->name    = "NROM";
            map->read    = &NROM_read;
            map->write   = &NROM_write;
            map->destroy = &NROM_destroy;
            break;
        }
        case 1: {
            MMC1* mmc1   = MMC1_build(cart);
            map->obj     = mmc1;
            map->name    = "MMC1";
            map->read    = &MMC1_read;
            map->write   = &MMC1_write;
            map->destroy = &MMC1_destroy;
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
