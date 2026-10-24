/*
 * hal_display_sim.c — SDL2 display backend (host simulator).
 *
 * Presents the same 240x320 RGB565 framebuffer the firmware uses, in an
 * SDL2 window scaled up so the pixels are visible.
 *
 * Colour handling
 * ---------------
 * The framebuffer is RGB565, exactly as on the device. SDL's native
 * pixel format is 32-bit, so the texture is created as ARGB8888 and
 * each RGB565 value is expanded during the upload. The expansion is
 * done with integer bit replication rather than a floating-point
 * scale, so the simulator's colours match the device's exactly.
 *
 * Dirty rectangles
 * ----------------
 * The SDL backend can update a sub-rectangle of the texture directly,
 * because SDL_TextureUpdate supports a source rectangle. The contract
 * is therefore identical to the firmware's: the same pixels end up in
 * the same place. Only the transport differs (the firmware has to
 * pack rows because the panel driver has no stride-aware blit).
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "hal/hal_display.h"

#include "boards/synth_board.h"
#include "hal/hal_log.h"

#include <SDL2/SDL.h>

#include <stdlib.h>
#include <string.h>

static const char *TAG = "hal_disp_sim";

/* How many times the 240x320 panel is scaled up in the window. */
#define SIM_SCALE 3

static bool      s_initialised;
static SDL_Window   *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture  *s_texture;
static uint16_t *s_framebuffer;
static uint8_t   s_backlight_pct;

/* Expand one RGB565 value into the 32-bit ARGB8888 the texture uses.
 *
 * The 5/6/5 bit fields are replicated into the 8-bit channels:
 *   r5 -> r8 = (r5 << 3) | (r5 >> 2)
 *   g6 -> g8 = (g6 << 2) | (g6 >> 4)
 *   b5 -> b8 = (b5 << 3) | (b5 >> 2)
 */
static inline uint32_t rgb565_to_argb8888(uint16_t c)
{
    uint32_t r5 = (c >> 11) & 0x1Fu;
    uint32_t g6 = (c >> 5)  & 0x3Fu;
    uint32_t b5 =  c        & 0x1Fu;

    uint32_t r8 = (r5 << 3) | (r5 >> 2);
    uint32_t g8 = (g6 << 2) | (g6 >> 4);
    uint32_t b8 = (b5 << 3) | (b5 >> 2);

    return 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
}

