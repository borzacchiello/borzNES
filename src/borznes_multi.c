#include "game_window.h"
#include "window.h"
#include "system.h"
#include "6502_cpu.h"
#include "memory.h"
#include "logging.h"
#include "ppu.h"
#include "apu.h"
#include "async.h"

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

#define BORZNES_DEFAULT_PORT 54000

typedef enum EmuState { DRAW_FRAME, WAIT_FOR_KEY, WAIT_UNTIL_READY } EmuState;

static int sync_frame_count = 2;

static void usage(const char* prog)
{
    fprintf(stderr,
            "USAGE: %s <game.rom> [ <peer_ip> ]\n"
            "   if <peer_ip> is not specified, listen on %d\n",
            prog, BORZNES_DEFAULT_PORT);
    exit(1);
}

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

static int estimate_latency(int fd)
{
    const int N     = 50;
    long      start = get_timestamp_microseconds();
    for (int i = 0; i < N; ++i) {
        char a;
        if (send(fd, "a", 1, 0) != 1)
            panic("estimate_latency(): unable to write [i= %d]", i);
        if (recv(fd, &a, 1, 0) != 1)
            panic("estimate_latency(): unable to read [i=%d]", i);
    }
    return (get_timestamp_microseconds() - start) / N / 1000l;
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
        usage(argv[0]);

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
        if (sync_recv(fd1, &magic, 7) != 7 ||
            memcmp(magic, "borzNES", 7) != 0) {
            panic("handshake failed (msg=\"%s\")", magic);
        }

        latency = estimate_latency(fd1);
        printf("estimated latency: %ld ms\n", latency);

        sync_frame_count = latency / (1000l / 60l) + 1;
        if (sync_frame_count > 6) {
            warning("the latency is too high, expect the emulator to be slow");
            sync_frame_count = 6;
        }

        int be_sync_frame_count = htonl(sync_frame_count);
        if (sync_send(fd1, &be_sync_frame_count, sizeof(be_sync_frame_count)) !=
            sizeof(be_sync_frame_count)) {
            panic("unable to send sync_frame_count");
        }
        printf("input sync every %u frame\n", sync_frame_count);
    } else {
        // player 2
        printf("Hello player 2! Trying to connect to %s\n", argv[2]);

        is_p1 = 0;
        fd1   = open_p2_socket(argv[2], BORZNES_DEFAULT_PORT);
        if (sync_send(fd1, "borzNES", 7) != 7)
            panic("write failed");
        printf("connected!\n");

        latency = estimate_latency(fd1);
        printf("estimated latency: %ld ms\n", latency);

        int be_sync_frame_count;
        if (sync_recv(fd1, &be_sync_frame_count, sizeof(be_sync_frame_count)) !=
            sizeof(be_sync_frame_count)) {
            panic("unable to recv sync_frame_count");
        }
        sync_frame_count = ntohl(be_sync_frame_count);
        printf("input sync every %u frame\n", sync_frame_count);
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    AsyncContext* ac    = async_init();
    System*       sys   = system_build(argv[1]);
    GameWindow*   gw    = simple_gw_build(sys);
    EmuState      state = DRAW_FRAME;

    gamewindow_draw(gw);

    long            start, end, microseconds_to_wait = 0;
    long            start_latency_calc;
    int             should_quit = 0, sync_count = 0, audio_on = 1;
    ControllerState p1, p2, freezed_p1;
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
                    // UTILS
                    case SDLK_q:
                        should_quit = 1;
                        break;
                    case SDLK_m:
                        audio_on = !audio_on;
                        if (audio_on)
                            apu_unpause(sys->apu);
                        else
                            apu_pause(sys->apu);
                        break;

                    // KEYMAPPINGS
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
            if (sync_count == 0) {
                freezed_p1 = p1;
                async_send(ac, fd1, &freezed_p1.state,
                           sizeof(freezed_p1.state));
                start_latency_calc = get_timestamp_microseconds();
            }

            if (++sync_count == sync_frame_count) {
                sync_count = 0;
                state      = WAIT_FOR_KEY;
            } else {
                state = WAIT_UNTIL_READY;
            }

            uint64_t cycles    = 0;
            uint32_t old_frame = sys->ppu->frame;
            while (sys->ppu->frame == old_frame)
                cycles += system_step(sys);

            microseconds_to_wait = 1000000ll * cycles / sys->cpu_freq;
        } else if (state == WAIT_FOR_KEY) {
            if (sync_recv(fd1, &p2.state, sizeof(p2.state)) != sizeof(p2.state))
                panic("unable to read keys");
            latency =
                (get_timestamp_microseconds() - start_latency_calc) / 1000l;

            system_update_controller(sys, is_p1 ? P1 : P2, freezed_p1);
            system_update_controller(sys, is_p1 ? P2 : P1, p2);
            state = WAIT_UNTIL_READY;
        } else if (state == WAIT_UNTIL_READY) {
            end = get_timestamp_microseconds();
            if (end - start >= microseconds_to_wait &&
                apu_get_queued(sys->apu) < sys->apu->spec.freq)
                state = DRAW_FRAME;
        }
    }

    gamewindow_destroy(gw);
    system_destroy(sys);
    async_destroy(ac);

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
