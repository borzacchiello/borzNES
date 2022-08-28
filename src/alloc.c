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
