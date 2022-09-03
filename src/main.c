#include "game_window.h"
#include "window.h"
#include "system.h"
#include "6502_cpu.h"
#include "memory.h"
#include "ppu.h"

#include <stdio.h>
#include <unistd.h>

static Window* mk_game_window()
{
    int gameview_width  = 256;
    int gameview_heigth = 240;

    Window* win = window_build(gameview_width * 3 +
                                   gameview_width * 2 /* some room for text */,
                               gameview_heigth * 3);
    return win;
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
        return 1;

    System*     sys = system_build(argv[1]);
    GameWindow* gw  = gamewindow_build(sys);

    gamewindow_draw(gw);

    int       old_frame = 0;
    SDL_Event e;
    while (1) {
        if (window_poll_event(&e)) {
            if (e.type == SDL_QUIT) {
                break;
            } else if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_SPACE) {
                    system_step(sys);
                } else if (e.key.keysym.sym == SDLK_s) {
                    int sl = sys->ppu->scanline;
                    while (sys->ppu->scanline == sl)
                        system_step(sys);
                } else if (e.key.keysym.sym == SDLK_f) {
                    int f = sys->ppu->frame;
                    while (sys->ppu->frame == f)
                        system_step(sys);
                } else if (e.key.keysym.sym == SDLK_q) {
                    break;
                }
            }
        }

        system_step(sys);

        if (old_frame != gw->sys->ppu->frame) {
            old_frame = gw->sys->ppu->frame;
            /*
            for (int x = 0; x < 256; ++x) {
                for (int y = 0; y < 240; ++y) {
                    uint8_t r = (x + y + gw->sys->ppu->frame) % 256;
                    uint8_t g = (x * y + gw->sys->ppu->frame) % 256;
                    uint8_t b = 256 - (x + y + gw->sys->ppu->frame) % 256;

                    gamewindow_set_pixel(gw, x, y, PACK_RGB(r, g, b));
                }
            }
            */
            gamewindow_draw(gw);
        }
    }

    gamewindow_destroy(gw);
    system_destroy(sys);
    return 0;
}
