#include "memory.h"
#include "alloc.h"
#include "system.h"
#include "mapper.h"
#include "logging.h"
#include "cartridge.h"
#include "ppu.h"

#include <assert.h>

typedef struct InternalMemory {
    struct System* sys;
} InternalMemory;

typedef struct InternalStandaloneMemory {
    uint8_t* buf;
} InternalStandaloneMemory;

static InternalMemory* internal_memory_build(System* sys)
{
    InternalMemory* res = malloc_or_fail(sizeof(InternalMemory));
    res->sys            = sys;
    return res;
}

static void internal_memory_destroy(void* _mem)
{
    InternalMemory* mem = (InternalMemory*)_mem;
    free(mem);
}

static InternalStandaloneMemory* _standalone_memory_build()
{
    InternalStandaloneMemory* res =
        malloc_or_fail(sizeof(InternalStandaloneMemory));
    res->buf = malloc_or_fail(0x10000);
    return res;
}

static void standalone_memory_destroy(void* _mem)
{
    InternalStandaloneMemory* mem = (InternalStandaloneMemory*)_mem;
    free(mem->buf);
    free(mem);
}

static uint8_t cpu_memory_read(void* _mem, uint16_t addr)
{
    InternalMemory* mem = (InternalMemory*)_mem;
    Ppu*            ppu = mem->sys->ppu;

    if (addr < 0x2000) {
        return mem->sys->RAM[addr % 0x800];
    }
    if (addr < 0x4000) {
        return ppu_read_register(ppu, 0x2000u + addr % 8);
    }
    if (addr == 0x4014) {
        return ppu_read_register(ppu, addr);
    }
    if (addr == 0x4015) {
        warning("0x%04x: APU read not currently supported", addr);
        return 0;
    }
    if (addr == 0x4016) {
        warning("0x%04x: Controller read not currently supported", addr);
        return 0;
    }
    if (addr == 0x4017) {
        warning("0x%04x: Controller read not currently supported", addr);
        return 0;
    }
    if (addr < 0x6000) {
        warning("0x%04x: I/O read not currently supported", addr);
        return 0;
    }
    if (addr >= 0x6000) {
        return mapper_read(mem->sys->mapper, addr);
    }

    panic("Invalid read @ 0x%04x", addr);
}

static void cpu_memory_write(void* _mem, uint16_t addr, uint8_t value)
{
    InternalMemory* mem = (InternalMemory*)_mem;
    Ppu*            ppu = mem->sys->ppu;

    if (addr < 0x2000) {
        mem->sys->RAM[addr % 0x800] = value;
        return;
    }
    if (addr < 0x4000) {
        ppu_write_register(ppu, 0x2000u + addr % 8, value);
        return;
    }
    if (addr < 0x4014) {
        warning("0x%04x [0x%02x]: APU write not currently supported", addr,
                value);
        return;
    }
    if (addr == 0x4014) {
        ppu_write_register(ppu, addr, value);
        return;
    }
    if (addr == 0x4015) {
        warning("0x%04x [0x%02x]: APU write not currently supported", addr,
                value);
        return;
    }
    if (addr == 0x4016) {
        warning("0x%04x [0x%02x]: Controller write not currently supported",
                addr, value);
        return;
    }
    if (addr == 0x4017) {
        warning("0x%04x [0x%02x]: APU write not currently supported", addr,
                value);
        return;
    }
    if (addr < 0x6000) {
        warning("0x%04x [0x%02x]: I/O write not currently supported", addr,
                value);
        return;
    }
    if (addr >= 0x6000) {
        mapper_write(mem->sys->mapper, addr, value);
        return;
    }

    panic("Invalid write @ 0x%04x [0x%02x]", addr, value);
}

static uint16_t ppu_mirror_tab[3][4] = {
    {0, 0, 1, 1}, {0, 1, 0, 1}, {1, 1, 1, 1}};

