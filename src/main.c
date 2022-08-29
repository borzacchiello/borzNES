#include "6502_cpu.h"
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

    uint8_t opcodes[] = {0xa2, 0x08, 0xca, 0x8e, 0x00, 0x02, 0xe0,
                         0x03, 0xd0, 0xf8, 0x8e, 0x01, 0x02, 0x00};

    Cpu* cpu = cpu_standalone_build();

    cpu->PC = 0x2000;
    for (uint32_t addr = 0x2000; addr < 0x2000 + sizeof(opcodes); ++addr)
        memory_write(cpu->mem, addr, opcodes[addr - 0x2000]);

    while (cpu->PC != 0x200d) {
        printf("%s\n", cpu_state(cpu));
        printf("%s\n", cpu_disassemble(cpu, cpu->PC));
        cpu_step(cpu);
    }
    printf("cycles: %lu\n", cpu->cycles);

    cpu_destroy(cpu);
    return 0;
}
