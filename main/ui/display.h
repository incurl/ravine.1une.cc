/*
 * display.h — 2.4" ILI9341 TFT driver + custom 2D draw API.
 *
 * Two screens are supported:
 *   0  = main / sequencer view (default after boot)
 *   1  = sketch picker (entered via WRITE long-press; set
 *         automatically by sketch_picker_enter / _exit)
 *
 * The display module reads the picker's state via the read-only
 * getters in sketch_picker.h; it doesn't own the picker state
 * machine.
 */
#ifndef PO33_DISPLAY_H
#define PO33_DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_init(void);
void      display_show_boot_screen(void);
void      display_tick(void);

/* ─── HAL accessors ───────────────────────────────────────────────
 *
 * These expose the panel and framebuffer that display.c already owns,
 * so that the HAL display backend (src/backend_esp32/
 * hal_display_esp32.c) can drive the *existing* driver instead of
 * initialising a second panel on the same SPI bus.
 *
 * display_fb() returns the RGB565 framebuffer in PSRAM, or NULL if
 * display_init() has not run. The buffer is owned by display.c: do
 * not free or replace it.
 *
 * display_panel() returns the esp_lcd panel handle, or NULL before
 * display_init(). The handle is owned by display.c and is only valid
 * for the lifetime of the driver.
 */
uint16_t *display_fb(void);
esp_lcd_panel_handle_t display_panel(void);

/* F-022 display-half: briefly show the current volume level on the
 * TFT. The indicator persists for ~1.2 s and then fades back to the
 * main screen automatically. The PO-33 does this with lit step
 * buttons ("press bpm; numbers are lit until the current level"); on
 * the TFT we render a centred 5-bar graphic that fills up to the
 * current level.
 *
 * Safe to call from any context (audio task, button scan task, UART
 * handler); the implementation is idempotent and re-arming the
 * timer just resets the deadline. */
void      display_show_volume_level(uint8_t level);   /* 0..5 */

#ifdef __cplusplus
}
#endif

#endif
