#include "mapper.h"
#include "alloc.h"
#include "cartridge.h"
#include "logging.h"

typedef struct NROM {
    Cartridge* cart;
    int        prg_banks, prg_bank1, prg_bank2;
} NROM;

static NROM* NROM_build(Cartridge* cart)
{
    NROM* map      = malloc_or_fail(sizeof(NROM));
    map->cart      = cart;
    map->prg_banks = cart->PRG_size / 0x4000;
    map->prg_bank1 = 0;
    map->prg_bank2 = map->prg_banks - 1;

    return map;
}
static void NROM_destroy(void* _map) { free(_map); }

static uint8_t NROM_read(void* _map, uint16_t addr)
{
    NROM* map = (NROM*)_map;
    if (addr < 0x2000)
        return map->cart->CHR[addr];
    if (addr >= 0xC000) {
        int index = map->prg_bank2 * 0x4000 + (addr - 0xC000);
        return map->cart->PRG[index];
    }
    if (addr >= 0x8000) {
        int index = map->prg_bank1 * 0x4000 + (addr - 0x8000);
        return map->cart->PRG[index];
    }
    if (addr >= 0x6000) {
        int index = addr - 0x6000;
        return map->cart->SRAM[index];
    }

    panic("unable to read at address 0x%04x from NROM mapper", addr);
}

static void NROM_write(void* _map, uint16_t addr, uint8_t value)
{
    NROM* map = (NROM*)_map;
    if (addr < 0x2000) {
        map->cart->CHR[addr] = value;
    } else if (addr >= 0x8000) {
        map->prg_bank1 = (int)value % map->prg_banks;
    } else if (addr >= 0x6000) {
        int index              = addr - 0x6000;
        map->cart->SRAM[index] = value;
    }

    panic("unable to write at address 0x%04x from NROM mapper", addr);
}

Mapper* mapper_build(Cartridge* cart)
{
    switch (cart->mapper) {
        case 0: {
            NROM*   nrom = NROM_build(cart);
            Mapper* map  = malloc_or_fail(sizeof(Mapper));
            map->obj     = nrom;
            map->read    = &NROM_read;
            map->write   = &NROM_write;
            map->destroy = &NROM_destroy;
            return map;
        }
        default:
            panic("unsupported mapper %u", cart->mapper);
    }

    // unreachable
    return NULL;
}

void mapper_destroy(Mapper* map)
{
    map->destroy(map->obj);
    free(map);
}

uint8_t mapper_read(Mapper* map, uint16_t addr)
{
    return map->read(map->obj, addr);
}

void mapper_write(Mapper* map, uint16_t addr, uint8_t value)
{
    return map->write(map->obj, addr, value);
}
