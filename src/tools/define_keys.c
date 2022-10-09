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

    Window* win = window_build_for_text(5, 32);

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
        CFG_P1_A,     "P1 A",        CFG_P1_B,      "P1 B",      CFG_P1_START,
        "P1 START",   CFG_P1_SELECT, "P1 SELECT",   CFG_P1_UP,   "P1 UP",
        CFG_P1_DOWN,  "P1 DOWN",     CFG_P1_LEFT,   "P1 LEFT",   CFG_P1_RIGHT,
        "P1 RIGHT",   CFG_P2_A,      "P2 A",        CFG_P2_B,    "P2 B",
        CFG_P2_START, "P2 START",    CFG_P2_SELECT, "P2 SELECT", CFG_P2_UP,
        "P2 UP",      CFG_P2_DOWN,   "P2 DOWN",     CFG_P2_LEFT, "P2 LEFT",
        CFG_P2_RIGHT, "P2 RIGHT",
    };

    int i = 0;
    window_draw_text(win, 1, 1, 0, color_white, "choose key for (q to quit):");
    window_draw_text(win, 3, 4, 0, color_white, keys[i + 1]);
    window_present(win);

    SDL_Event e;
    while (1) {
        if (window_poll_event(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q))
                break;
            uint64_t kval = input_handler_encode_button(e);
            if (kval == 0)
                continue;

            // info("Chosen %016x for %s", kval, keys[i]);
            config_set_value(keys[i], kval);

            i += 2;
            if (i >= sizeof(keys) / sizeof(char*))
                break;
            window_prepare_redraw(win);
            window_draw_text(win, 1, 1, 0, color_white,
                             "choose key for (q to quit):");
            window_draw_text(win, 3, 4, 0, color_white, keys[i + 1]);
            window_present(win);
        }
    }

    config_save(DEFAULT_CFG_NAME);
    config_unload();

    free(joysticks);
    window_destroy(win);
    return 0;
}
