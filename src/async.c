#include "async.h"
#include "alloc.h"
#include "logging.h"

#include <string.h>
#include <errno.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

static void* thread_fun(void* _arg)
{
    AsyncContext* arg = (AsyncContext*)_arg;
    pthread_mutex_lock(&arg->mutex_1);

    while (arg->should_run) {
        sync_send(arg->fd, &arg->buf, arg->buf_size);
        pthread_mutex_unlock(&arg->mutex_2);
        pthread_mutex_lock(&arg->mutex_1);
    }
    return NULL;
}

AsyncContext* async_init()
{
    AsyncContext* ac = (AsyncContext*)malloc_or_fail(sizeof(AsyncContext));

    pthread_mutex_init(&ac->mutex_1, NULL);
    pthread_mutex_init(&ac->mutex_2, NULL);

    pthread_mutex_lock(&ac->mutex_1);

    if (pthread_create(&ac->thread, NULL, &thread_fun, ac) != 0)
        panic("pthread_create failed");

    return ac;
}

void async_destroy(AsyncContext* ac)
{
    ac->should_run = 0;
    pthread_mutex_unlock(&ac->mutex_1);
    pthread_join(ac->thread, NULL);

    pthread_mutex_destroy(&ac->mutex_1);
    pthread_mutex_destroy(&ac->mutex_2);
    free(ac);
}

void async_send(AsyncContext* ac, int fd, const void* i_buf, int64_t size)
{
    if (size > sizeof(ac->buf))
        panic("Unable to send more than %u bytes", sizeof(ac->buf));

    pthread_mutex_lock(&ac->mutex_2);
    ac->fd       = fd;
    ac->buf_size = size;
    memcpy(ac->buf, i_buf, size);
    pthread_mutex_unlock(&ac->mutex_1);
}

int64_t sync_send(int fd, const void* i_buf, int64_t len)
{
    if (len <= 0)
        panic("sync_send(): invalid len");

    const void* pt      = i_buf;
    int64_t     n_wrote = 0;
    while (n_wrote < len) {
        int64_t r = send(fd, pt, len - n_wrote, 0);
        if (r < 0)
            panic("sync_send(): write failed [%s]", strerror(errno));
        if (r == 0)
            panic("sync_send(): peer disconnected");
        pt += r;
        n_wrote += r;
    }
    return n_wrote;
}

int64_t sync_recv(int fd, void* o_buf, int64_t len)
{
    if (len <= 0)
        panic("sync_recv(): invalid len");

    void*   pt     = o_buf;
    int64_t n_read = 0;
    while (n_read < len) {
        int64_t r = recv(fd, pt, len - n_read, 0);
        if (r < 0)
            panic("sync_recv(): read failed [%s]", strerror(errno));
        if (r == 0)
            panic("sync_recv(): peer disconnected");
        pt += r;
        n_read += r;
    }
    return n_read;
}
