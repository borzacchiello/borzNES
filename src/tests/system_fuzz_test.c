#include <cifuzz/cifuzz.h>
#include <assert.h>

#include "common.h"

#define N 1000L

FUZZ_TEST_SETUP()
{
    // Perform any one-time setup required by the FUZZ_TEST function.
}

FUZZ_TEST(const uint8_t* data, size_t size)
{
    static System* sys = NULL;

    int result = setjmp(env);
    if (result != 0) {
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
