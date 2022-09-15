#include "game_window.h"
#include "window.h"
#include "system.h"
#include "6502_cpu.h"
#include "memory.h"
#include "ppu.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char const* argv[])
{
    if (argc < 2)
        return 1;

    System*     sys = system_build(argv[1]);
    GameWindow* gw  = gamewindow_build(sys);

    gamewindow_draw(gw);

    long            start;
    int             should_quit = 0;
    int             fast_freq = 0, slow_freq = 0;
    ControllerState p1, p2;
    p1.state = 0;
    p2.state = 0;

    SDL_Event e;
    while (!should_quit) {
        start = get_timestamp_milliseconds();
        if (window_poll_event(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
#ifdef DEBUG_MODE
                    case SDLK_i: {
                        system_step(sys);
                        break;
                    }
                    case SDLK_o: {
                        uint32_t old_frame = sys->ppu->frame;
                        while (sys->ppu->frame == old_frame)
                            system_step(sys);
                        break;
                    }
#endif
                    case SDLK_q:
                        should_quit = 1;
                        break;
                    case SDLK_p:
                        gw->patterntab_palette_idx =
                            (gw->patterntab_palette_idx + 1) % 8;
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
                        system_save_state(sys);
                        break;
                    case SDLK_F2:
                        system_load_state(sys);
                        break;
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
            system_update_controller(sys, P1, p1);
            system_update_controller(sys, P2, p2);
        }

#ifndef DEBUG_MODE
        system_step_ms(sys, get_timestamp_milliseconds() - start);
#endif
    }

    gamewindow_destroy(gw);
    system_destroy(sys);
    return 0;
}
