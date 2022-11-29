#include <cifuzz/cifuzz.h>
#include <assert.h>
#include <stdio.h>

#include "common.h"
#include "roms.h"

#define N 1000L

static void load_state(System* sys, FILE* fin)
{
    Buffer ram_state = read_buffer(fin);
    if (ram_state.size != sizeof(sys->RAM))
        panic("system_load_state(): invalid buffer");
    memcpy(sys->RAM, ram_state.buffer, ram_state.size);
    free_or_fail(ram_state.buffer);

    cartridge_deserialize(sys->cart, fin);
    cpu_deserialize(sys->cpu, fin);
    ppu_deserialize(sys->ppu, fin);
    mapper_deserialize(sys->mapper, fin);
}

FUZZ_TEST_SETUP()
{
    // Perform any one-time setup required by the FUZZ_TEST function.
}

FUZZ_TEST(const uint8_t* data, size_t size)
{
    System* sys = NULL;
    FILE*   fin = NULL;

    int result = setjmp(env);
    if (result != 0) {
        if (sys)
            del_sys(sys);
        if (fin)
            fclose(fin);
        return;
    }

    sys = mk_sys((const uint8_t*)ROM_branch_timing_tests,
                 sizeof(ROM_branch_timing_tests));

    // FIXME: load the state from buffer... This is super slow
    static const char* fname = "/dev/shm/input.state";
    FILE*              fout  = fopen(fname, "wb");
    if (fout == NULL)
        abort();
    if (fwrite(data, 1, size, fout) != size)
        abort();
    if (fclose(fout) != 0)
        abort();

    fin = fopen(fname, "rb");
    if (fin == NULL)
        abort();
    load_state(sys, fin);
    fclose(fin);
    fin = NULL;

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
