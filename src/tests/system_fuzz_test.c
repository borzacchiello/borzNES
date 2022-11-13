#include <cifuzz/cifuzz.h>
#include <assert.h>

#include "common.h"

#include "../system.h"
#include "../6502_cpu.h"
#include "../memory.h"
#include "../ppu.h"
#include "../apu.h"
#include "../cartridge.h"
#include "../mapper.h"
#include "../alloc.h"

#define N 1000L

static System* mk_sys(const uint8_t* Data, size_t Size)
{
    Buffer  buf = {.buffer = (uint8_t*)Data, .size = Size};
    System* sys = calloc_or_fail(sizeof(System));

    Cartridge* cart = cartridge_load_from_buffer(buf);
    Mapper*    map  = mapper_build(cart);
    Cpu*       cpu  = cpu_build(sys);
    Ppu*       ppu  = ppu_build(sys);
    Apu*       apu  = calloc_or_fail(sizeof(Apu));

    sys->cart            = cart;
    sys->mapper          = map;
    sys->cpu             = cpu;
    sys->ppu             = ppu;
    sys->apu             = apu;
    sys->cpu_freq        = CPU_1X_FREQ;
    sys->state_save_path = NULL;

    cpu_reset(cpu);
    ppu_reset(ppu);

    return sys;
}

static void del_sys(System* sys)
{
    if (sys->cart)
        cartridge_unload(sys->cart);
    if (sys->mapper)
        mapper_destroy(sys->mapper);
    if (sys->cpu)
        cpu_destroy(sys->cpu);
    if (sys->ppu)
        ppu_destroy(sys->ppu);
    free(sys->apu);
    free(sys);
}

FUZZ_TEST_SETUP()
{
    // Perform any one-time setup required by the FUZZ_TEST function.
}

FUZZ_TEST(const uint8_t* data, size_t size)
{
    System* sys = NULL;

    int result = setjmp(env);
    if (result != 0) {
        if (sys)
            del_sys(sys);
        return;
    }

    sys = mk_sys(data, size);

    for (long i = 0; i < N; ++i) {
        uint64_t cpu_cycles = cpu_step(sys->cpu);
        uint64_t ppu_cycles = 3ul * cpu_cycles;

        for (uint64_t i = 0; i < ppu_cycles; ++i) {
            ppu_step(sys->ppu);
            mapper_step(sys->mapper, sys);
        }
    }

    del_sys(sys);
}
