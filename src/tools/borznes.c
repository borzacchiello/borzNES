#include "../game_window.h"
#include "../window.h"
#include "../system.h"
#include "../6502_cpu.h"
#include "../memory.h"
#include "../ppu.h"
#include "../apu.h"
#include "../config.h"
#include "../input_handler.h"
#include "../async.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define DELTA_MS_TO_WAIT     5000
#define SLEEP_BETWEEN_FRAMES 0
#define REWIND_BUF_SIZE      100

#ifdef __MINGW32__
#define REWIND_DIR "borznes_rewind_states"
#else
#define REWIND_DIR "/tmp/borznes_rewind_states"
#endif

typedef enum { NORMAL_MODE, DEBUG_MODE, REWIND_MODE } EmulationMode;

static int  rewind_num_states = 0;
static int  rewind_id         = 0;
static char rewind_tmp_path[128];

static void usage(const char* prog)
{
    fprintf(stderr, "USAGE: %s <game.rom>\n", prog);
    exit(1);
}

void init_rewind()
{
    struct stat st = {0};
    if (stat(REWIND_DIR, &st) == -1) {
#ifdef __MINGW32__
        mkdir(REWIND_DIR);
#else
        mkdir(REWIND_DIR, 0700);
#endif
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
        usage(argv[0]);

    config_load(DEFAULT_CFG_NAME);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

    System* sys = system_build(argv[1]);

#ifdef ENABLE_DEBUG_GW
    GameWindow* gw = rich_gw_build(sys);
#else
    GameWindow* gw = simple_gw_build(sys);
#endif

    init_rewind();
    gamewindow_draw(gw);

    InputHandler* ih = input_handler_build();

    EmulationMode mode = NORMAL_MODE;
    long          start, end, last_rewind_timestamp = 0;
    int           should_quit = 0, fast_freq = 0, slow_freq = 0, audio_on = 1,
        should_draw            = 1;
    uint64_t        ms_to_wait = 0;
    ControllerState p1, p2;
    MiscKeys        mk = {0};
    p1.state           = 0;
    p2.state           = 0;

    SDL_Event e;
    while (!should_quit) {
        if (window_poll_event(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            }

            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q) {
                should_quit = 1;
                continue;
            }

#ifdef ENABLE_DEBUG_GW
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_i) {
                    if (mode == DEBUG_MODE) {
                        system_step(sys);
                        gamewindow_draw(gw);
                    }
                } else if (e.key.keysym.sym == SDLK_o) {
                    if (mode == DEBUG_MODE) {
                        uint32_t old_frame = sys->ppu->frame;
                        while (sys->ppu->frame == old_frame)
                            system_step(sys);
                        gamewindow_draw(gw);
                    }
                } else if (e.key.keysym.sym == SDLK_d) {
                    if (mode == NORMAL_MODE)
                        mode = DEBUG_MODE;
                    else if (mode == DEBUG_MODE)
                        mode = NORMAL_MODE;
                }
            }
#endif

            input_handler_get_input(ih, e, &p1, &p2, &mk);
            system_update_controller(sys, P1, p1);
            system_update_controller(sys, P2, p2);

            if (mk.mute) {
                mk.mute  = 0;
                audio_on = !audio_on;
                if (audio_on) {
                    apu_unpause(sys->apu);
                    gamewindow_show_popup(gw, "audio on");
                } else {
                    apu_pause(sys->apu);
                    gamewindow_show_popup(gw, "audio off");
                }
            }
            if (mk.fast_mode) {
                mk.fast_mode = 0;
                slow_freq    = 0;
                fast_freq    = !fast_freq;
                if (fast_freq) {
                    sys->cpu_freq = CPU_2X_FREQ;
                    gamewindow_show_popup(gw, "fast mode");
                } else {
                    sys->cpu_freq = CPU_1X_FREQ;
                    gamewindow_show_popup(gw, "normal mode");
                }
            }
            if (mk.slow_mode) {
                mk.slow_mode = 0;
                fast_freq    = 0;
                slow_freq    = !slow_freq;
                if (slow_freq) {
                    sys->cpu_freq = CPU_0_5X_FREQ;
                    gamewindow_show_popup(gw, "slow mode");
                } else {
                    sys->cpu_freq = CPU_1X_FREQ;
                    gamewindow_show_popup(gw, "normal mode");
                }
            }
            if (mk.save_state) {
                mk.save_state = 0;
                system_save_state(sys, sys->state_save_path);
                gamewindow_show_popup(gw, "state saved");
            }
            if (mk.load_state) {
                mk.load_state = 0;
                system_load_state(sys, sys->state_save_path);
                gamewindow_show_popup(gw, "state loaded");
            }
            if (mode != REWIND_MODE && mk.rewind) {
                mode = REWIND_MODE;
            } else if (!mk.rewind && mode == REWIND_MODE) {
                mode = NORMAL_MODE;
            }
        }

        if (mode == NORMAL_MODE) {
            if (should_draw) {
                uint64_t cycles    = 0;
                uint32_t old_frame = sys->ppu->frame;
                while (sys->ppu->frame == old_frame)
                    cycles += system_step(sys);

                should_draw = 0;
                ms_to_wait  = 1000000ll * cycles / sys->cpu_freq;
                if (ms_to_wait > DELTA_MS_TO_WAIT)
                    ms_to_wait -= DELTA_MS_TO_WAIT;
                else
                    ms_to_wait = 0;
            } else {
                end = get_timestamp_microseconds();
                if (end - start > ms_to_wait &&
                    apu_get_queued(sys->apu) < sys->apu->spec.freq) {
                    start       = end;
                    should_draw = 1;
                } else if (ms_to_wait > end - start &&
                           ms_to_wait - (end - start) > 5000)
                    if (SLEEP_BETWEEN_FRAMES)
                        msleep(ms_to_wait / 1000 - 5);
            }

            end = get_timestamp_microseconds();
            if (end - last_rewind_timestamp > 500000) {
                last_rewind_timestamp = end;
                save_rewind_state(sys);
            }
        }

        if (mode == REWIND_MODE) {
            end = get_timestamp_microseconds();
            if (end - last_rewind_timestamp > 500000) {
                last_rewind_timestamp = end;
                if (load_rewind_state(sys)) {
                    int old_frame = sys->ppu->frame;
                    while (sys->ppu->frame != old_frame + 2)
                        system_step(sys);
                }
            }
        }
    }

    gamewindow_destroy(gw);
    system_destroy(sys);
    input_handler_destroy(ih);
    config_unload();

    SDL_Quit();
    return 0;
}
