#include <cifuzz/cifuzz.h>
#include <assert.h>

#include "common.h"

#include "../6502_cpu.h"
#include "../memory.h"

#define N 10000L

FUZZ_TEST_SETUP()
{
    // Perform any one-time setup required by the FUZZ_TEST function.
}

FUZZ_TEST(const uint8_t* data, size_t size)
{
    static Cpu* cpu = NULL;

    int result = setjmp(env);
    if (result != 0) {
        return;
    }

    if (size < 4)
        return;
    if (size > 0x10000 + 4)
        return;

    cpu = cpu_standalone_build();

    uint8_t a     = data[0];
    uint8_t x     = data[1];
    uint8_t y     = data[2];
    uint8_t flags = data[3];

    cpu->A     = a;
    cpu->X     = x;
    cpu->Y     = y;
    cpu->flags = flags;
    cpu->SP    = 0xFD;

    for (size_t i = 4; i < size; ++i)
        memory_write(cpu->mem, i, data[i]);

    for (long i = 0; i < N; ++i)
        cpu_step(cpu);

    cpu_destroy(cpu);
}
