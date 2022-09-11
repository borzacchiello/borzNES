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

    int             should_quit = 0;
    int             old_frame   = 0;
    ControllerState p1, p2;
    p1.state = 0;
    p2.state = 0;
    SDL_Event e;
    while (!should_quit) {
        if (sys->ppu->frame % 15 == 0 && window_poll_event(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_q:
                        should_quit = 1;
                        break;
                    case SDLK_z:
                        p1.A = 1;
                        break;
                    case SDLK_x:
                        p1.B = 1;
                        break;
                    case SDLK_RSHIFT:
                        p1.START = 1;
                        break;
                    case SDLK_RETURN:
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
                    case SDLK_RSHIFT:
                        p1.START = 0;
                        break;
                    case SDLK_RETURN:
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

        system_step(sys);
        if (old_frame != gw->sys->ppu->frame) {
            old_frame = gw->sys->ppu->frame;
            gamewindow_draw(gw);
        }
    }

    gamewindow_destroy(gw);
    system_destroy(sys);
    return 0;
}
