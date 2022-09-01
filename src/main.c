#include "system.h"
#include "6502_cpu.h"
#include "ppu.h"
#include "memory.h"
#include "ui.h"

#include <unistd.h>

static Window* mk_game_window()
{
    int gameview_width  = 256;
    int gameview_heigth = 240;

    Window* win = window_build(gameview_width * 2 + gameview_width +
                                   gameview_width / 2 /* some room for text */,
                               gameview_heigth * 2);
    return win;
}

int main(int argc, char const* argv[])
{
    if (argc < 2)
        return 1;

    System* sys = system_build(argv[1]);
    printf("%s\n", system_tostring(sys));

    SDL_Event e;
    Window*   win = mk_game_window();
    window_present(win);

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

        window_prepare_redraw(win);

        int col_off = 256 * 2 / win->text_char_width + 1;

        window_draw_text(win, 0, col_off, color_white,
                         cpu_disassemble(sys->cpu, sys->cpu->PC));
        window_draw_text(win, 3, col_off, color_white, cpu_tostring(sys->cpu));
        window_draw_text(win, 10, col_off, color_white, ppu_tostring(sys->ppu));

        window_present(win);
    }

    window_destroy(win);
    system_destroy(sys);
    return 0;
}
