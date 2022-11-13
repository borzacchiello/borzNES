#include "input_handler.h"
#include "config.h"
#include "logging.h"
#include "alloc.h"

#define INPUT_KEY       0
#define INPUT_JOYBUTTON 1
#define INPUT_JOYAXIS   2

#define HANDLE_KEY(binding, output_var)                                        \
    if (input_type(binding) == INPUT_KEY &&                                    \
        e.key.keysym.sym == key_value(binding))                                \
        output_var = v;

#define HANDLE_JOYBTN(binding, output_var)                                     \
    if (input_type(binding) == INPUT_JOYBUTTON && joy_n == joy_num(binding) && \
        joy_btn == joy_button(binding))                                        \
        output_var = v;

#define HANDLE_JOYAXIS(binding, output_var)                                    \
    if (input_type(binding) == INPUT_JOYAXIS && joy_n == joy_num(binding) &&   \
        joy_a == joy_axis(binding))                                            \
        output_var = v == 0 ? v : (joy_v == joy_value(binding) ? 1 : 0);

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
    LOAD_OR_FAIL(CFG_KEY_MUTE, &ih->kb.mute);
    LOAD_OR_FAIL(CFG_KEY_LOAD_STATE, &ih->kb.load_state);
    LOAD_OR_FAIL(CFG_KEY_SAVE_STATE, &ih->kb.save_state);
    LOAD_OR_FAIL(CFG_KEY_FAST_MODE, &ih->kb.fast_mode);
    LOAD_OR_FAIL(CFG_KEY_SLOW_MODE, &ih->kb.slow_mode);
    LOAD_OR_FAIL(CFG_KEY_REWIND, &ih->kb.rewind);

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
                             ControllerState* p2, MiscKeys* k)
{
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        uint8_t v = e.type == SDL_KEYDOWN ? 1 : 0;

        if (p1) {
            HANDLE_KEY(ih->kb.p1_A, p1->A)
            HANDLE_KEY(ih->kb.p1_B, p1->B)
            HANDLE_KEY(ih->kb.p1_START, p1->START)
            HANDLE_KEY(ih->kb.p1_SELECT, p1->SELECT)
            HANDLE_KEY(ih->kb.p1_UP, p1->UP)
            HANDLE_KEY(ih->kb.p1_DOWN, p1->DOWN)
            HANDLE_KEY(ih->kb.p1_LEFT, p1->LEFT)
            HANDLE_KEY(ih->kb.p1_RIGHT, p1->RIGHT)
        }
        if (p2) {
            HANDLE_KEY(ih->kb.p2_A, p2->A)
            HANDLE_KEY(ih->kb.p2_B, p2->B)
            HANDLE_KEY(ih->kb.p2_START, p2->START)
            HANDLE_KEY(ih->kb.p2_SELECT, p2->SELECT)
            HANDLE_KEY(ih->kb.p2_UP, p2->UP)
            HANDLE_KEY(ih->kb.p2_DOWN, p2->DOWN)
            HANDLE_KEY(ih->kb.p2_LEFT, p2->LEFT)
            HANDLE_KEY(ih->kb.p2_RIGHT, p2->RIGHT)
        }
        if (k) {
            HANDLE_KEY(ih->kb.mute, k->mute)
            HANDLE_KEY(ih->kb.load_state, k->load_state)
            HANDLE_KEY(ih->kb.save_state, k->save_state)
            HANDLE_KEY(ih->kb.fast_mode, k->fast_mode)
            HANDLE_KEY(ih->kb.slow_mode, k->slow_mode)
            HANDLE_KEY(ih->kb.rewind, k->rewind)
        }
    } else if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) {
        uint8_t v       = e.type == SDL_JOYBUTTONDOWN ? 1 : 0;
        int     joy_n   = e.jbutton.which + 1;
        int     joy_btn = e.jbutton.button;

        if (p1) {
            HANDLE_JOYBTN(ih->kb.p1_A, p1->A)
            HANDLE_JOYBTN(ih->kb.p1_B, p1->B)
            HANDLE_JOYBTN(ih->kb.p1_START, p1->START)
            HANDLE_JOYBTN(ih->kb.p1_SELECT, p1->SELECT)
            HANDLE_JOYBTN(ih->kb.p1_UP, p1->UP)
            HANDLE_JOYBTN(ih->kb.p1_DOWN, p1->DOWN)
            HANDLE_JOYBTN(ih->kb.p1_LEFT, p1->LEFT)
            HANDLE_JOYBTN(ih->kb.p1_RIGHT, p1->RIGHT)
        }
        if (p2) {
            HANDLE_JOYBTN(ih->kb.p2_A, p2->A)
            HANDLE_JOYBTN(ih->kb.p2_B, p2->B)
            HANDLE_JOYBTN(ih->kb.p2_START, p2->START)
            HANDLE_JOYBTN(ih->kb.p2_SELECT, p2->SELECT)
            HANDLE_JOYBTN(ih->kb.p2_UP, p2->UP)
            HANDLE_JOYBTN(ih->kb.p2_DOWN, p2->DOWN)
            HANDLE_JOYBTN(ih->kb.p2_LEFT, p2->LEFT)
            HANDLE_JOYBTN(ih->kb.p2_RIGHT, p2->RIGHT)
        }
        if (k) {
            HANDLE_JOYBTN(ih->kb.mute, k->mute)
            HANDLE_JOYBTN(ih->kb.load_state, k->load_state)
            HANDLE_JOYBTN(ih->kb.save_state, k->save_state)
            HANDLE_JOYBTN(ih->kb.fast_mode, k->fast_mode)
            HANDLE_JOYBTN(ih->kb.slow_mode, k->slow_mode)
            HANDLE_JOYBTN(ih->kb.rewind, k->rewind)
        }
    } else if (e.type == SDL_JOYAXISMOTION) {
        uint64_t joy_n = e.jaxis.which + 1;
        uint64_t joy_a = e.jaxis.axis & 1;
        uint64_t joy_v = e.jaxis.value < 0 ? 0 : 1;
        uint8_t  v     = 1;
        if (e.jaxis.value >= -joy_axis_cutoff &&
            e.jaxis.value <= joy_axis_cutoff) {
            joy_v = 0xff;
            v     = 0;
        }

        if (p1) {
            HANDLE_JOYAXIS(ih->kb.p1_A, p1->A)
            HANDLE_JOYAXIS(ih->kb.p1_B, p1->B)
            HANDLE_JOYAXIS(ih->kb.p1_START, p1->START)
            HANDLE_JOYAXIS(ih->kb.p1_SELECT, p1->SELECT)
            HANDLE_JOYAXIS(ih->kb.p1_UP, p1->UP)
            HANDLE_JOYAXIS(ih->kb.p1_DOWN, p1->DOWN)
            HANDLE_JOYAXIS(ih->kb.p1_LEFT, p1->LEFT)
            HANDLE_JOYAXIS(ih->kb.p1_RIGHT, p1->RIGHT)
        }
        if (p2) {
            HANDLE_JOYAXIS(ih->kb.p2_A, p2->A)
            HANDLE_JOYAXIS(ih->kb.p2_B, p2->B)
            HANDLE_JOYAXIS(ih->kb.p2_START, p2->START)
            HANDLE_JOYAXIS(ih->kb.p2_SELECT, p2->SELECT)
            HANDLE_JOYAXIS(ih->kb.p2_UP, p2->UP)
            HANDLE_JOYAXIS(ih->kb.p2_DOWN, p2->DOWN)
            HANDLE_JOYAXIS(ih->kb.p2_LEFT, p2->LEFT)
            HANDLE_JOYAXIS(ih->kb.p2_RIGHT, p2->RIGHT)
        }
        if (k) {
            HANDLE_JOYAXIS(ih->kb.mute, k->mute)
            HANDLE_JOYAXIS(ih->kb.load_state, k->load_state)
            HANDLE_JOYAXIS(ih->kb.save_state, k->save_state)
            HANDLE_JOYAXIS(ih->kb.fast_mode, k->fast_mode)
            HANDLE_JOYAXIS(ih->kb.slow_mode, k->slow_mode)
            HANDLE_JOYAXIS(ih->kb.rewind, k->rewind)
        }
    }
}

void input_handler_destroy(InputHandler* ih)
{
    for (int i = 0; i < ih->joysticks_size; ++i)
        SDL_JoystickClose(ih->joysticks[i]);
    free_or_fail(ih->joysticks);
    free_or_fail(ih);
}
