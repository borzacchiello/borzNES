#include "input_handler.h"
#include "config.h"
#include "logging.h"
#include "alloc.h"

#define INPUT_KEY       0
#define INPUT_JOYBUTTON 1
#define INPUT_JOYAXIS   2

#define LOAD_OR_FAIL(key, oval)                                                \
    if (!config_get_value(key, oval))                                          \
        panic("%s not found in config", key);

static const int joy_axis_cutoff = 8000;

InputHandler* input_handler_build()
{
    if (!SDL_WasInit(SDL_INIT_JOYSTICK))
        panic("you must init SDL joystick first");

    SDL_JoystickEventState(SDL_ENABLE);
    InputHandler* ih = calloc_or_fail(sizeof(InputHandler));

    ih->joysticks_size = SDL_NumJoysticks();
    if (ih->joysticks_size > 0) {
        ih->joysticks =
            calloc_or_fail(sizeof(SDL_Joystick*) * ih->joysticks_size);

        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            ih->joysticks[i] = SDL_JoystickOpen(i);
            // info("Opened joystick %s\n", SDL_JoystickName(ih->joysticks[i]));
        }
    }

    LOAD_OR_FAIL(CFG_P1_A, &ih->kb.p1_A);
    LOAD_OR_FAIL(CFG_P1_B, &ih->kb.p1_B);
    LOAD_OR_FAIL(CFG_P1_START, &ih->kb.p1_START);
    LOAD_OR_FAIL(CFG_P1_SELECT, &ih->kb.p1_SELECT);
    LOAD_OR_FAIL(CFG_P1_UP, &ih->kb.p1_UP);
    LOAD_OR_FAIL(CFG_P1_DOWN, &ih->kb.p1_DOWN);
    LOAD_OR_FAIL(CFG_P1_LEFT, &ih->kb.p1_LEFT);
    LOAD_OR_FAIL(CFG_P1_RIGHT, &ih->kb.p1_RIGHT);
    LOAD_OR_FAIL(CFG_P2_A, &ih->kb.p2_A);
    LOAD_OR_FAIL(CFG_P2_B, &ih->kb.p2_B);
    LOAD_OR_FAIL(CFG_P2_START, &ih->kb.p2_START);
    LOAD_OR_FAIL(CFG_P2_SELECT, &ih->kb.p2_SELECT);
    LOAD_OR_FAIL(CFG_P2_UP, &ih->kb.p2_UP);
    LOAD_OR_FAIL(CFG_P2_DOWN, &ih->kb.p2_DOWN);
    LOAD_OR_FAIL(CFG_P2_LEFT, &ih->kb.p2_LEFT);
    LOAD_OR_FAIL(CFG_P2_RIGHT, &ih->kb.p2_RIGHT);

    return ih;
}

uint64_t input_handler_encode_button(SDL_Event e)
{
    if (e.type == SDL_KEYDOWN) {
        return (uint64_t)e.key.keysym.sym << 2;
    } else if (e.type == SDL_JOYBUTTONDOWN) {
        uint64_t joy_n = e.jbutton.which + 1;
        uint64_t joy_b = e.jbutton.button;
        return (joy_n << 32) | (joy_b << 2) | 1;
    } else if (e.type == SDL_JOYAXISMOTION) {
        if (e.jaxis.value >= -joy_axis_cutoff &&
            e.jaxis.value <= joy_axis_cutoff)
            return 0;

        uint64_t joy_n = e.jaxis.which + 1;
        uint64_t joy_a = e.jaxis.axis & 1;
        uint64_t joy_v = e.jaxis.value < 0 ? 0 : 1;
        return (joy_n << 32) | (joy_a << 3) | (joy_v << 2) | 2;
    }
    return 0;
}

