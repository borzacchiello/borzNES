#include "004_mmc3.h"
#include "mapper_common.h"

static inline int32_t MMC3_calc_prg_bank_offset(MMC3* map, int32_t idx)
{
    return calc_prg_bank_offset(map->cart, idx, 0x2000);
}

static inline int32_t MMC3_calc_chr_bank_offset(MMC3* map, int32_t idx)
{
    return calc_chr_bank_offset(map->cart, idx, 0x0400);
}

MMC3* MMC3_build(Cartridge* cart)
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

void MMC3_step(void* _map, System* sys)
{
    MMC3* map = (MMC3*)_map;

    if (sys->ppu->cycle != 280)
        return;
    if (sys->ppu->scanline >= 240 && sys->ppu->scanline <= 260)
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

uint8_t MMC3_read(void* _map, uint16_t addr)
{
    MMC3* map = (MMC3*)_map;
    if (addr < 0x2000) {
        int32_t base = map->chr_offsets[addr / 0x0400];
        int32_t off  = addr % 0x0400;
        check_inbound(base + off, map->cart->CHR_size);
        return map->cart->CHR[base + off];
    }
    if (addr >= 0x8000) {
        addr -= 0x8000;
        int32_t base = map->prg_offsets[addr / 0x2000];
        int32_t off  = addr % 0x2000;
        check_inbound(base + off, map->cart->PRG_size);
        return map->cart->PRG[base + off];
    }
    if (addr >= 0x6000) {
        return map->cart->SRAM[addr - 0x6000];
    }

    warning("unexpected read at address 0x%04x from MMC3 mapper", addr);
    return 0;
}

void MMC3_write(void* _map, uint16_t addr, uint8_t value)
{
    MMC3* map = (MMC3*)_map;
    if (addr < 0x2000) {
        int32_t base = map->chr_offsets[addr / 0x0400];
        int32_t off  = addr % 0x0400;
        check_inbound(base + off, map->cart->CHR_size);
        map->cart->CHR[base + off] = value;
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

    warning("unexpected write at address 0x%04x from MMC3 mapper [0x%02x]",
            addr, value);
}

GEN_SERIALIZER(MMC3)
GEN_DESERIALIZER(MMC3)
