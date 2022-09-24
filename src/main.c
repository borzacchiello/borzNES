#include "game_window.h"
#include "window.h"
#include "system.h"
#include "6502_cpu.h"
#include "memory.h"
#include "ppu.h"
#include "apu.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define ENABLE_REWIND   1
#define REWIND_BUF_SIZE 100
#define REWIND_DIR      "/tmp/borznes"

typedef enum { NORMAL_MODE, DEBUG_MODE, REWIND_MODE } EmulationMode;

static int  rewind_num_states = 0;
static int  rewind_id         = 0;
static char rewind_tmp_path[128];

void init_rewind()
{
    struct stat st = {0};
    if (stat(REWIND_DIR, &st) == -1) {
        mkdir(REWIND_DIR, 0700);
    }
}

void save_rewind_state(System* sys)
{
    memset(rewind_tmp_path, 0, sizeof(rewind_tmp_path));

    sprintf(rewind_tmp_path, REWIND_DIR "/%03u.sav", rewind_id);
    system_save_state(sys, rewind_tmp_path);

    rewind_id = (rewind_id + 1) % REWIND_BUF_SIZE;
    if (rewind_num_states < REWIND_BUF_SIZE)
        rewind_num_states++;
}

int load_rewind_state(System* sys)
{
    if (rewind_num_states == 0)
        return 0;

    memset(rewind_tmp_path, 0, sizeof(rewind_tmp_path));

    if (--rewind_id < 0)
        rewind_id = REWIND_BUF_SIZE - 1;
    rewind_num_states--;

    sprintf(rewind_tmp_path, REWIND_DIR "/%03u.sav", rewind_id);
    system_load_state(sys, rewind_tmp_path);

    return 1;
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
        return 1;

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    System*     sys = system_build(argv[1]);
    GameWindow* gw  = gamewindow_build(sys);

    init_rewind();
    gamewindow_draw(gw);

    EmulationMode   mode = NORMAL_MODE;
    long            start, end, last_rewind_timestamp = 0;
    int             should_quit = 0, fast_freq = 0, slow_freq = 0, audio_on = 1;
    ControllerState p1, p2;
    p1.state = 0;
    p2.state = 0;

    end = get_timestamp_milliseconds();

    SDL_Event e;
    while (!should_quit) {
        start = end;
        if (window_poll_event(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    // DEBUG MODE KEYS
                    case SDLK_i: {
                        // step one CPU instruction
                        if (mode == DEBUG_MODE) {
                            system_step(sys);
                            gamewindow_draw(gw);
                        }
                        break;
                    }
                    case SDLK_o: {
                        // step one PPU frame
                        if (mode == DEBUG_MODE) {
                            uint32_t old_frame = sys->ppu->frame;
                            while (sys->ppu->frame == old_frame)
                                system_step(sys);
                            gamewindow_draw(gw);
                        }
                        break;
                    }
                    case SDLK_d: {
                        // toggle debug mode
                        if (mode == NORMAL_MODE)
                            mode = DEBUG_MODE;
                        else if (mode == DEBUG_MODE)
                            mode = NORMAL_MODE;
                        break;
                    }
                    // UTILS
                    case SDLK_q:
                        should_quit = 1;
                        break;
                    case SDLK_p:
                        audio_on = !audio_on;
                        if (audio_on)
                            apu_unpause(sys->apu);
                        else
                            apu_pause(sys->apu);
                        break;
                    case SDLK_f:
                        slow_freq     = 0;
                        fast_freq     = !fast_freq;
                        sys->cpu_freq = fast_freq ? CPU_2X_FREQ : CPU_1X_FREQ;
                        break;
                    case SDLK_g:
                        fast_freq     = 0;
                        slow_freq     = !slow_freq;
                        sys->cpu_freq = slow_freq ? CPU_0_5X_FREQ : CPU_1X_FREQ;
                        break;
                    case SDLK_F1:
                        system_save_state(sys, sys->state_save_path);
                        break;
                    case SDLK_F2:
                        system_load_state(sys, sys->state_save_path);
                        break;
                        // REWIND
#if ENABLE_REWIND
                    case SDLK_r:
                        mode = REWIND_MODE;
                        break;
#endif
                    // GAME KEYBINDINGS
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
                    // REWIND
                    case SDLK_r:
                        mode = NORMAL_MODE;
                        break;
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
            system_update_controller(sys, P1, p1);
            system_update_controller(sys, P2, p2);
        }

        end = get_timestamp_milliseconds();
        if (mode == NORMAL_MODE) {
            system_step_ms(sys, end - start);
#if ENABLE_REWIND
            if (end - last_rewind_timestamp > 500) {
                last_rewind_timestamp = end;
                save_rewind_state(sys);
            }
#endif
        }
#if ENABLE_REWIND
        if (mode == REWIND_MODE) {
            if (end - last_rewind_timestamp > 500) {
                last_rewind_timestamp = end;
                if (load_rewind_state(sys)) {
                    int old_frame = sys->ppu->frame;
                    while (sys->ppu->frame != old_frame + 2)
                        system_step(sys);
                }
            }
        }
#endif
    }

    gamewindow_destroy(gw);
    system_destroy(sys);

    SDL_Quit();
    return 0;
}
