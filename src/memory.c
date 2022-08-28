#include "memory.h"
#include "alloc.h"
#include "system.h"

Memory* memory_build(System* sys)
{
    Memory* res = malloc_or_fail(sizeof(Memory));
    res->sys    = sys;
    res->buf    = malloc_or_fail(0x10000u);
    return res;
}

void memory_destroy(Memory* mem)
{
    free(mem->buf);
    free(mem);
}

uint8_t memory_read(Memory* mem, uint16_t addr) { return mem->buf[addr]; }

void memory_write(Memory* mem, uint16_t addr, uint8_t value)
{
    mem->buf[addr] = value;
}
