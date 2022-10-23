#include "mapper_common.h"

int32_t calc_prg_bank_offset(Cartridge* cart, int32_t idx, uint32_t bank_size)
{
    if (cart->PRG_size % bank_size != 0)
        panic("calc_prg_bank_offset(): incorrect PRG bank_size");

    idx %= cart->PRG_size / bank_size;
    int32_t off = idx * bank_size;
    if (off < 0)
        off += cart->PRG_size;
    return off;
}

int32_t calc_chr_bank_offset(Cartridge* cart, int32_t idx, uint32_t bank_size)
{
    if (cart->CHR_size % bank_size != 0)
        panic("calc_chr_bank_offset(): incorrect CHR bank_size");

    idx %= cart->CHR_size / bank_size;
    int32_t off = idx * bank_size;
    if (off < 0)
        off += cart->CHR_size;
    return off;
}

int32_t calc_ram_bank_offset(Cartridge* cart, int32_t idx, uint32_t bank_size)
{
    if (cart->SRAM_size % bank_size != 0)
        panic("calc_ram_bank_offset(): incorrect CHR bank_size");

    idx %= cart->SRAM_size / bank_size;
    int32_t off = idx * bank_size;
    if (off < 0)
        off += cart->SRAM_size;
    return off;
}
