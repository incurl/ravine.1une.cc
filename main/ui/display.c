/*
 * display.c — 2.4" ILI9341 (SPI) using esp_lcd + custom 2D draw API on a
 * PSRAM framebuffer.
 */
#include "display.h"
#include "config.h"
#include "sequencer/sequencer.h"
#include "sequencer/pattern.h"
#include "audio/amy_bridge.h"
#include "sketch/sketch_picker.h"
#include "system/clock.h"
#include "system/power_mgmt.h"
#include "ui/input.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "disp";

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io = NULL;
static uint16_t *s_fb = NULL;

/* 5x7 font (digits, A-Z, basic punctuation). */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
    {0x7F,0x20,0x18,0x20,0x7F}, {0x63,0x14,0x08,0x14,0x63},
    {0x03,0x04,0x78,0x04,0x03}, {0x61,0x51,0x49,0x45,0x43},
};

static inline uint8_t font_glyph(char c, uint8_t *out5)
{
    if (c >= '0' && c <= '9') { memcpy(out5, font5x7[1 + (c-'0')], 5); return 1; }
    if (c >= 'A' && c <= 'Z') { memcpy(out5, font5x7[12 + (c-'A')], 5); return 1; }
    if (c >= 'a' && c <= 'z') { memcpy(out5, font5x7[12 + (c-'a')], 5); return 1; }
    switch (c) {
    case ' ': memcpy(out5, font5x7[0], 5);  return 1;
    case ':': memcpy(out5, font5x7[11], 5); return 1;
    case '.': memcpy(out5, font5x7[1], 5);  return 1;
    }
    memset(out5, 0, 5);
    return 0;
}

static inline void fb_set_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    s_fb[y * TFT_WIDTH + x] = color;
}

static void fb_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            fb_set_pixel(xx, yy, color);
}

static void fb_draw_text(int x, int y, const char *s, uint16_t color)
{
    while (*s) {
        uint8_t g[5];
        font_glyph(*s++, g);
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) fb_set_pixel(x + col, y + row, color);
            }
        }
        x += 6;
    }
}

static void fb_flush(void)
{
    if (!s_panel || !s_fb) return;
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, TFT_WIDTH, TFT_HEIGHT, s_fb);
}

/* ─── HAL accessors ───────────────────────────────────────────────
 *
 * The HAL display backend drives the panel and framebuffer that this
 * module already owns, rather than creating a second one. These two
 * accessors are the entire coupling point between the legacy driver
 * and the HAL; nothing else about display.c is changed.
 */
uint16_t *display_fb(void)
{
    return s_fb;
}

esp_lcd_panel_handle_t display_panel(void)
{
    return s_panel;
}

static void backlight_init(void)
{
    ledc_timer_config_t tcfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num       = TFT_BL_PWM_TIMER,
        .freq_hz         = TFT_BL_PWM_HZ,
    };
    ledc_timer_config(&tcfg);
    ledc_channel_config_t ccfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = TFT_BL_PWM_CHANNEL,
        .timer_sel  = TFT_BL_PWM_TIMER,
        .gpio_num   = TFT_BL_GPIO,
        .duty       = (TFT_BL_DEFAULT_DUTY * 255) / 100,
    };
    ledc_channel_config(&ccfg);
}

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "Initializing 2.4\" TFT (ILI9341) on SPI");

    s_fb = heap_caps_malloc(TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t),
                            MALLOC_CAP_SPIRAM);
    if (!s_fb) {
        ESP_LOGE(TAG, "Framebuffer alloc failed");
        return ESP_ERR_NO_MEM;
    }
    memset(s_fb, 0, TFT_WIDTH * TFT_HEIGHT * sizeof(uint16_t));

    backlight_init();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = TFT_MOSI_GPIO,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = TFT_SCK_GPIO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = TFT_DC_GPIO,
        .cs_gpio_num       = TFT_CS_GPIO,
        .pclk_hz           = TFT_SPI_CLK_HZ,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)TFT_SPI_HOST, &io_cfg, &s_io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = TFT_RST_GPIO,
        .data_endian     = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(s_io, &panel_cfg, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    ESP_LOGI(TAG, "TFT ready.");
    return ESP_OK;
}

void display_show_boot_screen(void)
{
    fb_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, 0x0000);
    fb_draw_text(60, 140, "PO 33 K O",  0xFFFF);
    fb_draw_text(40, 160, "ESP32 S3 FW", 0x07E0);
    fb_flush();
}

