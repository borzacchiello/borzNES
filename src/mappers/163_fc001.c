#include "163_fc001.h"
#include "mapper_common.h"

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

FC_001* FC_001_build(Cartridge* cart)
{
    FC_001* map = calloc_or_fail(sizeof(FC_001));
    map->cart   = cart;

    map->laststrobe = 1;
    map->reg0       = 3;
    map->reg3       = 7;
    FC_001_sync(map);
    return map;
}

uint8_t FC_001_read(void* _map, uint16_t addr)
{
    FC_001* map = (FC_001*)_map;
    if (addr < 0x2000) {
        int32_t base = map->bank_chr[addr / 0x1000];
        int32_t off  = addr % 0x1000;
        check_inbound(base + off, map->cart->CHR_size);
        return map->cart->CHR[base + off];
    }
    if (addr >= 0x8000) {
        uint16_t off = addr - 0x8000;
        check_inbound(map->bank_prg + off, map->cart->PRG_size);
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
        check_inbound(addr - 0x6000, map->cart->SRAM_size);
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in FC_001 mapper", addr);
    return 0;
}

void FC_001_write(void* _map, uint16_t addr, uint8_t value)
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
        check_inbound(addr - 0x6000, map->cart->SRAM_size);
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in FC_001 mapper [0x%02x]", addr, value);
}

void FC_001_step(void* _map, System* sys)
{
    FC_001* map = (FC_001*)_map;
    Ppu*    ppu = sys->ppu;

    if (sys->ppu->cycle != 280)
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
