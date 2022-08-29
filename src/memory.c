#include "memory.h"
#include "alloc.h"
#include "system.h"
#include "mapper.h"
#include "logging.h"

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
    if (addr < 0x2000) {
        return mem->sys->RAM[addr % 0x800];
    }
    if (addr < 0x4000) {
        warning("0x%04x: PPU read not currently supported", addr);
        return 0;
    }
    if (addr == 0x4014) {
        warning("0x%04x: PPU read not currently supported", addr);
        return 0;
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
    if (addr < 0x2000) {
        mem->sys->RAM[addr % 0x800] = value;
        return;
    }
    if (addr < 0x4000) {
        warning("0x%04x [0x%02x]: PPU write not currently supported", addr,
                value);
        return;
    }
    if (addr < 0x4014) {
        warning("0x%04x [0x%02x]: APU write not currently supported", addr,
                value);
        return;
    }
    if (addr == 0x4014) {
        warning("0x%04x [0x%02x]: PPU write not currently supported", addr,
                value);
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

static uint8_t ppu_memory_read(void* _mem, uint16_t addr)
{
    InternalMemory* mem = (InternalMemory*)_mem;
    addr                = addr % 0x4000;
    if (addr < 0x2000) {
        return mapper_read(mem->sys->mapper, addr);
    }
    if (addr < 0x3F00) {
        panic("PPU @ 0x%04x: currently unsupported read", addr);
    }
    if (addr < 0x4000) {
        panic("PPU @ 0x%04x: currently unsupported read", addr);
    }

    panic("Invalid read in PPU @ 0x%04x", addr);
}

static void ppu_memory_write(void* _mem, uint16_t addr, uint8_t value)
{
    InternalMemory* mem = (InternalMemory*)_mem;
    addr                = addr % 0x4000;
    if (addr < 0x2000) {
        mapper_write(mem->sys->mapper, addr, value);
    }
    if (addr < 0x3F00) {
        panic("PPU @ 0x%04x: currently unsupported write [0x%02x]", addr,
              value);
    }
    if (addr < 0x4000) {
        panic("PPU @ 0x%04x: currently unsupported write [0x%02x]", addr,
              value);
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
