#include "007_axrom.h"
#include "mapper_common.h"

AxROM* AxROM_build(Cartridge* cart)
{
    AxROM* map = calloc_or_fail(sizeof(AxROM));
    map->cart  = cart;
    return map;
}

uint8_t AxROM_read(void* _map, uint16_t addr)
{
    AxROM* map = (AxROM*)_map;
    if (addr < 0x2000) {
        return map->cart->CHR[addr];
    }
    if (addr >= 0x8000) {
        int32_t idx = map->prg_bank * 0x8000 + (addr - 0x8000);
        check_inbound(idx, map->cart->PRG_size);
        return map->cart->PRG[idx];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in AxROM mapper", addr);
    return 0;
}

void AxROM_write(void* _map, uint16_t addr, uint8_t value)
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
