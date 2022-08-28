#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void panic(char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    fprintf(stderr, "!Panic: ");
    vfprintf(stderr, format, argp);
    fprintf(stderr, "\n");
    exit(1);
}

void info(char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    fprintf(stderr, "!Info: ");
    vfprintf(stderr, format, argp);
    fprintf(stderr, "\n");
}
