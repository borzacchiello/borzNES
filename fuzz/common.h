#ifndef FCOMMON_H
#define FCOMMON_H

#include "../src/alloc.h"

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

static Buffer read_file_raw(const char* path)
{
    FILE* fileptr = fopen(path, "rb");
    if (fileptr == NULL)
        panic("unable to open the file %s", path);

    if (fseek(fileptr, 0, SEEK_END) < 0)
        panic("fseek failed");

    long filelen = ftell(fileptr);
    if (filelen < 0)
        panic("ftell failed");

    rewind(fileptr);

    uint8_t* buffer = malloc_or_fail(filelen * sizeof(char));
    if (fread(buffer, 1, filelen, fileptr) != filelen)
        panic("fread failed");

    fclose(fileptr);

    Buffer res = {.buffer = buffer, .size = filelen};
    return res;
}

#endif
