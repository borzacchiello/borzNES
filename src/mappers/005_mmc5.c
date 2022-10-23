#include "005_mmc5.h"
#include "mapper_common.h"

#define EXTRA_NAMETABLE_MODE    0
#define EXTENDED_ATTRIBUTE_MODE 1
#define CPU_ACCESS_MODE         2
#define CPU_READONLY_MODE       3

MMC5* MMC5_build(struct Cartridge* cart)
{
    warning("Mapper MMC5 is WIP, not all features have been implemented");

    MMC5* map = calloc_or_fail(sizeof(MMC5));
    map->cart = cart;

    // Force every MMC5 game to have 64k of RAM
    cart->SRAM_size = 2 << 15;
    cart->SRAM      = realloc_or_fail(cart->SRAM, cart->SRAM_size);

    map->prg_regs[4] = 0xff;

    map->read_only_ram = 1;
    map->chr_A_mode    = 1;
    return map;
}

static int32_t get_chr_offset(MMC5* map, uint16_t addr)
{
    int32_t   off  = 0;
    uint16_t* regs = map->chr_A_mode ? map->chr_regs_A : map->chr_regs_B;

    static uint16_t conv_tab_1[] = {3, 7};
    static uint16_t conv_tab_2[] = {1, 3, 5, 7};

    switch (map->chr_mode) {
        case 0: {
            int32_t base = regs[7];
            base         = calc_chr_bank_offset(map->cart, base, 0x2000);
            off          = addr;
            break;
        }
        case 1: {
            int32_t base = regs[conv_tab_1[addr / 0x1000]];
            base         = calc_chr_bank_offset(map->cart, base, 0x1000);
            off          = addr % 0x1000 + base;
            break;
        }
        case 2: {
            int32_t base = regs[conv_tab_2[addr / 0x800]];
            base         = calc_chr_bank_offset(map->cart, base, 0x800);
            off          = addr % 0x800 + base;
            break;
        }
        case 3: {
            int32_t base = regs[addr / 0x400];
            base         = calc_chr_bank_offset(map->cart, base, 0x400);
            off          = addr % 0x400 + base;
            break;
        }
    }
    return off;
}

static int32_t get_prg_offset(MMC5* map, uint16_t addr, int* in_ram)
{
    if (addr < 0x8000) {
        *in_ram = 0;
        return map->prg_regs[0] + (addr - 0x6000) % 0x2000;
    }

    int32_t off = 0;
    switch (map->prg_mode) {
        case 0: {
            *in_ram      = (map->prg_regs[4] & 0x80) ? 0 : 1;
            int32_t base = (map->prg_regs[4] & 0x7f) >> 2;
            if (*in_ram)
                base = calc_ram_bank_offset(map->cart, base, 0x8000);
            else
                base = calc_prg_bank_offset(map->cart, base, 0x8000);
            off = base + addr - 0x8000;
            break;
        }
        case 1: {
            int32_t base;
            if (addr < 0xC000) {
                *in_ram = (map->prg_regs[2] & 0x80) ? 0 : 1;
                base    = (map->prg_regs[2] & 0x7f) >> 1;
                if (*in_ram)
                    base = calc_ram_bank_offset(map->cart, base, 0x4000);
                else
                    base = calc_prg_bank_offset(map->cart, base, 0x4000);
                off = base + addr - 0x8000;
            } else {
                *in_ram = (map->prg_regs[4] & 0x80) ? 0 : 1;
                base    = (map->prg_regs[4] & 0x7f) >> 1;
                if (*in_ram)
                    base = calc_ram_bank_offset(map->cart, base, 0x4000);
                else
                    base = calc_prg_bank_offset(map->cart, base, 0x4000);
                off = base + addr - 0xC000;
            }
            break;
        }
        case 2: {
            int32_t base;
            if (addr < 0xC000) {
                *in_ram = (map->prg_regs[2] & 0x80) ? 0 : 1;
                base    = (map->prg_regs[2] & 0x7f) >> 1;
                if (*in_ram)
                    base = calc_ram_bank_offset(map->cart, base, 0x4000);
                else
                    base = calc_prg_bank_offset(map->cart, base, 0x4000);
                off = base + addr - 0x8000;
            } else if (addr < 0xE000) {
                *in_ram = (map->prg_regs[3] & 0x80) ? 0 : 1;
                base    = map->prg_regs[3] & 0x7f;
                if (*in_ram)
                    base = calc_ram_bank_offset(map->cart, base, 0x2000);
                else
                    base = calc_prg_bank_offset(map->cart, base, 0x2000);
                off = base + addr - 0xC000;
            } else {
                *in_ram = (map->prg_regs[4] & 0x80) ? 0 : 1;
                base    = map->prg_regs[4] & 0x7f;
                if (*in_ram)
                    base = calc_ram_bank_offset(map->cart, base, 0x2000);
                else
                    base = calc_prg_bank_offset(map->cart, base, 0x2000);
                off = base + addr - 0xE000;
            }
            break;
        }
        case 3: {
            *in_ram = (map->prg_regs[(addr - 0x6000) / 0x2000] & 0x80) ? 0 : 1;
            int32_t base = map->prg_regs[(addr - 0x6000) / 0x2000] & 0x7f;
            if (*in_ram)
                base = calc_ram_bank_offset(map->cart, base, 0x2000);
            else
                base = calc_prg_bank_offset(map->cart, base, 0x2000);
            off = base + addr % 0x2000;
            break;
        }
    }
    return off;
}

