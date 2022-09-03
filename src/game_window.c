#include "game_window.h"
#include "alloc.h"
#include "system.h"
#include "window.h"
#include "6502_cpu.h"
#include "ppu.h"
#include "memory.h"
#include "logging.h"

#include <string.h>

static uint32_t palette_colors[] = {
    PACK_RGB(84, 84, 84),    // 0x00
    PACK_RGB(0, 30, 116),    // 0x01
    PACK_RGB(8, 16, 144),    // 0x02
    PACK_RGB(48, 0, 136),    // 0x03
    PACK_RGB(68, 0, 100),    // 0x04
    PACK_RGB(92, 0, 48),     // 0x05
    PACK_RGB(84, 4, 0),      // 0x06
    PACK_RGB(60, 24, 0),     // 0x07
    PACK_RGB(32, 42, 0),     // 0x08
    PACK_RGB(8, 58, 0),      // 0x09
    PACK_RGB(0, 64, 0),      // 0x0a
    PACK_RGB(0, 60, 0),      // 0x0b
    PACK_RGB(0, 50, 60),     // 0x0c
    PACK_RGB(0, 0, 0),       // 0x0d
    PACK_RGB(0, 0, 0),       // 0x0e
    PACK_RGB(0, 0, 0),       // 0x0f
    PACK_RGB(152, 150, 152), // 0x10
    PACK_RGB(8, 76, 196),    // 0x11
    PACK_RGB(48, 50, 236),   // 0x12
    PACK_RGB(92, 30, 228),   // 0x13
    PACK_RGB(136, 20, 176),  // 0x14
    PACK_RGB(160, 20, 100),  // 0x15
    PACK_RGB(152, 34, 32),   // 0x16
    PACK_RGB(120, 60, 0),    // 0x17
    PACK_RGB(84, 90, 0),     // 0x18
    PACK_RGB(40, 114, 0),    // 0x19
    PACK_RGB(8, 124, 0),     // 0x1a
    PACK_RGB(0, 118, 40),    // 0x1b
    PACK_RGB(0, 102, 120),   // 0x1c
    PACK_RGB(0, 0, 0),       // 0x1d
    PACK_RGB(0, 0, 0),       // 0x1e
    PACK_RGB(0, 0, 0),       // 0x1f
    PACK_RGB(236, 238, 236), // 0x20
    PACK_RGB(76, 154, 236),  // 0x21
    PACK_RGB(120, 124, 236), // 0x22
    PACK_RGB(176, 98, 236),  // 0x23
    PACK_RGB(228, 84, 236),  // 0x24
    PACK_RGB(236, 88, 180),  // 0x25
    PACK_RGB(236, 106, 100), // 0x26
    PACK_RGB(212, 136, 32),  // 0x27
    PACK_RGB(160, 170, 0),   // 0x28
    PACK_RGB(116, 196, 0),   // 0x29
    PACK_RGB(76, 208, 32),   // 0x2a
    PACK_RGB(56, 204, 108),  // 0x2b
    PACK_RGB(56, 180, 204),  // 0x2c
    PACK_RGB(60, 60, 60),    // 0x2d
    PACK_RGB(0, 0, 0),       // 0x2e
    PACK_RGB(0, 0, 0),       // 0x2f
    PACK_RGB(236, 238, 236), // 0x30
    PACK_RGB(168, 204, 236), // 0x31
    PACK_RGB(188, 188, 236), // 0x32
    PACK_RGB(212, 178, 236), // 0x33
    PACK_RGB(236, 174, 236), // 0x34
    PACK_RGB(236, 174, 212), // 0x35
    PACK_RGB(236, 180, 176), // 0x36
    PACK_RGB(228, 196, 144), // 0x37
    PACK_RGB(204, 210, 120), // 0x38
    PACK_RGB(180, 222, 120), // 0x39
    PACK_RGB(168, 226, 144), // 0x3a
    PACK_RGB(152, 226, 180), // 0x3b
    PACK_RGB(160, 214, 228), // 0x3c
    PACK_RGB(160, 162, 160), // 0x3d
    PACK_RGB(0, 0, 0),       // 0x3e
    PACK_RGB(0, 0, 0),       // 0x3f
};