static char s_buf[64];

/* Top status bar text color: blue (ILI9341 RGB565 BGR = 0x001F). */
#define COLOR_STATUS_BAR_BG  0x001F
#define COLOR_STATUS_BAR_FG  0xFFFF
#define COLOR_BG              0x0000
#define COLOR_TEXT            0xFFFF
#define COLOR_TEXT_DIM        0xC618   /* grey */
#define COLOR_HIGHLIGHT_BG    0xF800   /* red */
#define COLOR_SLOT_EMPTY      0x4208   /* dark grey */

/* PO-33 F-038 panel: shows the per-slot and per-pattern active-state
 * the same way the real PO-33 shows it via its lit step buttons.
 *
 *  - "lit"   (slot/pattern has data)         -> white filled square
 *  - "unlit" (slot/pattern is empty)         -> dim grey filled square
 *  - "flashing / currently selected"         -> red filled square with
 *                                               a yellow border (the
 *                                               "active" highlight color
 *                                               is the same red used by
 *                                               the step cursor above)
 *
 * 16 dots per row, 6x6 px each, 4-px gap. Total row width:
 *   16 * 6 + 15 * 4 = 156 px. Centered on a 240-px screen: x = 42.
 *
 * Pattern fill = "at least one step has a non-FF slot_id". Cheap O(N*M)
 * scan; runs once per display_tick (~30 Hz) on 16x16 = 256 step
 * checks, negligible. */
static void render_active_state_panel(void)
{
    static const int dot_w   = 6;
    static const int dot_h   = 6;
    static const int gap     = 4;
    static const int row_w   = SLOT_COUNT * dot_w + (SLOT_COUNT - 1) * gap;
    static const int row_x0  = (TFT_WIDTH - row_w) / 2;
    static const int row_y0  = 130;   /* SND row */
    static const int row_y1  = 158;   /* PAT row */

    fb_draw_text(4, row_y0 - 2, "SND", COLOR_TEXT_DIM);
    fb_draw_text(4, row_y1 - 2, "PAT", COLOR_TEXT_DIM);

    const uint8_t active_slot = sequencer_get_active_slot();
    const uint8_t active_pat  = sequencer_get_current_pattern();

    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        int x = row_x0 + i * (dot_w + gap);
        bool filled = amy_bridge_slot_has_sample(i);
        bool active = (i == active_slot);
        uint16_t fill_color =
            active ? COLOR_HIGHLIGHT_BG :
            filled ? COLOR_TEXT :
                     COLOR_SLOT_EMPTY;
        fb_fill_rect(x, row_y0, dot_w, dot_h, fill_color);
        if (active) {
            /* Yellow 1-px border so the "currently selected" indicator
             * reads as flashing even when the dot is otherwise the
             * same red as the step cursor. Inset by 1 px so the border
             * sits on the outside of the fill. */
            fb_fill_rect(x - 1,             row_y0 - 1,            dot_w + 2, 1, 0xFFE0);
            fb_fill_rect(x - 1,             row_y0 + dot_h,        dot_w + 2, 1, 0xFFE0);
            fb_fill_rect(x - 1,             row_y0,                1, dot_h, 0xFFE0);
            fb_fill_rect(x + dot_w,         row_y0,                1, dot_h, 0xFFE0);
        }
    }

    for (uint8_t p = 0; p < PATTERN_COUNT; p++) {
        int x = row_x0 + p * (dot_w + gap);
        bool filled = false;
        for (uint8_t s = 0; s < STEPS_PER_PATTERN; s++) {
            if (g_patterns[p].steps[s].slot_id != 0xFF) {
                filled = true;
                break;
            }
        }
        bool active = (p == active_pat);
        uint16_t fill_color =
            active ? COLOR_HIGHLIGHT_BG :
            filled ? COLOR_TEXT :
                     COLOR_SLOT_EMPTY;
        fb_fill_rect(x, row_y1, dot_w, dot_h, fill_color);
        if (active) {
            fb_fill_rect(x - 1,             row_y1 - 1,            dot_w + 2, 1, 0xFFE0);
            fb_fill_rect(x - 1,             row_y1 + dot_h,        dot_w + 2, 1, 0xFFE0);
            fb_fill_rect(x - 1,             row_y1,                1, dot_h, 0xFFE0);
            fb_fill_rect(x + dot_w,         row_y1,                1, dot_h, 0xFFE0);
        }
    }
}

