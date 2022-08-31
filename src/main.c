#include "system.h"
#include "6502_cpu.h"
#include "ppu.h"
#include "memory.h"
#include "ui.h"

int main(int argc, char const* argv[])
{
    /*
        Window* win = window_build_for_text(64, 64);

        for (uint32_t row = 0; row < win->text_rows; ++row)
            for (uint32_t col = 0; col < win->text_cols; ++col)
                window_draw_text(win, row, col, color_white, "A");
        window_present(win);

        SDL_Event e;
        while (1) {
            if (window_poll_event(&e) && e.type == SDL_QUIT)
                break;
        }

        window_destroy(win);
        return 0;
    */

    /*
     if (argc < 2)
         return 2;

     const char* path = argv[1];
     Cartridge*  cart = cartridge_load(path);
     cartridge_print(cart);

     cartridge_unload(cart);
     */

    if (argc < 2)
        return 1;

    System* sys = system_build(argv[1]);
    printf("%s\n", system_tostring(sys));

    printf("\n\n%s\n%s\n", cpu_tostring(sys->cpu), ppu_tostring(sys->ppu));
    while (1) {
        printf("-> %s\n", cpu_disassemble(sys->cpu, sys->cpu->PC));
        // getchar();

        system_step(sys);
        printf("%s\n", cpu_tostring(sys->cpu));
        printf("%s\n", ppu_tostring(sys->ppu));
    }

    system_destroy(sys);
    return 0;
}