sys_status_t hal_display_init(void)
{
    if (s_initialised) {
        return SYS_OK;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        HAL_LOG_ERROR(TAG, "SDL_Init failed: %s", SDL_GetError());
        return SYS_ERR_IO;
    }

    s_window = SDL_CreateWindow("Ravine Phoenix — Simulator",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                SYNTH_DISPLAY_WIDTH * SIM_SCALE,
                                SYNTH_DISPLAY_HEIGHT * SIM_SCALE,
                                SDL_WINDOW_SHOWN);
    if (s_window == NULL) {
        HAL_LOG_ERROR(TAG, "SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return SYS_ERR_IO;
    }

    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED);
    if (s_renderer == NULL) {
        /* Fall back to software rendering; headless CI hosts often
         * have no accelerated driver. */
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (s_renderer == NULL) {
        HAL_LOG_ERROR(TAG, "SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = NULL;
        SDL_Quit();
        return SYS_ERR_IO;
    }

    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  SYNTH_DISPLAY_WIDTH, SYNTH_DISPLAY_HEIGHT);
    if (s_texture == NULL) {
        HAL_LOG_ERROR(TAG, "SDL_CreateTexture failed: %s", SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        s_renderer = NULL;
        s_window = NULL;
        SDL_Quit();
        return SYS_ERR_IO;
    }

    s_framebuffer = calloc((size_t)SYNTH_DISPLAY_WIDTH * SYNTH_DISPLAY_HEIGHT,
                           sizeof(uint16_t));
    if (s_framebuffer == NULL) {
        SDL_DestroyTexture(s_texture);
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        s_texture  = NULL;
        s_renderer = NULL;
        s_window   = NULL;
        SDL_Quit();
        return SYS_ERR_NO_MEM;
    }

    s_backlight_pct = SYNTH_DISPLAY_BACKLIGHT_DEFAULT_PCT;
    s_initialised   = true;

    HAL_LOG_INFO(TAG, "display HAL ready: %dx%d (x%d window)",
                 SYNTH_DISPLAY_WIDTH, SYNTH_DISPLAY_HEIGHT, SIM_SCALE);
    return SYS_OK;
}

sys_status_t hal_display_deinit(void)
{
    if (!s_initialised) {
        return SYS_OK;
    }
    free(s_framebuffer);
    s_framebuffer = NULL;
    if (s_texture != NULL)  { SDL_DestroyTexture(s_texture);  s_texture = NULL; }
    if (s_renderer != NULL) { SDL_DestroyRenderer(s_renderer); s_renderer = NULL; }
    if (s_window != NULL)   { SDL_DestroyWindow(s_window);    s_window = NULL; }
    SDL_Quit();

    s_initialised   = false;
    s_backlight_pct = 0;
    return SYS_OK;
}

int32_t hal_display_width(void)
{
    return SYNTH_DISPLAY_WIDTH;
}

int32_t hal_display_height(void)
{
    return SYNTH_DISPLAY_HEIGHT;
}

uint16_t *hal_display_framebuffer(void)
{
    return s_initialised ? s_framebuffer : NULL;
}


/* ─── Submit ────────────────────────────────────────────────────── */

/* Expand a region of the framebuffer into the 32-bit scratch buffer. */
static void expand_region(const hal_rect_t *r, uint32_t *dst)
{
    for (int32_t row = 0; row < r->h; row++) {
        const uint16_t *src = &s_framebuffer[(size_t)(r->y + row) * SYNTH_DISPLAY_WIDTH + r->x];
        uint32_t *out = &dst[(size_t)row * r->w];
        for (int32_t col = 0; col < r->w; col++) {
            out[col] = rgb565_to_argb8888(src[col]);
        }
    }
}

sys_status_t hal_display_submit_dirty(const hal_rect_t *rect)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }

    hal_rect_t r;
    if (rect != NULL) {
        if (rect->w <= 0 || rect->h <= 0) {
            return SYS_ERR_INVALID_ARG;
        }
        /* Clip to the panel. */
        int32_t x0 = rect->x < 0 ? 0 : rect->x;
        int32_t y0 = rect->y < 0 ? 0 : rect->y;
        int32_t x1 = rect->x + rect->w;
        int32_t y1 = rect->y + rect->h;
        if (x1 > SYNTH_DISPLAY_WIDTH)  { x1 = SYNTH_DISPLAY_WIDTH; }
        if (y1 > SYNTH_DISPLAY_HEIGHT) { y1 = SYNTH_DISPLAY_HEIGHT; }
        if (x1 <= x0 || y1 <= y0) {
            return SYS_OK;            /* entirely off-screen */
        }
        r.x = x0;
        r.y = y0;
        r.w = x1 - x0;
        r.h = y1 - y0;
    } else {
        r.x = 0;
        r.y = 0;
        r.w = SYNTH_DISPLAY_WIDTH;
        r.h = SYNTH_DISPLAY_HEIGHT;
    }

    size_t px = (size_t)r.w * (size_t)r.h;
    uint32_t *packed = malloc(px * sizeof(uint32_t));
    if (packed == NULL) {
        return SYS_ERR_NO_MEM;
    }
    expand_region(&r, packed);

    SDL_Rect dst_rect = { r.x, r.y, r.w, r.h };
    SDL_UpdateTexture(s_texture, &dst_rect, packed, r.w * (int)sizeof(uint32_t));

    free(packed);

    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);

    return SYS_OK;
}

sys_status_t hal_display_clear(uint16_t rgb565)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    size_t px = (size_t)SYNTH_DISPLAY_WIDTH * SYNTH_DISPLAY_HEIGHT;
    for (size_t i = 0; i < px; i++) {
        s_framebuffer[i] = rgb565;
    }
    return SYS_OK;
}

sys_status_t hal_display_backlight_set(uint8_t percent)
{
    if (percent > 100) {
        return SYS_ERR_INVALID_ARG;
    }
    s_backlight_pct = percent;
    /* The simulator has no backlight; the value is recorded so the
     * application can read back what it set. */
    return SYS_OK;
}

uint8_t hal_display_backlight_get(void)
{
    return s_backlight_pct;
}

#endif /* BOARD_SIMULATOR */
