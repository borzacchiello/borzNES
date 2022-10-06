#include "../window.h"
#include "../alloc.h"
#include "../logging.h"
#include "../input_handler.h"
#include "../config.h"

#include <SDL2/SDL.h>

int main(int argc, char const* argv[])
{
    SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_VIDEO);
    SDL_JoystickEventState(SDL_ENABLE);

    config_load(DEFAULT_CFG_NAME);

    Window* win = window_build_for_text(4, 20);

    SDL_Joystick** joysticks      = NULL;
    int            joysticks_size = SDL_NumJoysticks();
    if (joysticks_size > 0) {
        joysticks = calloc_or_fail(sizeof(SDL_Joystick*) * joysticks_size);

        for (int i = 0; i < joysticks_size; ++i) {
            joysticks[i] = SDL_JoystickOpen(i);
            info("Opened joystick %s\n", SDL_JoystickName(joysticks[i]));
        }
    }

    char* keys[] = {
        CFG_P1_A,      NULL, CFG_P1_B,     NULL, CFG_P1_START,  NULL,
        CFG_P1_SELECT, NULL, CFG_P1_UP,    NULL, CFG_P1_DOWN,   NULL,
        CFG_P1_LEFT,   NULL, CFG_P1_RIGHT, NULL, CFG_P2_A,      NULL,
        CFG_P2_B,      NULL, CFG_P2_START, NULL, CFG_P2_SELECT, NULL,
        CFG_P2_UP,     NULL, CFG_P2_DOWN,  NULL, CFG_P2_LEFT,   NULL,
        CFG_P2_RIGHT,  NULL,
    };

    int i = 0;
    window_draw_text(win, 0, 0, color_white, "choose key for:");
    window_draw_text(win, 1, 0, color_white, keys[i]);
    window_present(win);

    SDL_Event e;
    while (1) {
        if (window_poll_event(&e)) {
            if (e.type == SDL_QUIT)
                break;
            uint64_t kval = input_handler_encode_button(e);
            if (kval == 0)
                continue;

            // info("Chosen %016x for %s", kval, keys[i]);
            config_set_value(keys[i], kval);

            keys[i + 1] = (char*)kval;
            i += 2;
            if (i >= sizeof(keys) / sizeof(char*))
                break;
            window_prepare_redraw(win);
            window_draw_text(win, 0, 0, color_white, "choose key for:");
            window_draw_text(win, 1, 0, color_white, keys[i]);
            window_present(win);
        }
    }

    config_save(DEFAULT_CFG_NAME);
    config_unload();

    free(joysticks);
    window_destroy(win);
    return 0;
}