static uint16_t mirror_address(uint8_t mirror_type, uint16_t addr)
{
    assert(mirror_type >= 0 && mirror_type <= 2);

    addr         = (addr - 0x2000) % 0x1000;
    uint16_t tab = addr / 0x400;
    uint16_t off = addr % 0x400;
    return 0x2000 + ppu_mirror_tab[mirror_type][tab] * 0x400 + off;
}

static uint8_t read_palette(Ppu* ppu, uint16_t addr)
{
    if (addr >= 16 && addr % 4 == 0)
        addr -= 16;
    return ppu->palette_data[addr];
}

static void write_palette(Ppu* ppu, uint16_t addr, uint8_t val)
{
    if (addr >= 16 && addr % 4 == 0)
        addr -= 16;
    ppu->palette_data[addr] = val;
}

static uint8_t ppu_memory_read(void* _mem, uint16_t addr)
{
    InternalMemory* mem  = (InternalMemory*)_mem;
    Ppu*            ppu  = mem->sys->ppu;
    Cartridge*      cart = mem->sys->cart;

    addr = addr % 0x4000;
    if (addr < 0x2000) {
        // [ 0x0000 -> 0x1FFF ] Pattern Memory (CHR)
        return mapper_read(mem->sys->mapper, addr);
    }
    if (addr < 0x3F00) {
        // Nametable Memory (VRAM) [ 0x2000 -> 0x3EFF ]
        return ppu->nametable_data[mirror_address(cart->mirror, addr) % 2048u];
    }
    if (addr < 0x4000) {
        // Palette Memory
        return read_palette(ppu, addr % 32);
    }

    panic("Invalid read in PPU @ 0x%04x", addr);
}

static void ppu_memory_write(void* _mem, uint16_t addr, uint8_t value)
{
    InternalMemory* mem  = (InternalMemory*)_mem;
    Ppu*            ppu  = mem->sys->ppu;
    Cartridge*      cart = mem->sys->cart;

    addr = addr % 0x4000;
    if (addr < 0x2000) {
        mapper_write(mem->sys->mapper, addr, value);
        return;
    }
    if (addr < 0x3F00) {
        ppu->nametable_data[mirror_address(cart->mirror, addr) % 2048u] = value;
        return;
    }
    if (addr < 0x4000) {
        write_palette(ppu, addr % 32, value);
        return;
    }

    panic("Invalid write in PPU @ 0x%04x [0x%02x]", addr, value);
}

static uint8_t standalone_memory_read(void* _mem, uint16_t addr)
{
    InternalStandaloneMemory* mem = (InternalStandaloneMemory*)_mem;
    return mem->buf[addr];
}

static void standalone_memory_write(void* _mem, uint16_t addr, uint8_t value)
{
    InternalStandaloneMemory* mem = (InternalStandaloneMemory*)_mem;
    mem->buf[addr]                = value;
}

Memory* cpu_memory_build(struct System* sys)
{
    Memory* mem  = malloc_or_fail(sizeof(Memory));
    mem->read    = &cpu_memory_read;
    mem->write   = &cpu_memory_write;
    mem->destroy = &internal_memory_destroy;
    mem->obj     = internal_memory_build(sys);

    return mem;
}

Memory* ppu_memory_build(struct System* sys)
{
    Memory* mem  = malloc_or_fail(sizeof(Memory));
    mem->read    = &ppu_memory_read;
    mem->write   = &ppu_memory_write;
    mem->destroy = &internal_memory_destroy;
    mem->obj     = internal_memory_build(sys);

    return mem;
}

Memory* standalone_memory_build()
{
    Memory* mem  = malloc_or_fail(sizeof(Memory));
    mem->read    = &standalone_memory_read;
    mem->write   = &standalone_memory_write;
    mem->destroy = &standalone_memory_destroy;
    mem->obj     = _standalone_memory_build();

    return mem;
}

void memory_destroy(Memory* mem)
{
    mem->destroy(mem->obj);
    free(mem);
}

uint8_t memory_read(Memory* mem, uint16_t addr)
{
    return mem->read(mem->obj, addr);
}

void memory_write(Memory* mem, uint16_t addr, uint8_t value)
{
    mem->write(mem->obj, addr, value);
}
