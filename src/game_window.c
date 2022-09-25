#include "game_window.h"
#include "alloc.h"
#include "system.h"
#include "window.h"
#include "6502_cpu.h"
#include "ppu.h"
#include "memory.h"
#include "logging.h"

#include <string.h>
#include <sys/time.h>

#define SHOW_FPS      1
#define SHOW_CPU_INFO 1
#define SHOW_PPU_INFO 1

static long get_timestamp_milliseconds()
{
    struct timeval te;
    gettimeofday(&te, NULL);

    return te.tv_sec * 1000LL + te.tv_usec / 1000;
}

long get_timestamp_microseconds()
{
    struct timeval te;
    gettimeofday(&te, NULL);

    return te.tv_sec * 1000000LL + te.tv_usec;
}

static long g_prev_timestamp = 0;

GameWindow* gamewindow_build(System* sys)
{
    GameWindow* gw = malloc_or_fail(sizeof(GameWindow));
    gw->sys        = sys;

    gw->gamewin_scale    = 3;
    gw->gamewin_width    = 256;
    gw->gamewin_height   = 240;
    gw->text_top_padding = 1;
    gw->text_width       = gw->gamewin_width * 2 + gw->gamewin_width / 3;
    gw->gamewin_x        = 10;
    gw->gamewin_y        = 10;

    gw->patterntab1_y =
        gw->gamewin_height * gw->gamewin_scale - gw->gamewin_height - 30;
    gw->patterntab1_x = gw->gamewin_width * gw->gamewin_scale + 30;
    gw->patterntab2_y = gw->patterntab1_y;
    gw->patterntab2_x = gw->patterntab1_x + gw->gamewin_width + 30;

    gw->palettes_w = 10 * 4 * 8 + 5 * 8;
    gw->palettes_h = 10;
    gw->palettes_x = gw->patterntab1_x;
    gw->palettes_y = gw->patterntab1_y - 20;

    gw->patterntab_palette_idx = 0;

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

    g_prev_timestamp = get_timestamp_milliseconds();
    ppu_set_game_window(sys->ppu, gw);
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

    static int draw_context_counter = 0;
    if (++draw_context_counter == 16) {
        draw_context_counter = 0;
        draw_palettes(gw);
        draw_patterntables(gw, gw->patterntab_palette_idx);
    }

#if SHOW_FPS
    static char fps_str[32];
    static int  show_fps_counter = 0;
    if (++show_fps_counter == 30) {
        show_fps_counter = 0;
        memset(fps_str, 0, sizeof(fps_str));

        long   dt  = get_timestamp_milliseconds() - g_prev_timestamp;
        double fps = 1000.0l / dt;
        sprintf(fps_str, "FPS: %.03lf", fps);
    }
    g_prev_timestamp = get_timestamp_milliseconds();
    window_draw_text(gw->win, gw->text_top_padding, gw->text_col_off,
                     color_white, fps_str);
#endif

#if SHOW_CPU_INFO
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

            window_draw_text(gw->win, i + 4 + gw->text_top_padding,
                             gw->text_col_off - 1, color_white, disas);
        }
    }

    window_draw_text(gw->win, 8 + gw->text_top_padding, gw->text_col_off,
                     color_white, cpu_tostring(gw->sys->cpu));
#endif
#if SHOW_PPU_INFO
    window_draw_text(gw->win, 15 + gw->text_top_padding, gw->text_col_off,
                     color_white, ppu_tostring(gw->sys->ppu));
#endif

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

    window_present(gw->win);
}
