/*
 * hal_display_esp32.c — ESP32-S3 display backend.
 *
 * Wraps the *existing* legacy driver (main/ui/display.c) rather than
 * creating a second panel. The legacy driver already owns:
 *
 *   - the SPI2 bus and the esp_lcd panel IO,
 *   - the ILI9341 panel instance,
 *   - a PSRAM RGB565 framebuffer of TFT_WIDTH x TFT_HEIGHT pixels.
 *
 * The HAL reaches those through the two accessors added to display.c
 * (display_fb / display_panel). Nothing else in the legacy driver is
 * modified.
 *
 * Dirty-rectangle transport
 * -------------------------
 * The ILI9341 panel driver in this project (espressif/esp_lcd_ili9341)
 * implements only esp_lcd_panel_draw_bitmap(), which pushes
 * (x_end-x_start) * (y_end-y_start) * bytes as ONE contiguous blob.
 * It does not implement esp_lcd_panel_draw_bitmap_2d(), so there is no
 * stride-aware blit.
 *
 * A sub-rectangle of the framebuffer is therefore not contiguous in
 * memory: its rows are separated by the full screen width. To push a
 * dirty rectangle we pack the requested rows into a contiguous scratch
 * buffer and hand that to the panel.
 *
 * The scratch buffer is allocated once at init (PSRAM, sized for the
 * largest possible dirty rectangle) and reused; the display path
 * performs no heap allocation afterwards.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_ESP32S3)

#include "hal/hal_display.h"

#include "boards/synth_board.h"
#include "hal/hal_log.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

#include "main/config.h"
#include "main/ui/display.h"

/* The board header must agree with the firmware's own pin map. These
 * assertions are the mechanism that stops the two from drifting. */
_Static_assert(SYNTH_DISPLAY_WIDTH  == TFT_WIDTH,  "board width != TFT_WIDTH");
_Static_assert(SYNTH_DISPLAY_HEIGHT == TFT_HEIGHT, "board height != TFT_HEIGHT");
_Static_assert(SYNTH_DISPLAY_SPI_HOST == SPI2_HOST, "board SPI host mismatch");
_Static_assert(SYNTH_DISPLAY_PIN_MOSI == TFT_MOSI_GPIO, "MOSI mismatch");
_Static_assert(SYNTH_DISPLAY_PIN_SCK  == TFT_SCK_GPIO,  "SCK mismatch");
_Static_assert(SYNTH_DISPLAY_PIN_CS   == TFT_CS_GPIO,   "CS mismatch");
_Static_assert(SYNTH_DISPLAY_PIN_DC   == TFT_DC_GPIO,   "DC mismatch");
_Static_assert(SYNTH_DISPLAY_PIN_RST  == TFT_RST_GPIO,  "RST mismatch");
_Static_assert(SYNTH_DISPLAY_PIN_BL   == TFT_BL_GPIO,   "BL mismatch");
_Static_assert(SYNTH_DISPLAY_BL_PWM_TIMER  == TFT_BL_PWM_TIMER,  "BL timer mismatch");
_Static_assert(SYNTH_DISPLAY_BL_PWM_CHANNEL == TFT_BL_PWM_CHANNEL, "BL channel mismatch");

/* Backlight duty is configured in whole percent; the LEDC channel
 * takes an 8-bit duty. */
#define BACKLIGHT_DUTY_MAX 255

static const char *TAG = "hal_disp";

static bool              s_initialised;
static uint16_t         *s_scratch;      /* contiguous dirty-rect staging */
static size_t            s_scratch_px;   /* capacity in pixels */
static SemaphoreHandle_t s_lock;         /* guards framebuffer + scratch */
static uint8_t           s_backlight_pct;

/* ─── Backlight ─────────────────────────────────────────────────── */

static void backlight_apply(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    uint32_t duty = ((uint32_t)percent * BACKLIGHT_DUTY_MAX) / 100u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, TFT_BL_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, TFT_BL_PWM_CHANNEL);
    s_backlight_pct = percent;
}

/* ─── Init ──────────────────────────────────────────────────────── */

sys_status_t hal_display_init(void)
{
    if (s_initialised) {
        return SYS_OK;
    }

    /* The legacy driver owns the panel. If it has not been brought up
     * yet, the HAL cannot present a framebuffer of its own without
     * fighting it for the SPI bus. */
    if (display_panel() == NULL || display_fb() == NULL) {
        HAL_LOG_ERROR(TAG, "legacy display driver is not initialised");
        return SYS_ERR_INVALID_STATE;
    }

    /* Staging buffer for dirty rectangles: worst case is a full-screen
     * rectangle, i.e. the whole framebuffer. */
    s_scratch_px = (size_t)SYNTH_DISPLAY_WIDTH * SYNTH_DISPLAY_HEIGHT;
    s_scratch = heap_caps_malloc(s_scratch_px * sizeof(uint16_t),
                                 MALLOC_CAP_SPIRAM);
    if (s_scratch == NULL) {
        HAL_LOG_ERROR(TAG, "scratch buffer alloc failed (%u px)",
                      (unsigned)s_scratch_px);
        return SYS_ERR_NO_MEM;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        heap_caps_free(s_scratch);
        s_scratch = NULL;
        return SYS_ERR_NO_MEM;
    }

    backlight_apply(SYNTH_DISPLAY_BACKLIGHT_DEFAULT_PCT);

    s_initialised = true;
    HAL_LOG_INFO(TAG, "display HAL ready: %dx%d RGB565",
                 SYNTH_DISPLAY_WIDTH, SYNTH_DISPLAY_HEIGHT);
    return SYS_OK;
}

