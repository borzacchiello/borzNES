#include "003_cnrom.h"
#include "mapper_common.h"

CNROM* CNROM_build(Cartridge* cart)
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

uint8_t CNROM_read(void* _map, uint16_t addr)
{
    CNROM* map = (CNROM*)_map;
    if (addr < 0x2000) {
        int32_t base = CNROM_calc_chr_bank_offset(map, map->chr_bank);
        int32_t off  = addr;
        check_inbound(base + off, map->cart->CHR_size);
        return map->cart->CHR[base + off];
    }
    if (addr >= 0xC000) {
        int32_t base = CNROM_calc_prg_bank_offset(map, -1);
        int32_t off  = addr - 0xC000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x8000) {
        int32_t base = 0;
        int32_t off  = addr - 0x8000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read @ 0x%04x in CNROM mapper", addr);
    return 0;
}

void CNROM_write(void* _map, uint16_t addr, uint8_t value)
{
    CNROM* map = (CNROM*)_map;
    if (addr < 0x2000) {
        int32_t base = CNROM_calc_chr_bank_offset(map, map->chr_bank);
        int32_t off  = addr;
        check_inbound(base + off, map->cart->CHR_size);
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
