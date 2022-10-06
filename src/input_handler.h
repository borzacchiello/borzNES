#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include "system.h"

#include <SDL2/SDL.h>

typedef struct KeyBindings {
    uint64_t p1_A, p1_B, p1_SELECT, p1_START, p1_UP, p1_DOWN, p1_LEFT, p1_RIGHT;
    uint64_t p2_A, p2_B, p2_SELECT, p2_START, p2_UP, p2_DOWN, p2_LEFT, p2_RIGHT;
} KeyBindings;

typedef struct InputHandler {
    KeyBindings    kb;
    SDL_Joystick** joysticks;
    uint32_t       joysticks_size;
} InputHandler;

InputHandler* input_handler_build();
void          input_handler_destroy(InputHandler* ih);

uint64_t input_handler_encode_button(SDL_Event e);
void input_handler_get_input(InputHandler* ih, SDL_Event e, ControllerState* p1,
                             ControllerState* p2);

#endif
