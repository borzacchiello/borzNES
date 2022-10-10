#include "001_mmc1.h"
#include "mapper_common.h"

static inline int32_t MMC1_calc_prg_bank_offset(MMC1* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x4000);
}

static inline int32_t MMC1_calc_chr_bank_offset(MMC1* map, int32_t idx)
{
    return calc_chr_bank_offset(map->cart, idx, 0x1000);
}

MMC1* MMC1_build(Cartridge* cart)
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

uint8_t MMC1_read(void* _map, uint16_t addr)
{
    MMC1* map = (MMC1*)_map;
    if (addr < 0x2000) {
        int32_t base = map->chr_offsets[addr / 0x1000];
        int32_t off  = addr % 0x1000;
        check_inbound(base + off, map->cart->CHR_size);
        return map->cart->CHR[base + off];
    }
    if (addr >= 0x8000) {
        addr -= 0x8000;
        int32_t base = map->prg_offsets[addr / 0x4000];
        int32_t off  = addr % 0x4000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read at address 0x%04x from MMC1 mapper", addr);
    return 0;
}

void MMC1_write(void* _map, uint16_t addr, uint8_t value)
{
    MMC1* map = (MMC1*)_map;
    if (addr < 0x2000) {
        int32_t base = map->chr_offsets[addr / 0x1000];
        int32_t off  = addr % 0x1000;
        check_inbound(base + off, map->cart->CHR_size);
        map->cart->CHR[base + off] = value;
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
