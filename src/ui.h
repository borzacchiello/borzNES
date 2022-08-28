#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>

typedef struct {
    uint32_t      width;
    uint32_t      height;
    TTF_Font*     text_font;
    uint32_t      text_char_width;
    uint32_t      text_char_height;
    uint32_t      text_rows;
    uint32_t      text_cols;
    SDL_Window*   sdl_window;
    SDL_Renderer* sdl_renderer;
} Window;

typedef struct {
    uint8_t r, g, b, a;
} Color;

typedef struct {
    uint32_t x, y;
} Point;

extern Color color_white;
extern Color color_black;

Window* window_build(uint32_t width, uint32_t height);
Window* window_build_for_text(uint32_t rows, uint32_t cols);
void    window_destroy(Window* win);

void window_draw_text(Window* win, uint32_t row, uint32_t col, Color color,
                      char* text);
void window_draw_pixel(Window* win, Point p, Color c);
void window_present(Window* win);

int window_poll_event(SDL_Event* event);

#endif