static void render_main_screen(void)
{
    fb_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BG);

    /* top status bar */
    fb_fill_rect(0, 0, TFT_WIDTH, 18, COLOR_STATUS_BAR_BG);

    snprintf(s_buf, sizeof(s_buf), "PAT%u", sequencer_get_current_pattern());
    fb_draw_text(4, 4, s_buf, COLOR_STATUS_BAR_FG);

    /* Write-mode indicator (F-009). Replaces the BPM slot in the top
     * bar while write mode is active; the BPM is also accessible via
     * the existing "BPM" cheat-sheet row. Yellow on blue for
     * visibility against the white PAT/BPM labels. */
    if (write_mode_is_active()) {
        fb_draw_text(40, 4, "WRITE", 0xFFE0);  /* yellow */
    }

    snprintf(s_buf, sizeof(s_buf), "BPM %u", sequencer_get_bpm());
    fb_draw_text(80, 4, s_buf, COLOR_STATUS_BAR_FG);

    /* Tweak-mode indicator (F-015). Rendered only when write mode is
     * inactive -- write mode occupies the same status-bar row with a
     * yellow "WRITE" label, and they shouldn't visually fight.
     * Cyan (0x07FF) on the blue status bar distinguishes the label
     * from the white PAT/BPM/clock text without crowding them. */
    if (!write_mode_is_active()) {
        tweak_mode_t tm = tweak_get_mode();
        const char *label =
            (tm == TWEAK_TONE)   ? "TONE"   :
            (tm == TWEAK_FILTER) ? "FILTER" :
            (tm == TWEAK_TRIM)   ? "TRIM"   : "?";
        fb_draw_text(130, 4, label, 0x07FF);  /* cyan */
    }

    /* Battery indicator (F-035) at x=180. Replaces the clock on the
     * main screen -- a portable device needs the battery visible;
     * the clock still lives in the picker screen (where the user can
     * also see it via the alarm UI). 5-bar glyph + percentage label.
     *
     *   ._____._____
     *   |        .  |  <- battery outline with cap on the right
     *   | ||||   85% |  <- 4 internal bars filled by percent
     *   |_____._____|
     *
     * Outline: 24 wide x 10 tall starting at x=180, y=2. Cap: 2x4
     * rectangle to the right. Bars: each 3 wide x 6 tall, gap 1 px,
     * 4 bars total. Percentage label: at x=210, y=4. */
    static const int batt_x = 180;
    static const int batt_y = 3;
    static const int batt_w = 24;
    static const int batt_h = 11;
    /* Outline (top + bottom + left + right) */
    fb_fill_rect(batt_x,             batt_y,             batt_w, 1, COLOR_STATUS_BAR_FG);
    fb_fill_rect(batt_x,             batt_y + batt_h - 1, batt_w, 1, COLOR_STATUS_BAR_FG);
    fb_fill_rect(batt_x,             batt_y,             1, batt_h, COLOR_STATUS_BAR_FG);
    fb_fill_rect(batt_x + batt_w - 1, batt_y,            1, batt_h, COLOR_STATUS_BAR_FG);
    /* Cap on the right */
    fb_fill_rect(batt_x + batt_w,     batt_y + 3,        2, 5, COLOR_STATUS_BAR_FG);

    uint8_t pct = power_mgmt_battery_get_percent();
    if (pct == 0xFF) {
        /* ADC read failed -- render "ERR" instead of a misleading %. */
        fb_draw_text(batt_x + batt_w + 4, batt_y + 2, "ERR", COLOR_TEXT_DIM);
    } else {
        /* 4 internal bars: filled when pct >= (i+1) * 25 (so 25/50/75/100). */
        int bars = (pct >= 100) ? 4 :
                   (pct >=  75) ? 3 :
                   (pct >=  50) ? 2 :
                   (pct >=  25) ? 1 : 0;
        for (int i = 0; i < bars; i++) {
            int bx = batt_x + 2 + i * 5;   /* 3 px bar + 2 px gap */
            int by = batt_y + 3;           /* inside the outline */
            fb_fill_rect(bx, by, 3, 5, COLOR_STATUS_BAR_FG);
        }
        /* Percentage label to the right of the cap. */
        snprintf(s_buf, sizeof(s_buf), "%u", (unsigned)pct);
        fb_draw_text(batt_x + batt_w + 4, batt_y + 2, s_buf, COLOR_STATUS_BAR_FG);
    }

    /* 16 step grid */
    int step = sequencer_get_current_step();
    for (int i = 0; i < STEPS_PER_PATTERN; i++) {
        int x = 8 + i * 14;
        int y = 60;
        if (i == step) fb_fill_rect(x - 1, y - 1, 12, 12, COLOR_HIGHLIGHT_BG);
        fb_fill_rect(x, y, 10, 10, COLOR_TEXT);
    }

    snprintf(s_buf, sizeof(s_buf), "STEP %02u", step);
    fb_draw_text(8, 100, s_buf, 0x07FF);

    /* F-038: per-slot and per-pattern active-state panel. */
    render_active_state_panel();
}

