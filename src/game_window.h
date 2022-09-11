#ifndef GAME_WINDOW_H
#define GAME_WINDOW_H

#include <stdint.h>
#include <SDL2/SDL.h>

#define PACK_RGBA(r, g, b, a)                                                  \
    (((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a)
#define PACK_RGB(r, g, b)                                                      \
    (((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 0xFFu)

struct System;
struct Window;

typedef struct GameWindow {
    struct Window* win;
    struct System* sys;

    int gamewin_scale;
    int gamewin_width;
    int gamewin_height;
    int gamewin_x, gamewin_y;
    int patterntab1_x, patterntab1_y;
    int patterntab2_x, patterntab2_y;
    int palettes_w, palettes_h;
    int palettes_x, palettes_y;

    int text_width;
    int text_col_off;

    SDL_Surface* gamewin_surface;
    SDL_Surface* palettes_surface;
    SDL_Surface* patterntab1_surface;
    SDL_Surface* patterntab2_surface;
} GameWindow;

GameWindow* gamewindow_build(struct System* sys);
void        gamewindow_destroy(GameWindow* gw);

void gamewindow_set_pixel(GameWindow* gw, int x, int y, uint32_t rgba);
void gamewindow_draw(GameWindow* gw);

long get_timestamp_milliseconds();

#endif
