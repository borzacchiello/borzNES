#ifndef ASYNC_H
#define ASYNC_H

#include <stdint.h>
#include <pthread.h>

typedef struct AsyncContext {
    int             should_run;
    int             fd;
    char            buf[8];
    int64_t         buf_size;
    pthread_t       thread;
    pthread_mutex_t mutex_1, mutex_2;
} AsyncContext;

AsyncContext* async_init();
void          async_destroy(AsyncContext* ac);

void async_send(AsyncContext* ac, int fd, const void* i_buf, int64_t size);

int64_t sync_send(int fd, const void* i_buf, int64_t size);
int64_t sync_recv(int fd, void* o_buf, int64_t size);
void    msleep(uint32_t msec);

#endif
