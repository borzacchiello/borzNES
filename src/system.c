#include "system.h"
#include "alloc.h"
#include "cartridge.h"
#include "6502_cpu.h"
#include "mapper.h"
#include "ppu.h"
#include "apu.h"
#include "logging.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

static char* get_state_path(const char* fpath)
{
    size_t fpath_size = strlen(fpath);
    char*  res        = calloc_or_fail(fpath_size + 7);
    strcpy(res, fpath);
    strcat(res, ".state");

    return res;
}

System* system_build(const char* rom_path)
{
    System* sys = calloc_or_fail(sizeof(System));

    Cartridge* cart = cartridge_load(rom_path);
    Mapper*    map  = mapper_build(cart);
    Cpu*       cpu  = cpu_build(sys);
    Ppu*       ppu  = ppu_build(sys);
    Apu*       apu  = apu_build(sys);

    sys->cart            = cart;
    sys->mapper          = map;
    sys->cpu             = cpu;
    sys->ppu             = ppu;
    sys->apu             = apu;
    sys->cpu_freq        = CPU_1X_FREQ;
    sys->state_save_path = get_state_path(rom_path);

    cpu_reset(cpu);
    ppu_reset(ppu);
    apu_unpause(apu);
    return sys;
}

void system_destroy(System* sys)
{
    cartridge_unload(sys->cart);
    mapper_destroy(sys->mapper);
    cpu_destroy(sys->cpu);
    ppu_destroy(sys->ppu);
    apu_destroy(sys->apu);
    free(sys->state_save_path);
    free(sys);
}

uint64_t system_step(System* sys)
{
    uint64_t cpu_cycles = cpu_step(sys->cpu);
    uint64_t ppu_cycles = 3ul * cpu_cycles;
    uint64_t apu_cycles = cpu_cycles;

    for (uint64_t i = 0; i < ppu_cycles; ++i) {
        ppu_step(sys->ppu);
        mapper_step(sys->mapper, sys);
    }

    for (uint64_t i = 0; i < apu_cycles; ++i) {
        apu_step(sys->apu);
    }
    return cpu_cycles;
}

void system_step_ms(System* sys, int64_t delta_time_ms)
{
    int64_t cycles = (int64_t)(sys->cpu_freq * delta_time_ms / 1000000l);
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

void system_save_state(System* sys, const char* path)
{
    FILE* fout = fopen(path, "wb");
    if (fout == NULL)
        panic("unable to open the file %s", path);

    Buffer ram_state = {.buffer = (uint8_t*)&sys->RAM,
                        .size   = sizeof(sys->RAM)};
    dump_buffer(&ram_state, fout);

    cartridge_serialize(sys->cart, fout);
    cpu_serialize(sys->cpu, fout);
    ppu_serialize(sys->ppu, fout);
    mapper_serialize(sys->mapper, fout);

    fclose(fout);
}

void system_load_state(System* sys, const char* path)
{
    if (access(path, F_OK))
        // The file does not exist
        return;

    FILE* fin = fopen(path, "rb");
    if (fin == NULL)
        panic("unable to open the file %s", path);

    Buffer ram_state = read_buffer(fin);
    if (ram_state.size != sizeof(sys->RAM))
        panic("system_load_state(): invalid buffer");
    memcpy(sys->RAM, ram_state.buffer, ram_state.size);
    free(ram_state.buffer);

    cartridge_deserialize(sys->cart, fin);
    cpu_deserialize(sys->cpu, fin);
    ppu_deserialize(sys->ppu, fin);
    mapper_deserialize(sys->mapper, fin);

    fclose(fin);
}
