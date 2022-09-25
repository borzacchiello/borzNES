#include "game_window.h"
#include "window.h"
#include "system.h"
#include "6502_cpu.h"
#include "memory.h"
#include "logging.h"
#include "ppu.h"
#include "apu.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifdef __MINGW32__
#include <winsock2.h>
#else
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#define SYNC_FRAME_COUNT     2
#define BORZNES_DEFAULT_PORT 54000

typedef enum EmuState { DRAW_FRAME, WAIT_FOR_KEY, WAIT_UNTIL_READY } EmuState;

static int open_p1_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        panic("cannot create p1 socket");

    int flags = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&flags, sizeof(flags)) <
        0)
        panic("setsockopt failed");

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        panic("cannot bind p1 socket");

    if (listen(fd, 1) < 0)
        panic("cannot listen on p1 socket");

    return fd;
}

static int open_p2_socket(const char* ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        panic("cannot create p2 socket");

    int flags = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&flags, sizeof(flags)) <
        0)
        panic("setsockopt failed");

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port        = htons(port);
    uint32_t addr_len    = sizeof(addr);

    if (connect(fd, (struct sockaddr*)&addr, addr_len) < 0)
        panic("p1 server is not running");

    return fd;
}

static int64_t read_all(int fd, void* o_buf, int64_t len)
{
    if (len <= 0)
        panic("read_all(): invalid len");

    void*   pt     = o_buf;
    int64_t n_read = 0;
    while (n_read < len) {
        int64_t r = recv(fd, pt, len - n_read, 0);
        if (r < 0)
            panic("read_all(): read failed [%s]", strerror(errno));
        if (r == 0)
            panic("read_all(): peer disconnected");
        pt += r;
        n_read += r;
    }
    return n_read;
}

static int64_t write_all(int fd, void* i_buf, int64_t len)
{
    if (len <= 0)
        panic("write_all(): invalid len");

    void*   pt      = i_buf;
    int64_t n_wrote = 0;
    while (n_wrote < len) {
        int64_t r = send(fd, pt, len - n_wrote, 0);
        if (r < 0)
            panic("write_all(): write failed [%s]", strerror(errno));
        if (r == 0)
            panic("write_all(): peer disconnected");
        pt += r;
        n_wrote += r;
    }
    return n_wrote;
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
        return 1;

#ifdef __MINGW32__
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int is_p1;
    int fd1 = -1, fd2 = -1;
    if (argc == 2) {
        // player 1
        printf("Hello player 1! Waiting for player 2 to connect\n");

        is_p1 = 1;
        fd2   = open_p1_socket(BORZNES_DEFAULT_PORT);

        struct sockaddr_in client_addr;
        uint32_t           client_addr_len = sizeof(client_addr);
        if ((fd1 = accept(fd2, (struct sockaddr*)&client_addr,
                          &client_addr_len)) < 0)
            panic("accept error");

        printf("player 2 connected from %s\n", inet_ntoa(client_addr.sin_addr));

        static char magic[8];
        if (read_all(fd1, &magic, 7) != 7 || memcmp(magic, "borzNES", 7) != 0) {
            panic("handshake failed (msg=\"%s\")", magic);
        }
    } else {
        // player 2
        printf("Hello player 2! Trying to connect to %s\n", argv[2]);

        is_p1 = 0;
        fd1   = open_p2_socket(argv[2], BORZNES_DEFAULT_PORT);
        if (write_all(fd1, "borzNES", 7) != 7)
            panic("write failed");
        printf("connected!\n");
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    System*     sys   = system_build(argv[1]);
    GameWindow* gw    = gamewindow_build(sys);
    EmuState    state = DRAW_FRAME;

    gamewindow_draw(gw);

    long            start, end, microseconds_to_wait = 0;
    int             should_quit = 0, sync_count = 0;
    ControllerState p1, p2;
    p1.state = 0;
    p2.state = 0;

    SDL_Event e;
    while (!should_quit) {
        if (state == DRAW_FRAME)
            start = get_timestamp_microseconds();

        if (window_poll_event(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_z:
                        p1.A = 1;
                        break;
                    case SDLK_x:
                        p1.B = 1;
                        break;
                    case SDLK_RETURN:
                        p1.START = 1;
                        break;
                    case SDLK_RSHIFT:
                        p1.SELECT = 1;
                        break;
                    case SDLK_UP:
                        p1.UP = 1;
                        break;
                    case SDLK_DOWN:
                        p1.DOWN = 1;
                        break;
                    case SDLK_LEFT:
                        p1.LEFT = 1;
                        break;
                    case SDLK_RIGHT:
                        p1.RIGHT = 1;
                        break;
                }
            } else if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                    case SDLK_z:
                        p1.A = 0;
                        break;
                    case SDLK_x:
                        p1.B = 0;
                        break;
                    case SDLK_RETURN:
                        p1.START = 0;
                        break;
                    case SDLK_RSHIFT:
                        p1.SELECT = 0;
                        break;
                    case SDLK_UP:
                        p1.UP = 0;
                        break;
                    case SDLK_DOWN:
                        p1.DOWN = 0;
                        break;
                    case SDLK_LEFT:
                        p1.LEFT = 0;
                        break;
                    case SDLK_RIGHT:
                        p1.RIGHT = 0;
                        break;
                }
            }
        }

        if (state == DRAW_FRAME) {
            uint64_t cycles    = 0;
            uint32_t old_frame = sys->ppu->frame;
            while (sys->ppu->frame == old_frame)
                cycles += system_step(sys);

            if (++sync_count == SYNC_FRAME_COUNT) {
                sync_count           = 0;
                microseconds_to_wait = 1000000ll * cycles / sys->cpu_freq;
                state                = WAIT_FOR_KEY;
            } else {
                state = WAIT_UNTIL_READY;
            }
        } else if (state == WAIT_FOR_KEY) {
            if (write_all(fd1, &p1.state, sizeof(p1.state)) != sizeof(p1.state))
                panic("unable to send keys");

            if (read_all(fd1, &p2.state, sizeof(p2.state)) != sizeof(p2.state))
                panic("unable to read keys");

            system_update_controller(sys, is_p1 ? P1 : P2, p1);
            system_update_controller(sys, is_p1 ? P2 : P1, p2);
            state = WAIT_UNTIL_READY;
        } else if (state == WAIT_UNTIL_READY) {
            end = get_timestamp_microseconds();
            if (end - start >= microseconds_to_wait)
                state = DRAW_FRAME;
        }
    }

    gamewindow_destroy(gw);
    system_destroy(sys);

    SDL_Quit();

#ifdef __MINGW32__
    closesocket(fd1);
    if (fd2 > 0) {
        closesocket(fd2);
    }

    WSACleanup();
#else
    close(fd1);
    if (fd2 > 0)
        close(fd2);
#endif
    return 0;
}
