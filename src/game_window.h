#ifndef GAME_WINDOW_H
#define GAME_WINDOW_H

#include <stdint.h>
#include <SDL2/SDL.h>

#define PACK_RGBA(r, g, b, a)                                                  \
    (((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a)
#define PACK_RGB(r, g, b)                                                      \
    (((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | 0xFFu)

extern long latency;

struct System;
struct Window;

typedef struct GameWindow {
    void* obj;
    void (*set_pixel)(void* obj, int x, int y, uint32_t rgba);
    void (*draw)(void* obj);
    void (*destroy)(void* obj);
} GameWindow;

GameWindow* rich_gw_build(struct System* sys);
GameWindow* simple_gw_build(struct System* sys);

void gamewindow_destroy(GameWindow* gw);
void gamewindow_set_pixel(GameWindow* gw, int x, int y, uint32_t rgba);
void gamewindow_draw(GameWindow* gw);

long get_timestamp_microseconds();

#endif