static uint8_t MMC5_read_reg(MMC5* map, uint16_t addr)
{
    switch (addr) {
        case 0x5204: {
            uint8_t v = map->irq_pending << 7;
            if (map->scanline >= 0 && map->scanline <= 239)
                v |= (uint8_t)1 << 6;
            map->irq_pending = 0;
            return v;
        }
        case 0x5205: {
            uint16_t res =
                (uint16_t)map->multiplicand * (uint16_t)map->multiplier;
            return res & 0xff;
        }
        case 0x5206: {
            uint16_t res =
                (uint16_t)map->multiplicand * (uint16_t)map->multiplier;
            return (res >> 8) & 0xff;
        }
        default:
            warning("unsupported reg 0x%04x in MMC5 mapper [read]", addr);
    }
    return 0;
}

uint8_t MMC5_read(void* _map, uint16_t addr)
{
    MMC5* map = (MMC5*)_map;
    if (addr < 0x2000) {
        int32_t off = get_chr_offset(map, addr);
        check_inbound(off, map->cart->CHR_size);
        return map->cart->CHR[off];
    }
    if (addr >= 0x4020 && addr < 0x5C00) {
        return MMC5_read_reg(map, addr);
    }
    if (addr >= 0x6000) {
        int     in_ram;
        int32_t off = get_prg_offset(map, addr, &in_ram);
        if (in_ram) {
            check_inbound(off, map->cart->SRAM_size);
            return map->cart->SRAM[off];
        }
        check_inbound(off, map->cart->PRG_size);
        return map->cart->PRG[off];
    }
    if (addr >= 0x5C00 && addr < 0x6000) {
        // FIXME: I should check ExMode
        return map->ExRAM[addr - 0x5C00];
    }

    warning("unexpected read at address 0x%04x from MMC5 mapper", addr);
    return 0;
}

static void MMC5_write_reg(MMC5* map, uint16_t addr, uint8_t value)
{
    switch (addr) {
        case 0x5100:
            map->prg_mode = value & 3;
            break;
        case 0x5101:
            map->chr_mode = value & 3;
            break;
        case 0x5102:
            map->read_only_ram = (value & 3) != 1;
            break;
        case 0x5103:
            map->read_only_ram = (value & 3) != 2;
            break;
        case 0x5104:
            map->exram_mode = value & 3;
            if (map->exram_mode == 1)
                warning("ExRAM mode 1 is not implemented");
            break;
        case 0x5105: {
            map->nametab_reg = value;
            break;
        }
        case 0x5106:
            map->fill_tile = value;
            break;
        case 0x5107:
            map->fill_attr = value & 2;
            break;
        case 0x5113:
            map->prg_regs[0] = value & 7;
            break;
        case 0x5114 ... 0x5116: {
            uint8_t off        = addr - 0x5113;
            map->prg_regs[off] = value;
            break;
        }
        case 0x5117:
            map->prg_regs[4] = (value & 0x7f) | (1 << 7);
            break;
        case 0x5120 ... 0x5127: {
            uint8_t off          = addr - 0x5120;
            map->chr_regs_A[off] = (uint16_t)value | (map->high_chr_reg << 8);
            break;
        }
        case 0x5128 ... 0x512B: {
            uint8_t  off         = addr - 0x5128;
            uint16_t v           = (uint16_t)value | (map->high_chr_reg << 8);
            map->chr_regs_B[off] = map->chr_regs_B[off + 4] = v;
            break;
        }
        case 0x5130:
            map->high_chr_reg = value & 3;
            break;
        case 0x5203:
            map->irq_target = value;
            break;
        case 0x5204:
            map->irq_enable = (value & 0x80) ? 1 : 0;
            break;
        case 0x5205:
            map->multiplicand = value;
            break;
        case 0x5206:
            map->multiplier = value;
            break;
        default:
            warning("unsupported reg 0x%04x in MMC5 mapper [value: 0x%02x]",
                    addr, value);
    }
}

