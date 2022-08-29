#include "memory.h"
#include "alloc.h"
#include "system.h"
#include "logging.h"

Memory* memory_build(System* sys)
{
    Memory* res = malloc_or_fail(sizeof(Memory));
    res->sys    = sys;
    return res;
}

void memory_destroy(Memory* mem) { free(mem); }

uint8_t memory_read(Memory* mem, uint16_t addr)
{
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

void memory_write(Memory* mem, uint16_t addr, uint8_t value)
{
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
