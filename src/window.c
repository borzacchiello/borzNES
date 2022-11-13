#include "window.h"
#include "alloc.h"
#include "logging.h"

#define MAX_WIDTH  4096u
#define MAX_HEIGHT 4096u

#define MAX_ROWS 512u
#define MAX_COLS 512u

Color color_white = {255, 255, 255, 0};
Color color_black = {0, 0, 0, 0};

static volatile int g_is_window_created = 0;

#ifdef __MINGW32__
extern char* strdup(const char*);
extern char* strtok(char* str, const char* delim);
#endif

static inline SDL_Color to_sdl_color(Color c)
{
    SDL_Color sdl_c = {c.r, c.g, c.b, c.a};
    return sdl_c;
}

Window* window_build(uint32_t width, uint32_t height)
{
    if (!SDL_WasInit(SDL_INIT_VIDEO))
        panic("you must init SDL Video first");

    if (width > MAX_WIDTH)
        panic("width is too high (%u), the maximum value is %u", width,
              MAX_WIDTH);
    if (height > MAX_HEIGHT)
        panic("height is too high (%u), the maximum value is %u", height,
              MAX_HEIGHT);

    if (g_is_window_created)
        panic("you cannot create two windows");
    g_is_window_created = 1;

    Window* win = malloc_or_fail(sizeof(Window));
    win->width  = width;
    win->height = height;

    SDL_CreateWindowAndRenderer(width, height, 0, &win->sdl_window,
                                &win->sdl_renderer);
    SDL_SetRenderDrawColor(win->sdl_renderer, 0, 0, 0, 0);
    SDL_RenderClear(win->sdl_renderer);

    TTF_Init();
    win->text_font = TTF_OpenFont("courier.ttf", 20);
    if (win->text_font == NULL)
        panic("unable to find the font");

    // Deduce font size
    SDL_Color    textColor = {255, 255, 255, 0};
    SDL_Surface* surface = TTF_RenderText_Solid(win->text_font, "A", textColor);
    win->text_char_width = surface->w;
    win->text_char_height = surface->h;
    win->text_rows        = height / (uint32_t)surface->h;
    win->text_cols        = width / (uint32_t)surface->w;
    SDL_FreeSurface(surface);

    return win;
}

Window* window_build_for_text(uint32_t rows, uint32_t cols)
{
    if (rows > MAX_ROWS)
        panic("rows is too high (%u), the maximum value is %u", rows, MAX_ROWS);
    if (cols > MAX_COLS)
        panic("cols is too high (%u), the maximum value is %u", cols, MAX_COLS);

    if (g_is_window_created)
        panic("you cannot create two windows");
    g_is_window_created = 1;

    Window* win    = malloc_or_fail(sizeof(Window));
    win->text_rows = rows;
    win->text_cols = cols;

    TTF_Init();
    win->text_font = TTF_OpenFont("courier.ttf", 20);
    if (win->text_font == NULL)
        panic("unable to find the font");

    // Deduce font size
    SDL_Color    textColor = {255, 255, 255, 0};
    SDL_Surface* surface = TTF_RenderText_Solid(win->text_font, "A", textColor);
    win->text_char_width = surface->w;
    win->text_char_height = surface->h;
    win->width            = win->text_cols * (uint32_t)surface->w;
    win->height           = win->text_rows * (uint32_t)surface->h;
    SDL_FreeSurface(surface);

    SDL_CreateWindowAndRenderer(win->width, win->height, 0, &win->sdl_window,
                                &win->sdl_renderer);
    SDL_SetRenderDrawColor(win->sdl_renderer, 0, 0, 0, 0);
    SDL_RenderClear(win->sdl_renderer);

    return win;
}

void window_destroy(Window* win)
{
    g_is_window_created = 0;

    SDL_DestroyRenderer(win->sdl_renderer);
    SDL_DestroyWindow(win->sdl_window);
    TTF_CloseFont(win->text_font);
    TTF_Quit();

    free_or_fail(win);
}

static void window_draw_text_internal(Window* win, uint32_t row, uint32_t col,
                                      int shaded, Color color, char* text)
{
    uint32_t tlen = (uint32_t)strlen(text);
    if (col > win->text_cols || tlen + col > win->text_cols)
        panic("unable to draw text: column overflow");

    if (row >= win->text_rows)
        panic("unable to draw text: row overflow");

    SDL_Color    textColor = to_sdl_color(color);
    SDL_Surface* surface;
    if (!shaded)
        surface = TTF_RenderText_Solid(win->text_font, text, textColor);
    else
        surface = TTF_RenderText_Shaded(win->text_font, text, textColor,
                                        to_sdl_color(color_black));
    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(win->sdl_renderer, surface);
    SDL_Rect text_rect = {.y = row * win->text_char_height,
                          .x = col * win->text_char_width,
                          .h = surface->h,
                          .w = surface->w};

    SDL_RenderCopy(win->sdl_renderer, texture, NULL, &text_rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void window_draw_text(Window* win, uint32_t row, uint32_t col, int shaded,
                      Color color, const char* text)
{
    char* text_dup = strdup(text);
    if (text_dup == NULL)
        panic("strdup failed");

    uint32_t i     = 0;
    char*    token = strtok(text_dup, "\n");
    while (token != NULL) {
        window_draw_text_internal(win, row + i++, col, shaded, color, token);
        token = strtok(NULL, "\n");
    }

    free_or_fail(text_dup);
}

void window_draw_pixel(Window* win, Point p, Color c)
{
    SDL_SetRenderDrawColor(win->sdl_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawPoint(win->sdl_renderer, p.x, p.y);
}

void window_prepare_redraw(Window* win)
{
    SDL_SetRenderDrawColor(win->sdl_renderer, 0, 0, 0, 0);
    SDL_RenderClear(win->sdl_renderer);
}

void window_present(Window* win) { SDL_RenderPresent(win->sdl_renderer); }

int window_poll_event(SDL_Event* event) { return SDL_PollEvent(event); }