/* Sketch picker screen. Renders a top bar with "SKETCHES N / 16" + a
 * 4x4 grid (16 slots, 4 rows x 4 cols). The currently-highlighted
 * slot (from sketch_picker_get_highlight) is filled with a red
 * background and the ID rendered in inverse. Empty slots (beyond
 * sketch_picker_get_count) are rendered in dim grey.
 *
 * Layout (240x320):
 *   y=0..18   status bar (same blue as main)
 *   y=24      "SKETCHES  N / 16"        (white text)
 *   y=24      "Click=load WRITE=exit"   (grey text, right side)
 *   y=40..300 16 slots in a 4x4 grid, each row 64px tall
 *
 * The matrix rows mirror the matrix: slot index 0..3 in row 0, 4..7
 * in row 1, etc. This means the picker matrix maps naturally to the
 * physical 4x4 button matrix: pressing STEP 1 selects the first row
 * leftmost slot, pressing STEP 5 selects the second row leftmost
 * slot, etc. */
static void render_picker_screen(void)
{
    fb_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BG);

    /* top status bar (same look as main) */
    fb_fill_rect(0, 0, TFT_WIDTH, 18, COLOR_STATUS_BAR_BG);
    fb_draw_text(4, 4, "PICKER", COLOR_STATUS_BAR_FG);
    fb_draw_text(80, 4, "TURN=scroll", COLOR_STATUS_BAR_FG);

    /* "SKETCHES N / 16" header */
    uint8_t count = sketch_picker_get_count();
    snprintf(s_buf, sizeof(s_buf), "SKETCHES %u / 16", (unsigned)count);
    fb_draw_text(4, 24, s_buf, COLOR_TEXT);

    /* Hint text */
    fb_draw_text(120, 24, "click=load", COLOR_TEXT_DIM);

    /* 4x4 grid of slots. Each cell 56x64 px, 4-px gap. */
    const int cell_w = 56;
    const int cell_h = 64;
    const int gap = 4;
    const int grid_x = (TFT_WIDTH - (4 * cell_w + 3 * gap)) / 2;  /* centred */
    const int grid_y = 40;

    uint8_t highlight = sketch_picker_get_highlight();

    for (uint8_t i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        int x = grid_x + col * (cell_w + gap);
        int y = grid_y + row * (cell_h + gap);

        if (i == highlight) {
            fb_fill_rect(x, y, cell_w, cell_h, COLOR_HIGHLIGHT_BG);
            fb_fill_rect(x + 2, y + 2, cell_w - 4, cell_h - 4, COLOR_BG);
        } else {
            fb_fill_rect(x, y, cell_w, cell_h, COLOR_SLOT_EMPTY);
        }

        char id[8] = {0};
        if (i < count) {
            sketch_picker_get_id(i, id);
        } else {
            strcpy(id, "----");
        }
        /* ID at top of cell, white on highlighted, dim on others. */
        uint16_t text_color = (i == highlight) ? COLOR_TEXT_DIM : COLOR_TEXT;
        fb_draw_text(x + 6, y + 6, id, text_color);

        /* Sketch index at bottom of cell. */
        snprintf(s_buf, sizeof(s_buf), "%u", (unsigned)(i + 1));
        fb_draw_text(x + 22, y + 22, s_buf, text_color);

        /* Star marker for the highlighted slot. */
        if (i == highlight) {
            fb_draw_text(x + 22, y + 50, "*", 0xFFE0);  /* yellow */
        }
    }

    /* Footer with rotate/click hint. */
    fb_draw_text(4, 308, "WRITE=exit", COLOR_TEXT_DIM);

    /* F-033 still applies -- but the clock was bumped off the main
     * screen by the F-035 battery indicator. Show it in the picker
     * footer so the user can still read HH:MM when picking a
     * sketch. Right-aligned at x=190; the "tap step=load" hint that
     * used to live here was off-screen at x>=170 anyway. */
    uint16_t h, m;
    clock_get_hhmm(&h, &m);
    snprintf(s_buf, sizeof(s_buf), "%02u:%02u", (unsigned)h, (unsigned)m);
    fb_draw_text(190, 308, s_buf, COLOR_TEXT_DIM);
}

