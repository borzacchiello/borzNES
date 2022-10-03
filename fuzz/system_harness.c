#include "../src/system.h"
#include "../src/6502_cpu.h"
#include "../src/memory.h"
#include "../src/ppu.h"
#include "../src/apu.h"
#include "../src/cartridge.h"
#include "../src/mapper.h"

#include "common.h"

#define N 1000L

static System* mk_sys(const uint8_t* Data, size_t Size)
{
    Buffer  buf = {.buffer = Data, .size = Size};
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
    cartridge_unload(sys->cart);
    mapper_destroy(sys->mapper);
    cpu_destroy(sys->cpu);
    ppu_destroy(sys->ppu);
    free(sys->apu);
    free(sys);
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    int result = setjmp(env);
    if (result != 0)
        return 1;

    System* sys = mk_sys(Data, Size);

    for (long i = 0; i < N; ++i) {
        uint64_t cpu_cycles = cpu_step(sys->cpu);
        uint64_t ppu_cycles = 3ul * cpu_cycles;
        uint64_t apu_cycles = cpu_cycles;

        for (uint64_t i = 0; i < ppu_cycles; ++i) {
            ppu_step(sys->ppu);
            mapper_step(sys->mapper, sys);
        }
    }

    del_sys(sys);
    return 0;
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