static inline int input_type(uint64_t k) { return k & 3; }
static inline int key_value(uint64_t k) { return k >> 2; }
static inline int joy_num(uint64_t k) { return k >> 32; }
static inline int joy_button(uint64_t k) { return (k >> 2) & 0xff; }
static inline int joy_axis(uint64_t k) { return (k >> 3) & 1; }
static inline int joy_value(uint64_t k) { return (k >> 2) & 1; }

void input_handler_get_input(InputHandler* ih, SDL_Event e, ControllerState* p1,
                             ControllerState* p2)
{
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        uint8_t v = e.type == SDL_KEYDOWN ? 1 : 0;

        if (p1) {
            if (input_type(ih->kb.p1_A) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_A))
                p1->A = v;
            if (input_type(ih->kb.p1_B) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_B))
                p1->B = v;
            if (input_type(ih->kb.p1_START) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_START))
                p1->START = v;
            if (input_type(ih->kb.p1_SELECT) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_SELECT))
                p1->SELECT = v;
            if (input_type(ih->kb.p1_UP) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_UP))
                p1->UP = v;
            if (input_type(ih->kb.p1_DOWN) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_DOWN))
                p1->DOWN = v;
            if (input_type(ih->kb.p1_LEFT) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_LEFT))
                p1->LEFT = v;
            if (input_type(ih->kb.p1_RIGHT) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p1_RIGHT))
                p1->RIGHT = v;
        }
        if (p2) {
            if (input_type(ih->kb.p2_A) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_A))
                p2->A = v;
            if (input_type(ih->kb.p2_B) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_B))
                p2->B = v;
            if (input_type(ih->kb.p2_START) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_START))
                p2->START = v;
            if (input_type(ih->kb.p2_SELECT) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_SELECT))
                p2->SELECT = v;
            if (input_type(ih->kb.p2_UP) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_UP))
                p2->UP = v;
            if (input_type(ih->kb.p2_DOWN) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_DOWN))
                p2->DOWN = v;
            if (input_type(ih->kb.p2_LEFT) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_LEFT))
                p2->LEFT = v;
            if (input_type(ih->kb.p2_RIGHT) == INPUT_KEY &&
                e.key.keysym.sym == key_value(ih->kb.p2_RIGHT))
                p2->RIGHT = v;
        }
    } else if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) {
        uint8_t v       = e.type == SDL_JOYBUTTONDOWN ? 1 : 0;
        int     joy_n   = e.jbutton.which + 1;
        int     joy_btn = e.jbutton.button;

        if (p1) {
            if (input_type(ih->kb.p1_A) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_A) &&
                joy_btn == joy_button(ih->kb.p1_A))
                p1->A = v;
            if (input_type(ih->kb.p1_B) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_B) &&
                joy_btn == joy_button(ih->kb.p1_B))
                p1->B = v;
            if (input_type(ih->kb.p1_START) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_START) &&
                joy_btn == joy_button(ih->kb.p1_START))
                p1->START = v;
            if (input_type(ih->kb.p1_SELECT) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_SELECT) &&
                joy_btn == joy_button(ih->kb.p1_SELECT))
                p1->SELECT = v;
            if (input_type(ih->kb.p1_UP) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_UP) &&
                joy_btn == joy_button(ih->kb.p1_UP))
                p1->UP = v;
            if (input_type(ih->kb.p1_DOWN) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_DOWN) &&
                joy_btn == joy_button(ih->kb.p1_DOWN))
                p1->DOWN = v;
            if (input_type(ih->kb.p1_LEFT) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_LEFT) &&
                joy_btn == joy_button(ih->kb.p1_LEFT))
                p1->LEFT = v;
            if (input_type(ih->kb.p1_RIGHT) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p1_RIGHT) &&
                joy_btn == joy_button(ih->kb.p1_RIGHT))
                p1->RIGHT = v;
        }
        if (p2) {
            if (input_type(ih->kb.p2_A) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_A) &&
                joy_btn == joy_button(ih->kb.p2_A))
                p2->A = v;
            if (input_type(ih->kb.p2_B) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_B) &&
                joy_btn == joy_button(ih->kb.p2_B))
                p2->B = v;
            if (input_type(ih->kb.p2_START) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_START) &&
                joy_btn == joy_button(ih->kb.p2_START))
                p2->START = v;
            if (input_type(ih->kb.p2_SELECT) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_SELECT) &&
                joy_btn == joy_button(ih->kb.p2_SELECT))
                p2->SELECT = v;
            if (input_type(ih->kb.p2_UP) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_UP) &&
                joy_btn == joy_button(ih->kb.p2_UP))
                p2->UP = v;
            if (input_type(ih->kb.p2_DOWN) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_DOWN) &&
                joy_btn == joy_button(ih->kb.p2_DOWN))
                p2->DOWN = v;
            if (input_type(ih->kb.p2_LEFT) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_LEFT) &&
                joy_btn == joy_button(ih->kb.p2_LEFT))
                p2->LEFT = v;
            if (input_type(ih->kb.p2_RIGHT) == INPUT_JOYBUTTON &&
                joy_n == joy_num(ih->kb.p2_RIGHT) &&
                joy_btn == joy_button(ih->kb.p2_RIGHT))
                p2->RIGHT = v;
        }
    } else if (e.type == SDL_JOYAXISMOTION) {
        if (e.jaxis.value >= -joy_axis_cutoff &&
            e.jaxis.value <= joy_axis_cutoff) {
            if (p1) {
                if (input_type(ih->kb.p1_A) == INPUT_JOYAXIS)
                    p1->A = 0;
                if (input_type(ih->kb.p1_B) == INPUT_JOYAXIS)
                    p1->B = 0;
                if (input_type(ih->kb.p1_START) == INPUT_JOYAXIS)
                    p1->START = 0;
                if (input_type(ih->kb.p1_SELECT) == INPUT_JOYAXIS)
                    p1->SELECT = 0;
                if (input_type(ih->kb.p1_UP) == INPUT_JOYAXIS)
                    p1->UP = 0;
                if (input_type(ih->kb.p1_DOWN) == INPUT_JOYAXIS)
                    p1->DOWN = 0;
                if (input_type(ih->kb.p1_LEFT) == INPUT_JOYAXIS)
                    p1->LEFT = 0;
                if (input_type(ih->kb.p1_RIGHT) == INPUT_JOYAXIS)
                    p1->RIGHT = 0;
            }
            if (p2) {
                if (input_type(ih->kb.p2_A) == INPUT_JOYAXIS)
                    p2->A = 0;
                if (input_type(ih->kb.p2_B) == INPUT_JOYAXIS)
                    p2->B = 0;
                if (input_type(ih->kb.p2_START) == INPUT_JOYAXIS)
                    p2->START = 0;
                if (input_type(ih->kb.p2_SELECT) == INPUT_JOYAXIS)
                    p2->SELECT = 0;
                if (input_type(ih->kb.p2_UP) == INPUT_JOYAXIS)
                    p2->UP = 0;
                if (input_type(ih->kb.p2_DOWN) == INPUT_JOYAXIS)
                    p2->DOWN = 0;
                if (input_type(ih->kb.p2_LEFT) == INPUT_JOYAXIS)
                    p2->LEFT = 0;
                if (input_type(ih->kb.p2_RIGHT) == INPUT_JOYAXIS)
                    p2->RIGHT = 0;
            }
            return;
        }
        uint8_t  v     = 1;
        uint64_t joy_n = e.jaxis.which + 1;
        uint64_t joy_a = e.jaxis.axis & 1;
        uint64_t joy_v = e.jaxis.value < 0 ? 0 : 1;

        if (p1) {
            if (input_type(ih->kb.p1_A) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_A) &&
                joy_a == joy_axis(ih->kb.p1_A) &&
                joy_v == joy_value(ih->kb.p1_A))
                p1->A = v;
            if (input_type(ih->kb.p1_B) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_B) &&
                joy_a == joy_axis(ih->kb.p1_B) &&
                joy_v == joy_value(ih->kb.p1_B))
                p1->B = v;
            if (input_type(ih->kb.p1_START) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_START) &&
                joy_a == joy_axis(ih->kb.p1_START) &&
                joy_v == joy_value(ih->kb.p1_START))
                p1->START = v;
            if (input_type(ih->kb.p1_SELECT) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_SELECT) &&
                joy_a == joy_axis(ih->kb.p1_SELECT) &&
                joy_v == joy_value(ih->kb.p1_SELECT))
                p1->SELECT = v;
            if (input_type(ih->kb.p1_UP) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_UP) &&
                joy_a == joy_axis(ih->kb.p1_UP) &&
                joy_v == joy_value(ih->kb.p1_UP))
                p1->UP = v;
            if (input_type(ih->kb.p1_DOWN) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_DOWN) &&
                joy_a == joy_axis(ih->kb.p1_DOWN) &&
                joy_v == joy_value(ih->kb.p1_DOWN))
                p1->DOWN = v;
            if (input_type(ih->kb.p1_LEFT) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_LEFT) &&
                joy_a == joy_axis(ih->kb.p1_LEFT) &&
                joy_v == joy_value(ih->kb.p1_LEFT))
                p1->LEFT = v;
            if (input_type(ih->kb.p1_RIGHT) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p1_RIGHT) &&
                joy_a == joy_axis(ih->kb.p1_RIGHT) &&
                joy_v == joy_value(ih->kb.p1_RIGHT))
                p1->RIGHT = v;
        }
        if (p2) {
            if (input_type(ih->kb.p2_A) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_A) &&
                joy_a == joy_axis(ih->kb.p2_A) &&
                joy_v == joy_value(ih->kb.p2_A))
                p2->A = v;
            if (input_type(ih->kb.p2_B) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_B) &&
                joy_a == joy_axis(ih->kb.p2_B) &&
                joy_v == joy_value(ih->kb.p2_B))
                p2->B = v;
            if (input_type(ih->kb.p2_START) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_START) &&
                joy_a == joy_axis(ih->kb.p2_START) &&
                joy_v == joy_value(ih->kb.p2_START))
                p2->START = v;
            if (input_type(ih->kb.p2_SELECT) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_SELECT) &&
                joy_a == joy_axis(ih->kb.p2_SELECT) &&
                joy_v == joy_value(ih->kb.p2_SELECT))
                p2->SELECT = v;
            if (input_type(ih->kb.p2_UP) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_UP) &&
                joy_a == joy_axis(ih->kb.p2_UP) &&
                joy_v == joy_value(ih->kb.p2_UP))
                p2->UP = v;
            if (input_type(ih->kb.p2_DOWN) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_DOWN) &&
                joy_a == joy_axis(ih->kb.p2_DOWN) &&
                joy_v == joy_value(ih->kb.p2_DOWN))
                p2->DOWN = v;
            if (input_type(ih->kb.p2_LEFT) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_LEFT) &&
                joy_a == joy_axis(ih->kb.p2_LEFT) &&
                joy_v == joy_value(ih->kb.p2_LEFT))
                p2->LEFT = v;
            if (input_type(ih->kb.p2_RIGHT) == INPUT_JOYAXIS &&
                joy_n == joy_num(ih->kb.p2_RIGHT) &&
                joy_a == joy_axis(ih->kb.p2_RIGHT) &&
                joy_v == joy_value(ih->kb.p2_RIGHT))
                p2->RIGHT = v;
        }
    }
}

void input_handler_destroy(InputHandler* ih)
{
    for (int i = 0; i < ih->joysticks_size; ++i)
        SDL_JoystickClose(ih->joysticks[i]);
    free(ih->joysticks);
    free(ih);
}