GameWindow* gamewindow_build(System* sys)
{
    GameWindow* gw = malloc_or_fail(sizeof(GameWindow));
    gw->sys        = sys;

    gw->gamewin_scale  = 3;
    gw->gamewin_width  = 256;
    gw->gamewin_height = 240;
    gw->text_width     = gw->gamewin_width * 2 + gw->gamewin_width / 3;
    gw->gamewin_x      = 10;
    gw->gamewin_y      = 10;

    gw->patterntab1_y =
        gw->gamewin_height * gw->gamewin_scale - gw->gamewin_height - 30;
    gw->patterntab1_x = gw->gamewin_width * gw->gamewin_scale + 30;
    gw->patterntab2_y = gw->patterntab1_y;
    gw->patterntab2_x = gw->patterntab1_x + gw->gamewin_width + 30;

    gw->palettes_w = 10 * 4 * 8 + 5 * 8;
    gw->palettes_h = 10;
    gw->palettes_x = gw->patterntab1_x;
    gw->palettes_y = gw->patterntab1_y - 20;

    gw->win = window_build(gw->gamewin_width * gw->gamewin_scale +
                               gw->text_width + 20,
                           gw->gamewin_height * gw->gamewin_scale + 20);

    gw->text_col_off =
        gw->gamewin_width * gw->gamewin_scale / gw->win->text_char_width + 3;

    gw->gamewin_surface =
        SDL_CreateRGBSurface(0, gw->gamewin_width, gw->gamewin_height, 32,
                             0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    gw->palettes_surface =
        SDL_CreateRGBSurface(0, gw->palettes_w, gw->palettes_h, 32, 0xFF000000,
                             0x00FF0000, 0x0000FF00, 0x000000FF);
    gw->patterntab1_surface = SDL_CreateRGBSurface(
        0, 128, 128, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    gw->patterntab2_surface = SDL_CreateRGBSurface(
        0, 128, 128, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    return gw;
}

void gamewindow_destroy(GameWindow* gw)
{
    window_destroy(gw->win);
    SDL_FreeSurface(gw->gamewin_surface);
    SDL_FreeSurface(gw->palettes_surface);
    SDL_FreeSurface(gw->patterntab1_surface);
    SDL_FreeSurface(gw->patterntab2_surface);

    free(gw);
}

void gamewindow_set_pixel(GameWindow* gw, int x, int y, uint32_t rgba)
{
    if (x < 0 || x > 256 || y < 0 || y > 240)
        panic("gamewindow_set_pixel: invalid pixel %d, %d", x, y);

    uint32_t* p = (uint32_t*)((uint8_t*)gw->gamewin_surface->pixels +
                              y * gw->gamewin_surface->pitch +
                              x * gw->gamewin_surface->format->BytesPerPixel);
    *p          = rgba;
}

static void set_patterntab1_pixel(GameWindow* gw, int x, int y, uint32_t rgba)
{
    if (x < 0 || x > 256 || y < 0 || y > 240)
        panic("set_patterntab1_pixel: invalid pixel %d, %d", x, y);

    uint32_t* p =
        (uint32_t*)((uint8_t*)gw->patterntab1_surface->pixels +
                    y * gw->patterntab1_surface->pitch +
                    x * gw->patterntab1_surface->format->BytesPerPixel);
    *p = rgba;
}

static void set_patterntab2_pixel(GameWindow* gw, int x, int y, uint32_t rgba)
{
    if (x < 0 || x > 256 || y < 0 || y > 240)
        panic("set_patterntab2_pixel: invalid pixel %d, %d", x, y);

    uint32_t* p =
        (uint32_t*)((uint8_t*)gw->patterntab2_surface->pixels +
                    y * gw->patterntab2_surface->pitch +
                    x * gw->patterntab2_surface->format->BytesPerPixel);
    *p = rgba;
}

static void set_palettes_pixel(GameWindow* gw, int x, int y, uint32_t rgba)
{
    if (x < 0 || x > gw->palettes_w || y < 0 || y > gw->palettes_h)
        panic("set_palettes_pixel: invalid pixel %d, %d", x, y);

    uint32_t* p = (uint32_t*)((uint8_t*)gw->palettes_surface->pixels +
                              y * gw->palettes_surface->pitch +
                              x * gw->palettes_surface->format->BytesPerPixel);
    *p          = rgba;
}

static void draw_palettes(GameWindow* gw)
{
    int acc = 0;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 4; ++j) {
            uint16_t addr        = 0x3F00 + (i << 2) + j;
            uint8_t  palette_val = memory_read(gw->sys->ppu->mem, addr);
            uint32_t color       = palette_colors[palette_val % 64];
            for (int x = 0; x < gw->palettes_h; ++x) {
                for (int y = 0; y < gw->palettes_h; ++y) {
                    int xx =
                        4 * gw->palettes_h * i + gw->palettes_h * j + x + acc;
                    set_palettes_pixel(gw, xx, y, color);
                }
            }
        }
        acc += 5;
    }
}

static void draw_patterntables(GameWindow* gw, uint8_t palette)
{
    Ppu* ppu = gw->sys->ppu;
    for (uint16_t tile_y = 0; tile_y < 16; ++tile_y) {
        for (uint16_t tile_x = 0; tile_x < 16; ++tile_x) {
            uint16_t off = tile_y * 256 + tile_x * 16;

            for (uint16_t row = 0; row < 8; ++row) {
                uint8_t tile_lsb_1 = memory_read(ppu->mem, off + row);
                uint8_t tile_msb_1 = memory_read(ppu->mem, off + row + 8);

                uint8_t tile_lsb_2 = memory_read(ppu->mem, 0x1000 + off + row);
                uint8_t tile_msb_2 =
                    memory_read(ppu->mem, 0x1000 + off + row + 8);

                for (uint16_t col = 0; col < 8; ++col) {
                    uint8_t pixel_1 = (tile_lsb_1 & 0x01) + (tile_msb_1 & 0x01);
                    uint8_t pixel_2 = (tile_lsb_2 & 0x01) + (tile_msb_2 & 0x01);

                    uint8_t color_1 =
                        memory_read(ppu->mem,
                                    0x3F00u + (palette << 2) + pixel_1) &
                        0x3F;
                    uint8_t color_2 =
                        memory_read(ppu->mem,
                                    0x3F00u + (palette << 2) + pixel_2) &
                        0x3F;

                    set_patterntab1_pixel(gw, tile_x * 8 + (7 - col),
                                          tile_y * 8 + row,
                                          palette_colors[color_1]);
                    set_patterntab2_pixel(gw, tile_x * 8 + (7 - col),
                                          tile_y * 8 + row,
                                          palette_colors[color_2]);

                    tile_lsb_1 >>= 1;
                    tile_msb_1 >>= 1;
                    tile_lsb_2 >>= 1;
                    tile_msb_2 >>= 1;
                }
            }
        }
    }
}

void gamewindow_draw(GameWindow* gw)
{
    window_prepare_redraw(gw->win);

    draw_palettes(gw);
    draw_patterntables(gw, 0);

    SDL_Texture* gamewin_texture = SDL_CreateTextureFromSurface(
        gw->win->sdl_renderer, gw->gamewin_surface);
    SDL_Rect gamewin_rect = {.x = gw->gamewin_x,
                             .y = gw->gamewin_y,
                             .w = gw->gamewin_width * gw->gamewin_scale,
                             .h = gw->gamewin_height * gw->gamewin_scale};
    SDL_RenderCopy(gw->win->sdl_renderer, gamewin_texture, NULL, &gamewin_rect);
    SDL_DestroyTexture(gamewin_texture);

    SDL_Texture* palettes_texture = SDL_CreateTextureFromSurface(
        gw->win->sdl_renderer, gw->palettes_surface);
    SDL_Rect palettes_rect = {.x = gw->palettes_x,
                              .y = gw->palettes_y,
                              .w = gw->palettes_w,
                              .h = gw->palettes_h};
    SDL_RenderCopy(gw->win->sdl_renderer, palettes_texture, NULL,
                   &palettes_rect);
    SDL_DestroyTexture(palettes_texture);

    SDL_Texture* patterntab1_texture = SDL_CreateTextureFromSurface(
        gw->win->sdl_renderer, gw->patterntab1_surface);
    SDL_Rect patterntab1_rect = {.x = gw->patterntab1_x,
                                 .y = gw->patterntab1_y,
                                 .w = 128 * 2,
                                 .h = 128 * 2};
    SDL_RenderCopy(gw->win->sdl_renderer, patterntab1_texture, NULL,
                   &patterntab1_rect);
    SDL_DestroyTexture(patterntab1_texture);

    SDL_Texture* patterntab2_texture = SDL_CreateTextureFromSurface(
        gw->win->sdl_renderer, gw->patterntab2_surface);
    SDL_Rect patterntab2_rect = {.x = gw->patterntab2_x,
                                 .y = gw->patterntab2_y,
                                 .w = 128 * 2,
                                 .h = 128 * 2};
    SDL_RenderCopy(gw->win->sdl_renderer, patterntab2_texture, NULL,
                   &patterntab2_rect);
    SDL_DestroyTexture(patterntab2_texture);

    // Draw CPU and PPU info
    for (int32_t i = -2; i < 3; ++i) {
        int32_t addr = (int32_t)gw->sys->cpu->PC;
        addr += i;
        if (addr >= 0) {
            static char disas[64];
            memset(disas, 0, sizeof(disas));

            if (i == 0)
                strcpy(disas, ">");
            else
                strcpy(disas, " ");
            strcat(disas, cpu_disassemble(gw->sys->cpu, (uint16_t)addr));

            window_draw_text(gw->win, i + 4, gw->text_col_off - 1, color_white,
                             disas);
        }
    }

    window_draw_text(gw->win, 8, gw->text_col_off, color_white,
                     cpu_tostring(gw->sys->cpu));
    window_draw_text(gw->win, 15, gw->text_col_off, color_white,
                     ppu_tostring(gw->sys->ppu));

    window_present(gw->win);
}
