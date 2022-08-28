#include "system.h"
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
        return 2;

    uint8_t opcodes[] = {0xa2, 0x08, 0xca, 0x8e, 0x00, 0x02, 0xe0,
                         0x03, 0xd0, 0xf8, 0x8e, 0x01, 0x02, 0x00};

    System* sys = system_build(argv[1]);

    sys->cpu->PC = 0x2000;
    for (uint32_t addr = 0x2000; addr < 0x2000 + sizeof(opcodes); ++addr)
        memory_write(sys->cpu->mem, addr, opcodes[addr - 0x2000]);

    memory_write(sys->cpu->mem, 0xFFFE, 0xDE);
    memory_write(sys->cpu->mem, 0xFFFF, 0xAD);

    while (sys->cpu->PC != 0xdead) {
        printf("%s\n", cpu_state(sys->cpu));
        printf("%s\n", cpu_disassemble(sys->cpu, sys->cpu->PC));
        cpu_step(sys->cpu);
    }

    printf("cycles: %lu\n", sys->cpu->cycles);

    system_destroy(sys);
    return 0;
}
