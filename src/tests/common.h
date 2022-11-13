#ifndef FCOMMON_H
#define FCOMMON_H

#include "../alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>

static jmp_buf env;

void panic(char *format, ...)
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

#endif
