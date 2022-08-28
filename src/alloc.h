#ifndef ALLOC_H
#define ALLOC_H

#include <stdlib.h>

void* malloc_or_fail(size_t n);
void* calloc_or_fail(size_t n);

#endif