sys_status_t hal_display_deinit(void)
{
    if (!s_initialised) {
        return SYS_OK;
    }
    if (s_lock != NULL) {
        vSemaphoreDelete(s_lock);
        s_lock = NULL;
    }
    if (s_scratch != NULL) {
        heap_caps_free(s_scratch);
        s_scratch = NULL;
    }
    s_scratch_px    = 0;
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
    if (!s_initialised) {
        return NULL;
    }
    return display_fb();
}

/* ─── Submit ────────────────────────────────────────────────────── */

/* Clip a rectangle to the panel, returning false when nothing is left. */
static bool clip_rect(const hal_rect_t *in, hal_rect_t *out)
{
    int32_t x0 = in->x;
    int32_t y0 = in->y;
    int32_t x1 = in->x + in->w;
    int32_t y1 = in->y + in->h;

    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > SYNTH_DISPLAY_WIDTH)  { x1 = SYNTH_DISPLAY_WIDTH; }
    if (y1 > SYNTH_DISPLAY_HEIGHT) { y1 = SYNTH_DISPLAY_HEIGHT; }

    if (x1 <= x0 || y1 <= y0) {
        return false;
    }

    out->x = x0;
    out->y = y0;
    out->w = x1 - x0;
    out->h = y1 - y0;
    return true;
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
        if (!clip_rect(rect, &r)) {
            /* Entirely off-screen: nothing to do. */
            return SYS_OK;
        }
    } else {
        r.x = 0;
        r.y = 0;
        r.w = SYNTH_DISPLAY_WIDTH;
        r.h = SYNTH_DISPLAY_HEIGHT;
    }

    const uint16_t *fb = display_fb();
    esp_lcd_panel_handle_t panel = display_panel();
    if (fb == NULL || panel == NULL) {
        return SYS_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_ERR_TIMEOUT;
    }

    esp_err_t err;

    if (r.w == SYNTH_DISPLAY_WIDTH) {
        /* Full-width: the rows are already contiguous, push directly. */
        err = esp_lcd_panel_draw_bitmap(panel, r.x, r.y,
                                         r.x + r.w, r.y + r.h,
                                         &fb[(size_t)r.y * SYNTH_DISPLAY_WIDTH + r.x]);
    } else {
        /* Narrower than the screen: pack the rows contiguously. */
        size_t need = (size_t)r.w * (size_t)r.h;
        if (need > s_scratch_px) {
            xSemaphoreGive(s_lock);
            return SYS_ERR_INVALID_ARG;
        }

        const uint16_t *src = &fb[(size_t)r.y * SYNTH_DISPLAY_WIDTH + r.x];
        for (int32_t row = 0; row < r.h; row++) {
            memcpy(&s_scratch[(size_t)row * r.w],
                   &src[(size_t)row * SYNTH_DISPLAY_WIDTH],
                   (size_t)r.w * sizeof(uint16_t));
        }

        err = esp_lcd_panel_draw_bitmap(panel, r.x, r.y,
                                         r.x + r.w, r.y + r.h,
                                         s_scratch);
    }

    xSemaphoreGive(s_lock);

    if (err != ESP_OK) {
        HAL_LOG_ERROR(TAG, "draw_bitmap failed: %s", esp_err_to_name(err));
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

sys_status_t hal_display_clear(uint16_t rgb565)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    uint16_t *fb = display_fb();
    if (fb == NULL) {
        return SYS_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return SYS_ERR_TIMEOUT;
    }
    size_t px = (size_t)SYNTH_DISPLAY_WIDTH * SYNTH_DISPLAY_HEIGHT;
    for (size_t i = 0; i < px; i++) {
        fb[i] = rgb565;
    }
    xSemaphoreGive(s_lock);
    return SYS_OK;
}

sys_status_t hal_display_backlight_set(uint8_t percent)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (percent > 100) {
        return SYS_ERR_INVALID_ARG;
    }
    backlight_apply(percent);
    return SYS_OK;
}

uint8_t hal_display_backlight_get(void)
{
    return s_backlight_pct;
}

#endif /* BOARD_ESP32S3 */
