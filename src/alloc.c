#include "alloc.h"
#include "logging.h"

#include <string.h>

#ifdef NOLEAK
static void** allocated;
static size_t allocated_size;
static int    allocated_id;

static void init_allocated()
{
    allocated_id   = 0;
    allocated_size = 100;
    allocated      = malloc(sizeof(void*) * allocated_size);
    if (!allocated)
        panic("init_allocated(): unable to allocate memory");
}

static void add_to_allocated(void* a)
{
    if (allocated == NULL)
        init_allocated();

    if (allocated_id == allocated_size) {
        allocated_size = allocated_size * 4 / 3;
        allocated      = realloc(allocated, sizeof(void*) * allocated_size);
        if (!allocated)
            panic("add_to_allocated(): unable to allocate memory");
    }
    allocated[allocated_id++] = a;
}

static void remove_from_allocated(void* a)
{
    int i;
    for (i = 0; i < allocated_id; ++i)
        if (a == allocated[i])
            break;

    if (i == allocated_id)
        panic("remote_from_allocated(): unable to find buffer");
    if (allocated_id > 0)
        allocated[i] = allocated[allocated_id - 1];
    allocated_id--;
}
#endif

void free_all()
{
#ifdef NOLEAK
    for (int i = 0; i < allocated_id; ++i)
        free(allocated[i]);

    free(allocated);
    allocated_id = 0;
    allocated    = NULL;
#endif
}

void* malloc_or_fail(size_t n)
{
    void* r = malloc(n);
    if (r == NULL)
        panic("malloc failed");

#ifdef NOLEAK
    add_to_allocated(r);
#endif
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

#ifdef NOLEAK
    add_to_allocated(r);
#endif
    return r;
}

void free_or_fail(void* b)
{
    free(b);
#ifdef NOLEAK
    if (b)
        remove_from_allocated(b);
#endif
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
