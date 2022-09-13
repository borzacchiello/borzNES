#ifndef ALLOC_H
#define ALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct Buffer {
    uint8_t* buffer;
    uint64_t size;
} Buffer;

void* malloc_or_fail(size_t n);
void* calloc_or_fail(size_t n);

Buffer buf_malloc(size_t n);
Buffer buf_calloc(size_t n);

void   dump_buffer(const Buffer* buf, FILE* ofile);
Buffer read_buffer(FILE* ifile);

#endif