/* F-022 volume-level overlay state.
 *
 * display_show_volume_level(level) arms the overlay with a 1200 ms
 * deadline; display_tick() renders render_volume_overlay() while the
 * deadline is in the future and render_main_screen() otherwise. The
 * overlay auto-clears without needing a timer task -- every
 * display_tick() call also advances the deadline check, and once
 * expired we just stop rendering it. */
static bool     s_volume_overlay_active = false;
static uint8_t  s_volume_level = 0;
static int64_t  s_volume_deadline_us = 0;     /* esp_timer_get_time() value */

#define VOLUME_OVERLAY_MS 1200

void display_show_volume_level(uint8_t level)
{
    if (level > 5) level = 5;
    s_volume_level = level;
    s_volume_deadline_us = esp_timer_get_time() + (int64_t)(VOLUME_OVERLAY_MS) * 1000;
    s_volume_overlay_active = true;
}

static void render_volume_overlay(void)
{
    /* Has the deadline passed since the last arm? Auto-clear. */
    if (esp_timer_get_time() >= s_volume_deadline_us) {
        s_volume_overlay_active = false;
        render_main_screen();
        return;
    }

    /* Clear the framebuffer (don't preserve the main screen beneath
     * -- the overlay is meant to *replace* the main view for its
     * short lifetime, like the PO-33's lit-buttons display). */
    fb_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BG);

    /* Centred 5-bar graphic at y=120. Each bar is 24x40 px with a
     * 6-px gap. Total width: 5*24 + 4*6 = 144 px, centred on the
     * 240-px screen: x_start = 48. */
    static const int bar_w     = 24;
    static const int bar_h     = 40;
    static const int bar_gap   = 6;
    static const int bar_count = 5;
    static const int total_w   = bar_count * bar_w + (bar_count - 1) * bar_gap;
    static const int x_start   = (TFT_WIDTH - total_w) / 2;
    static const int y_top     = 120;

    /* Header label */
    fb_draw_text(4, y_top - 18, "VOLUME", COLOR_TEXT_DIM);

    /* "LEVEL 3/5" or similar on the right */
    snprintf(s_buf, sizeof(s_buf), "%u/%u", (unsigned)s_volume_level, (unsigned)bar_count);
    fb_draw_text(204, y_top - 18, s_buf, COLOR_TEXT_DIM);

    for (uint8_t i = 0; i < bar_count; i++) {
        int x = x_start + i * (bar_w + bar_gap);
        bool lit = (i < s_volume_level);
        /* Background bar in dim grey (always visible) */
        fb_fill_rect(x, y_top, bar_w, bar_h, COLOR_SLOT_EMPTY);
        if (lit) {
            /* Filled portion: green for bars 0..3, yellow for the top
             * bar (level 5 / max) to suggest a "level meter" reaching
             * its peak. The choice of yellow only at the top also
             * gives the user a visual "you have headroom" cue. */
            uint16_t color = (i == bar_count - 1) ? 0xFFE0 : 0x07E0;
            fb_fill_rect(x, y_top, bar_w, bar_h, color);
        }
        /* 1-px white outline so bars stay visible against any
         * neighbouring background. */
        fb_fill_rect(x,                 y_top,              bar_w, 1,             COLOR_TEXT);
        fb_fill_rect(x,                 y_top + bar_h - 1,  bar_w, 1,             COLOR_TEXT);
        fb_fill_rect(x,                 y_top,              1, bar_h,             COLOR_TEXT);
        fb_fill_rect(x + bar_w - 1,     y_top,              1, bar_h,             COLOR_TEXT);
    }
}

void display_tick(void)
{
    if (sketch_picker_is_active()) {
        render_picker_screen();
    } else if (s_volume_overlay_active) {
        render_volume_overlay();
    } else {
        render_main_screen();
    }
    fb_flush();
}