#include "../src/system.h"
#include "../src/6502_cpu.h"
#include "../src/memory.h"
#include "../src/ppu.h"
#include "../src/cartridge.h"
#include "../src/mapper.h"

#include "common.h"

#define N 1000L

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    int result = setjmp(env);
    if (result != 0)
        return 1;

    if (size < 4)
        return 2;
    if (size > 0x10000 + 4)
        return 3;

    Cpu* cpu = cpu_standalone_build();

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

#ifdef AFL
int main(int argc, char const* argv[])
{
    if (argc < 2)
        return 1;

    Buffer b = read_file_raw(argv[1]);
    return LLVMFuzzerTestOneInput(b.buffer, b.size);
}
#endif
