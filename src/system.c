#include "system.h"
#include "alloc.h"
#include "cartridge.h"
#include "6502_cpu.h"
#include "mapper.h"
#include "ppu.h"

#include <stdio.h>

System* system_build(const char* rom_path)
{
    System* sys = calloc_or_fail(sizeof(System));

    Cartridge* cart = cartridge_load(rom_path);
    Mapper*    map  = mapper_build(cart);
    Cpu*       cpu  = cpu_build(sys);
    Ppu*       ppu  = ppu_build(sys);

    sys->cart   = cart;
    sys->mapper = map;
    sys->cpu    = cpu;
    sys->ppu    = ppu;

    cpu_reset(cpu);
    ppu_reset(ppu);
    return sys;
}

void system_destroy(System* sys)
{
    cartridge_unload(sys->cart);
    mapper_destroy(sys->mapper);
    cpu_destroy(sys->cpu);
    free(sys);
}

uint64_t system_step(System* sys)
{
    uint64_t cpu_cycles = cpu_step(sys->cpu);
    uint64_t ppu_cycles = 3ul * cpu_cycles;

    for (uint64_t i = 0; i < ppu_cycles; ++i) {
        ppu_step(sys->ppu);
    }
    return cpu_cycles;
}

const char* system_tostring(System* sys)
{
    static char res[512];
    sprintf(res, "NES [ %s (%s), PC @ 0x%04x ]", sys->cart->fpath,
            sys->mapper->name, sys->cpu->PC);
    return res;
}
