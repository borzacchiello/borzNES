#include "009_mmc2.h"
#include "mapper_common.h"

MMC2* MMC2_build(Cartridge* cart)
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

uint8_t MMC2_read(void* _map, uint16_t addr)
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
        check_inbound(base + off, map->cart->CHR_size);
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
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0xC000) {
        int32_t base = MMC2_calc_prg_bank_offset(map, -2);
        int32_t off  = addr - 0xC000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0xA000) {
        int32_t base = MMC2_calc_prg_bank_offset(map, -3);
        int32_t off  = addr - 0xA000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x8000) {
        int32_t base = MMC2_calc_prg_bank_offset(map, map->prg_bank);
        int32_t off  = addr - 0x8000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in MMC2 mapper", addr);
    return 0;
}

void MMC2_write(void* _map, uint16_t addr, uint8_t value)
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
        check_inbound(addr - 0x6000, map->cart->SRAM_size);
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in MMC2 mapper [0x%02x]", addr, value);
}

GEN_SERIALIZER(MMC2)
GEN_DESERIALIZER(MMC2)
