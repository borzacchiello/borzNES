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

    sys->cart     = cart;
    sys->mapper   = map;
    sys->cpu      = cpu;
    sys->ppu      = ppu;
    sys->cpu_freq = CPU_1X_FREQ;

    cpu_reset(cpu);
    ppu_reset(ppu);
    return sys;
}

void system_destroy(System* sys)
{
    cartridge_unload(sys->cart);
    mapper_destroy(sys->mapper);
    cpu_destroy(sys->cpu);
    ppu_destroy(sys->ppu);
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

void system_step_ms(System* sys, int64_t delta_time_ms)
{
    int64_t cycles = (int64_t)(sys->cpu_freq / 1000l * delta_time_ms);
    while (cycles > 0)
        cycles -= system_step(sys);
}

void system_update_controller(System* sys, ControllerNum num,
                              ControllerState state)
{
    sys->controller_state[num] = state;
}

void system_load_controllers(System* sys)
{
    sys->controller_shift_reg[0] = sys->controller_state[0].state;
    sys->controller_shift_reg[1] = sys->controller_state[1].state;
}

uint8_t system_get_controller_val(System* sys, ControllerNum num)
{
    uint8_t res = sys->controller_shift_reg[num] & 1;
    sys->controller_shift_reg[num] >>= 1;
    return res;
}

const char* system_tostring(System* sys)
{
    static char res[512];
    sprintf(res, "NES [ %s (%s), PC @ 0x%04x ]", sys->cart->fpath,
            sys->mapper->name, sys->cpu->PC);
    return res;
}
