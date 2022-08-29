#include "ppu.h"
#include "system.h"
#include "memory.h"
#include "alloc.h"

Ppu* ppu_build(System* sys)
{
    Ppu* ppu = malloc_or_fail(sizeof(Ppu));
    ppu->mem = ppu_memory_build(sys);

    return ppu;
}

void ppu_destroy(Ppu* ppu)
{
    memory_destroy(ppu->mem);
    free(ppu);
}

void ppu_reset(Ppu* ppu)
{
    ppu->cycle    = 340;
    ppu->scanline = 240;
    ppu->frame    = 0;
}