void MMC5_write(void* _map, uint16_t addr, uint8_t value)
{
    MMC5* map = (MMC5*)_map;
    if (addr < 0x2000) {
        int32_t off = get_chr_offset(map, addr);
        check_inbound(off, map->cart->CHR_size);
        map->cart->CHR[off] = value;
        return;
    }
    if (addr >= 0x4020 && addr < 0x5C00) {
        MMC5_write_reg(map, addr, value);
        return;
    }
    if (addr >= 0x5C00 && addr < 0x6000) {
        // FIXME: I should check ExMode
        map->ExRAM[addr - 0x5C00] = value;
        return;
    }
    if (addr >= 0x6000) {
        // I should check whether it is writable
        int     in_ram;
        int32_t off = get_prg_offset(map, addr, &in_ram);
        if (in_ram) {
            check_inbound(off, map->cart->SRAM_size);
            map->cart->SRAM[off] = value;
        }
        return;
    }

    warning("unexpected write at address 0x%04x from MMC5 mapper [0x%02x]",
            addr, value);
}

static uint8_t get_nametab_reg_value(MMC5* map, uint8_t tab)
{
    switch (tab) {
        case 0:
            return map->nametab_reg & 3;
        case 1:
            return (map->nametab_reg >> 2) & 3;
        case 2:
            return (map->nametab_reg >> 4) & 3;
        case 3:
            return (map->nametab_reg >> 6) & 3;
        default:
            break;
    }

    panic("get_nametab_reg_value(): unreachable code");
}

uint8_t MMC5_nametable_read(void* _map, struct Ppu* ppu, uint16_t addr)
{
    assert(addr >= 0x2000 && addr <= 0x3EFF);

    MMC5* map = (MMC5*)_map;

    addr         = (addr - 0x2000) % 0x1000;
    uint16_t tab = addr / 0x400;
    uint16_t off = addr % 0x400;

    switch (get_nametab_reg_value(map, tab)) {
        case 0:
            // NTA
            return ppu->nametable_data[off];
        case 1:
            // NTB
            return ppu->nametable_data[0x400 + off];
        case 2:
            // ExRAM
            if (map->exram_mode == 0 || map->exram_mode == 1)
                return map->ExRAM[off];
            return 0;
        case 3:
            // FILL
            return map->fill_tile;
    }

    panic("MMC5_nametable_read(): unreachable");
}

void MMC5_nametable_write(void* _map, struct Ppu* ppu, uint16_t addr,
                          uint8_t value)
{
    assert(addr >= 0x2000 && addr <= 0x3EFF);

    MMC5* map = (MMC5*)_map;

    addr         = (addr - 0x2000) % 0x1000;
    uint16_t tab = addr / 0x400;
    uint16_t off = addr % 0x400;

    switch (get_nametab_reg_value(map, tab)) {
        case 0:
            // NTA
            ppu->nametable_data[off] = value;
            return;
        case 1:
            // NTB
            ppu->nametable_data[0x400 + off] = value;
            return;
        case 2:
        case 3:
            return;
    }

    panic("MMC5_nametable_read(): unreachable");
}

void MMC5_step(void* _map, System* sys)
{
    MMC5* map = (MMC5*)_map;

    map->scanline = sys->ppu->scanline;
    if (map->irq_enable && map->scanline != 0 && map->scanline < 240) {
        // This is a *very* simplified version of its actual behavior
        if (map->scanline == map->irq_target) {
            cpu_trigger_irq(sys->cpu);
        }
    }
}

void MMC5_notify_fetching(void* _map, Ppu* ppu, FetchingTarget ft)
{
    MMC5* map = (MMC5*)_map;

    if (ppu->ctrl_flags.sprite_size) {
        if (ft == FETCHING_BACKGROUND)
            map->chr_A_mode = 0;
        else
            map->chr_A_mode = 1;
    }
}

GEN_SERIALIZER(MMC5)
GEN_DESERIALIZER(MMC5)
