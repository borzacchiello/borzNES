#include "system.h"
#include "alloc.h"

System* system_build(const char* rom_path)
{
    System* sys = malloc_or_fail(sizeof(System));

    Cartridge* cart = cartridge_load(rom_path);
    Mapper*    map  = mapper_build(cart);
    Cpu*       cpu  = cpu_build(sys);

    sys->cart   = cart;
    sys->mapper = map;
    sys->cpu    = cpu;

    cpu_reset(cpu);
    return sys;
}

void system_destroy(System* sys)
{
    cartridge_unload(sys->cart);
    mapper_destroy(sys->mapper);
    cpu_destroy(sys->cpu);
    free(sys);
}
