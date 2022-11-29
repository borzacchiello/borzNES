#ifndef FCOMMON_H
#define FCOMMON_H

#include "../system.h"
#include "../6502_cpu.h"
#include "../memory.h"
#include "../ppu.h"
#include "../apu.h"
#include "../cartridge.h"
#include "../mapper.h"
#include "../alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>

static jmp_buf env;

void panic(char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    vfprintf(stderr, format, argp);
    fprintf(stderr, "\n");

    free_all();
    longjmp(env, 1);
}

void info(char* format, ...)
{
    va_list argp;
    va_start(argp, format);
}

void warning(char* format, ...)
{
    va_list argp;
    va_start(argp, format);
}

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
    free_or_fail(sys->apu);
    free_or_fail(sys);
}

#endif
