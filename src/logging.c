#include "logging.h"
#include "alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static void common_print(const char* msg, char* format, va_list argp)
{
    fprintf(stderr, "!%s: ", msg);
    vfprintf(stderr, format, argp);
    fprintf(stderr, "\n");
}

void panic(char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    common_print("Panic", format, argp);
    exit(1);
}

void info(char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    common_print("Info", format, argp);
}

void warning(char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    common_print("Warning", format, argp);
}
