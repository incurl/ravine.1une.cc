/*
 * hal_display.h — colour TFT display abstraction.
 *
 * The display HAL owns exactly one thing: an RGB565 pixel buffer plus
 * a way to push a region of it to the panel. There are deliberately no
 * e-ink concepts here — no global/partial refresh modes, no waveform
 * tables, no grayscale control, no "slow but low power" update path.
 * A colour TFT simply redraws pixels.
 *
 * Layering: this header includes no ESP-IDF and no SDL2 headers, so it
 * is includable from every layer of both targets.
 *
 * Coordinate system (identical on both targets):
 *   - Origin (0,0) is the top-left pixel.
 *   - x grows right, y grows down.
 *   - The visible area is SYNTH_DISPLAY_WIDTH x SYNTH_DISPLAY_HEIGHT.
 *   - Rectangles are half-open: [x, x+w) x [y, y+h).
 *
 * Colour format: RGB565, packed little-endian in a uint16_t. A pixel
 * value of 0xF800 is pure red, 0x07E0 pure green, 0x001F pure blue,
 * 0xFFFF white, 0x0000 black.
 *
 * Ownership:
 *   The framebuffer belongs to the backend and stays valid until the
 *   next hal_display_init(). Callers must not free or reallocate it.
 *
 * Threading:
 *   The framebuffer is *not* synchronised. Callers draw into it and
 *   then submit; a backend may read the buffer during the submit. On
 *   the firmware target, draw + submit must both happen on the UI
 *   task, or the caller must provide its own exclusion.
 */
#ifndef SYNTH_HAL_DISPLAY_H
#define SYNTH_HAL_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** An axis-aligned rectangle in pixel coordinates, half-open. */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} hal_rect_t;

/**
 * Initialise the display backend.
 *
 * On the firmware this brings up the panel, the SPI bus and the
 * framebuffer. On the simulator it opens the SDL window and creates the
 * texture.
 *
 * NOTE (transition period): the legacy firmware driver main/ui/display.c
 * also initialises the panel. Exactly one of the two must own the
 * hardware. If the legacy driver already ran, this call returns
 * SYS_ERR_BUSY rather than aborting on a double SPI-bus init.
 *
 * Returns SYS_OK, SYS_ERR_BUSY, SYS_ERR_NO_MEM, or SYS_ERR_IO.
 */
sys_status_t hal_display_init(void);

/**
 * Release the display backend. Safe to call when not initialised.
 */
sys_status_t hal_display_deinit(void);

/** Panel width in pixels. Never negative; 0 before init. */
int32_t hal_display_width(void);

/** Panel height in pixels. Never negative; 0 before init. */
int32_t hal_display_height(void);

/**
 * The RGB565 framebuffer, or NULL if the display is not initialised.
 *
 * Length is SYNTH_DISPLAY_FB_BYTES. The buffer is owned by the backend;
 * do not free it. May be NULL before init.
 */
uint16_t *hal_display_framebuffer(void);

/**
 * Push a region of the framebuffer to the panel.
 *
 * @param rect Region to push, in half-open pixel coordinates. NULL
 *             means the whole screen. A rect that is entirely outside
 *             the panel is a no-op and returns SYS_OK.
 *
 * The pixels read are exactly the framebuffer contents at the given
 * coordinates — the caller is responsible for having drawn them.
 *
 * Transport note: the ESP32 panel driver in use (ILI9341) has no
 * stride-aware blit, so the firmware backend packs the requested rows
 * into a contiguous scratch buffer before pushing them. The pixel
 * result is identical; only the wire traffic differs.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_INVALID_STATE, or
 * SYS_ERR_IO.
 */
sys_status_t hal_display_submit_dirty(const hal_rect_t *rect);

/**
 * Fill the entire framebuffer with one colour. Does not push to the
 * panel; call hal_display_submit_dirty(NULL) afterwards.
 */
sys_status_t hal_display_clear(uint16_t rgb565);

/**
 * Set the backlight brightness.
 *
 * @param percent 0 (off) .. 100 (full).
 *
 * The simulator treats this as a no-op beyond reporting the value.
 *
 * Returns SYS_OK or SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_display_backlight_set(uint8_t percent);

/**
 * Current backlight brightness, percent (0..100). 0 before init.
 */
uint8_t hal_display_backlight_get(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_HAL_DISPLAY_H */
