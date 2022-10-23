#include "alloc.h"
#include "logging.h"

#include <string.h>

void* malloc_or_fail(size_t n)
{
    void* r = malloc(n);
    if (r == NULL)
        panic("malloc failed");

    return r;
}

void* calloc_or_fail(size_t n)
{
    void* r = malloc_or_fail(n);
    memset(r, 0, n);
    return r;
}

void* realloc_or_fail(void* b, size_t size)
{
    // check for weird usage of the API...
    if (b == NULL)
        panic("realloc buffer is NULL");
    if (size == 0)
        panic("realloc size is zero");

    void* r = realloc(b, size);
    if (r == NULL)
        panic("realloc failed");

    return r;
}

Buffer buf_malloc(size_t n)
{
    Buffer res = {.buffer = malloc_or_fail(n), .size = n};
    return res;
}

Buffer buf_calloc(size_t n)
{
    Buffer res = {.buffer = calloc_or_fail(n), .size = n};
    return res;
}

void dump_buffer(const Buffer* buf, FILE* ofile)
{
    if (fwrite(&buf->size, 1, sizeof(buf->size), ofile) != sizeof(buf->size))
        panic("unable to dump the buffer (size)");

    if (fwrite(buf->buffer, 1, buf->size, ofile) != buf->size)
        panic("unable to dump the buffer");
}

Buffer read_buffer(FILE* ifile)
{
    uint64_t size;
    if (fread(&size, 1, sizeof(size), ifile) != sizeof(size))
        panic("unable to read buffer (size)");

    if (size > 10000000u)
        panic("trying to read a buffer > 10 MB");

    Buffer res = buf_malloc(size);
    if (fread(res.buffer, 1, res.size, ifile) != res.size)
        panic("unable to read buffer");

    return res;
}
