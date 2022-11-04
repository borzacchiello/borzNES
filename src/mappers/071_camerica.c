#include "071_camerica.h"
#include "mapper_common.h"

Map071* Map071_build(Cartridge* cart)
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

uint8_t Map071_read(void* _map, uint16_t addr)
{
    Map071* map = (Map071*)_map;
    if (addr < 0x2000) {
        return map->cart->CHR[addr];
    }
    if (addr >= 0xC000) {
        uint32_t base = Map071_calc_prg_bank_offset(map, -1);
        uint32_t off  = addr - 0xC000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x8000) {
        uint32_t base = Map071_calc_prg_bank_offset(map, map->prg_bank);
        uint32_t off  = addr - 0x8000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        check_inbound(addr - 0x6000, map->cart->SRAM_size);
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in Map071 mapper", addr);
    return 0;
}

void Map071_write(void* _map, uint16_t addr, uint8_t value)
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
        check_inbound(addr - 0x6000, map->cart->SRAM_size);
        map->cart->SRAM[addr - 0x6000] = value;
        return;
    }

    warning("unexpected write @ 0x%04x in Map071 mapper [0x%02x]", addr, value);
}

GEN_SERIALIZER(Map071)
GEN_DESERIALIZER(Map071)
