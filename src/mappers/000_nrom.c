#include "000_nrom.h"
#include "mapper_common.h"

NROM* NROM_build(Cartridge* cart)
{
    NROM* map      = malloc_or_fail(sizeof(NROM));
    map->cart      = cart;
    map->prg_banks = cart->PRG_size / 0x4000;
    map->prg_bank1 = 0;
    map->prg_bank2 = map->prg_banks - 1;

    return map;
}

uint8_t NROM_read(void* _map, uint16_t addr)
{
    NROM* map = (NROM*)_map;
    if (addr < 0x2000)
        return map->cart->CHR[addr];
    if (addr >= 0xC000) {
        int index = map->prg_bank2 * 0x4000 + (addr - 0xC000);
        check_inbound(index, map->cart->PRG_size);
        return map->cart->PRG[index];
    }
    if (addr >= 0x8000) {
        int index = map->prg_bank1 * 0x4000 + (addr - 0x8000);
        check_inbound(index, map->cart->PRG_size);
        return map->cart->PRG[index];
    }
    if (addr >= 0x6000) {
        int index = addr - 0x6000;
        check_inbound(index, map->cart->SRAM_size);
        return map->cart->SRAM[index];
    }

    warning("unexpected read at address 0x%04x from NROM mapper", addr);
    return 0;
}

void NROM_write(void* _map, uint16_t addr, uint8_t value)
{
    NROM* map = (NROM*)_map;
    if (addr < 0x2000) {
        check_inbound(addr, map->cart->CHR_size);
        map->cart->CHR[addr] = value;
    } else if (addr >= 0x8000) {
        map->prg_bank1 = (int)value % map->prg_banks;
    } else if (addr >= 0x6000) {
        int index = addr - 0x6000;
        check_inbound(index, map->cart->SRAM_size);
        map->cart->SRAM[index] = value;
    } else {
        warning("unexpected write at address 0x%04x from NROM mapper [0x%02x]",
                addr, value);
    }
}

static void NROM_postcheck(void* _map)
{
    NROM* map = (NROM*)_map;

    if (map->prg_banks == 0)
        panic("NROM: invalid prg_banks");
}

GEN_SERIALIZER(NROM)
GEN_DESERIALIZER_WITH_POSTCHECK(NROM, NROM_postcheck)
