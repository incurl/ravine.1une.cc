/*
 * main_sim.c — SDL2 host simulator entry point.
 *
 * Runs the Ravine Phoenix HAL against a desktop SDL2 window, so that
 * application logic can be developed and tested on a PC without the
 * device attached.
 *
 * What the simulator does:
 *   - Opens a 240x320 panel window (scaled 3x so the pixels show).
 *   - Maps the PC keyboard onto the device's keypad.
 *   - Maps the storage root onto ./sim_flash/.
 *   - Runs a single event loop that services input, draws a status
 *     screen, and pushes the framebuffer to the window.
 *
 * What it deliberately does NOT do:
 *   - It does not synthesise audio. The audio HAL is a silent stub;
 *     the real DSP is AMY, on the device.
 *   - It does not run the real sequencer or the legacy firmware's
 *     main.c. Those belong to the application layer, which is the next
 *     increment of the migration.
 *
 * Usage:
 *   ravine_sim              run the interactive simulator
 *   ravine_sim --selftest   run the non-interactive HAL self-test
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "synth/hal.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "sim";

/* ─── Colour helpers (RGB565) ───────────────────────────────────── */

#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

#define COLOR_BG        RGB565(12, 14, 20)
#define COLOR_PANEL     RGB565(28, 32, 44)
#define COLOR_ACCENT    RGB565(0, 200, 220)
#define COLOR_TEXT      RGB565(230, 235, 245)
#define COLOR_DIM       RGB565(120, 130, 150)
#define COLOR_OK        RGB565(80, 220, 120)
#define COLOR_WARN      RGB565(240, 180, 60)

/* ─── A small 5x7 font, enough for the status screen ─────────────── */

/* Each glyph is 5 columns of 7 bits, LSB = top row. */
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x01,0x01}, /* F */
    {0x3E,0x41,0x41,0x51,0x32}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
};

static void draw_pixel(uint16_t *fb, int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= SYNTH_DISPLAY_WIDTH || y >= SYNTH_DISPLAY_HEIGHT) {
        return;
    }
    fb[y * SYNTH_DISPLAY_WIDTH + x] = color;
}

static void fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color)
{
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            draw_pixel(fb, xx, yy, color);
        }
    }
}

static void draw_text(uint16_t *fb, int x, int y, const char *s, uint16_t color)
{
    while (*s != '\0') {
        char ch = *s++;
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        if (ch < ' ' || ch > '~') {
            x += 6;
            continue;
        }
        const uint8_t *glyph = font5x7[(int)ch - ' '];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1u << row)) {
                    draw_pixel(fb, x + col, y + row, color);
                }
            }
        }
        x += 6;
    }
}

/* ─── Screen ────────────────────────────────────────────────────── */

typedef struct {
    uint32_t presses;
    uint32_t releases;
    uint32_t long_presses;
    uint32_t holds;
    uint32_t knob_deltas;
    uint32_t encoder_deltas;
    char     last_key[8];
} sim_stats_t;

static void draw_screen(uint16_t *fb, const sim_stats_t *st, bool running)
{
    fill_rect(fb, 0, 0, SYNTH_DISPLAY_WIDTH, SYNTH_DISPLAY_HEIGHT, COLOR_BG);

    /* Header bar. */
    fill_rect(fb, 0, 0, SYNTH_DISPLAY_WIDTH, 24, COLOR_PANEL);
    draw_text(fb, 6, 8, "RAVINE PHOENIX", COLOR_ACCENT);
    draw_text(fb, 150, 8, "HAL SIM", COLOR_DIM);

    /* Status line. */
    fill_rect(fb, 6, 34, SYNTH_DISPLAY_WIDTH - 12, 22, COLOR_PANEL);
    draw_text(fb, 12, 41, running ? "RUNNING" : "STOPPED",
              running ? COLOR_OK : COLOR_WARN);

    /* Event counters. */
    int y = 68;
    draw_text(fb, 8, y, "INPUT EVENTS", COLOR_TEXT);      y += 12;
    draw_text(fb, 12, y, "PRESS", COLOR_DIM);
    draw_text(fb, 120, y, "RELEASE", COLOR_DIM);
    draw_text(fb, 210, y, "LONG", COLOR_DIM);              y += 10;
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "%u", st->presses);
        draw_text(fb, 12, y, buf, COLOR_TEXT);
        snprintf(buf, sizeof(buf), "%u", st->releases);
        draw_text(fb, 120, y, buf, COLOR_TEXT);
        snprintf(buf, sizeof(buf), "%u", st->long_presses);
        draw_text(fb, 210, y, buf, COLOR_TEXT);
        y += 10;
    }
    draw_text(fb, 12, y, "HOLD", COLOR_DIM);
    draw_text(fb, 120, y, "KNOB", COLOR_DIM);
    draw_text(fb, 210, y, "ENC", COLOR_DIM);               y += 10;
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "%u", st->holds);
        draw_text(fb, 12, y, buf, COLOR_TEXT);
        snprintf(buf, sizeof(buf), "%u", st->knob_deltas);
        draw_text(fb, 120, y, buf, COLOR_TEXT);
        snprintf(buf, sizeof(buf), "%u", st->encoder_deltas);
        draw_text(fb, 210, y, buf, COLOR_TEXT);
        y += 10;
    }

    /* Last key. */
    fill_rect(fb, 6, y + 4, SYNTH_DISPLAY_WIDTH - 12, 20, COLOR_PANEL);
    draw_text(fb, 12, y + 10, "LAST KEY", COLOR_DIM);
    draw_text(fb, 110, y + 10, st->last_key, COLOR_ACCENT);

    /* Footer with the key map. */
    fill_rect(fb, 0, SYNTH_DISPLAY_HEIGHT - 46, SYNTH_DISPLAY_WIDTH, 46, COLOR_PANEL);
    draw_text(fb, 6, SYNTH_DISPLAY_HEIGHT - 38, "1QWE ASDF ZXCV = MATRIX", COLOR_DIM);
    draw_text(fb, 6, SYNTH_DISPLAY_HEIGHT - 28, "G H J K L T Y = MODIFIERS", COLOR_DIM);
    draw_text(fb, 6, SYNTH_DISPLAY_HEIGHT - 18, "WHEEL = KNOB A   ESC = QUIT", COLOR_DIM);
}

/* ─── Event handling ────────────────────────────────────────────── */

static void consume_events(sim_stats_t *st)
{
    hal_input_event_t ev;
    while (hal_input_pop(&ev)) {
        switch (ev.type) {
        case HAL_INPUT_KEY_PRESS:
            st->presses++;
            snprintf(st->last_key, sizeof(st->last_key), "K%u",
                     (unsigned)ev.key_id);
            break;
        case HAL_INPUT_KEY_RELEASE:    st->releases++; break;
        case HAL_INPUT_KEY_LONG_PRESS: st->long_presses++; break;
        case HAL_INPUT_KEY_HOLD:       st->holds++; break;
        case HAL_INPUT_KNOB_DELTA:     st->knob_deltas++; break;
        case HAL_INPUT_ENCODER_DELTA:  st->encoder_deltas++; break;
        }
    }
}

/* ─── Interactive mode ──────────────────────────────────────────── */

static int run_interactive(void)
{
    sim_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    strcpy(stats.last_key, "-");

    HAL_LOG_INFO(TAG, "simulator starting (ESC to quit)");

    bool running = true;
    uint32_t last_draw = 0;

    while (running) {
        /* Service the input backend, which drains SDL's event queue. */
        hal_input_tick();
        consume_events(&stats);

        /* The window was closed, or the user pressed Escape. */
        if (hal_input_quit_requested()) {
            break;
        }

        /* The render loop is deliberately throttled here, in the
         * application layer, not in the HAL. */
        uint32_t now = SDL_GetTicks();
        if (now - last_draw >= 50) {
            last_draw = now;

            uint16_t *fb = hal_display_framebuffer();
            if (fb != NULL) {
                draw_screen(fb, &stats, running);
                hal_display_submit_dirty(NULL);
            }
        }

        SDL_Delay(10);
    }

    HAL_LOG_INFO(TAG, "simulator exiting");
    return 0;
}

/* ─── Self-test ─────────────────────────────────────────────────── */

static int failures;

#define CHECK(cond, ...)                                                  \
    do {                                                                 \
        if (cond) {                                                      \
            printf("  ok   ");                                           \
        } else {                                                         \
            printf("  FAIL ");                                           \
            failures++;                                                  \
        }                                                                \
        printf(__VA_ARGS__);                                             \
        printf("\n");                                                    \
    } while (0)

static void test_log(void)
{
    printf("log:\n");
    hal_log_set_level(HAL_LOG_LEVEL_INFO);
    CHECK(hal_log_enabled(HAL_LOG_LEVEL_ERROR), "error level enabled");
    CHECK(hal_log_enabled(HAL_LOG_LEVEL_INFO),  "info level enabled");
    CHECK(!hal_log_enabled(HAL_LOG_LEVEL_DEBUG), "debug suppressed at INFO");
    hal_log_set_level(HAL_LOG_LEVEL_NONE);
    CHECK(!hal_log_enabled(HAL_LOG_LEVEL_ERROR), "nothing enabled at NONE");
    hal_log_set_level(HAL_LOG_LEVEL_INFO);
}

static void test_display(void)
{
    printf("display:\n");
    CHECK(hal_display_width() == SYNTH_DISPLAY_WIDTH, "width == %d",
          SYNTH_DISPLAY_WIDTH);
    CHECK(hal_display_height() == SYNTH_DISPLAY_HEIGHT, "height == %d",
          SYNTH_DISPLAY_HEIGHT);

    uint16_t *fb = hal_display_framebuffer();
    CHECK(fb != NULL, "framebuffer available");

    if (fb != NULL) {
        hal_display_clear(0x1234);
        CHECK(fb[0] == 0x1234, "clear fills the framebuffer");

        fill_rect(fb, 20, 30, 10, 10, 0xABCD);
        hal_rect_t r = { 20, 30, 10, 10 };
        CHECK(hal_display_submit_dirty(&r) == SYS_OK, "dirty rect submit");

        hal_rect_t off = { -50, -50, 10, 10 };
        CHECK(hal_display_submit_dirty(&off) == SYS_OK, "off-screen rect is a no-op");

        hal_rect_t bad = { 0, 0, 0, 0 };
        CHECK(hal_display_submit_dirty(&bad) == SYS_ERR_INVALID_ARG,
              "zero-size rect rejected");
    }

    CHECK(hal_display_backlight_set(50) == SYS_OK, "backlight set");
    CHECK(hal_display_backlight_get() == 50, "backlight readback");
    CHECK(hal_display_backlight_set(200) == SYS_ERR_INVALID_ARG,
          "backlight >100 rejected");
}

static void test_input(void)
{
    printf("input:\n");
    CHECK(hal_input_init() == SYS_OK, "init");
    CHECK(hal_input_pending() == 0, "queue starts empty");
    CHECK(!hal_input_is_pressed(SYNTH_KEY_STEP1), "no key pressed yet");

    /* With no SDL events pending, ticking adds nothing. */
    hal_input_tick();
    CHECK(hal_input_pending() == 0, "tick with no SDL events adds nothing");

    hal_input_flush();
    CHECK(hal_input_pending() == 0, "flush empties the queue");
}

static void test_audio(void)
{
    printf("audio:\n");
    CHECK(hal_audio_init() == SYS_OK, "init");
    CHECK(hal_audio_is_initialised(), "reports initialised");
    CHECK(hal_audio_set_volume(3) == SYS_OK, "set volume");
    CHECK(hal_audio_get_volume() == 3, "volume readback");
    CHECK(hal_audio_set_volume(99) == SYS_ERR_INVALID_ARG, "volume > max rejected");
    CHECK(hal_audio_stop() == SYS_OK, "stop");
    CHECK(!hal_audio_is_running(), "not running after stop");
    CHECK(hal_audio_start() == SYS_OK, "start");
    CHECK(hal_audio_is_running(), "running after start");
}

static void test_storage(void)
{
    printf("storage:\n");
    CHECK(hal_storage_init() == SYS_OK, "init");
    CHECK(hal_storage_is_mounted(), "reports mounted");

    const char *path = "/sketches/selftest/hello.bin";
    const char *payload = "ravine hal selftest";

    size_t written = 0;
    CHECK(hal_storage_write(path, payload, strlen(payload), &written) == SYS_OK,
          "write");
    CHECK(written == strlen(payload), "wrote %u bytes", (unsigned)written);
    CHECK(hal_storage_exists(path), "file exists");

    char buf[64];
    size_t got = 0;
    CHECK(hal_storage_read(path, buf, sizeof(buf), &got) == SYS_OK, "read");
    CHECK(got == strlen(payload), "read %u bytes", (unsigned)got);
    CHECK(memcmp(buf, payload, got) == 0, "contents match");

    hal_file_stat_t st;
    CHECK(hal_storage_stat(path, &st) == SYS_OK, "stat");
    CHECK(st.size_bytes == strlen(payload), "stat size matches");

    CHECK(hal_storage_read("/sketches/selftest/nope.bin", buf, sizeof(buf), &got)
          == SYS_ERR_NOT_FOUND, "missing file reports NOT_FOUND");

    /* Path traversal must be refused. */
    CHECK(hal_storage_write("/sketches/../../escape.bin", "x", 1, NULL)
          == SYS_ERR_INVALID_ARG, "path traversal rejected");

    char names[8 * HAL_STORAGE_NAME_MAX];
    size_t count = 0;
    CHECK(hal_storage_list("/sketches/selftest", names, 8, &count) == SYS_OK,
          "list");
    CHECK(count >= 1, "listing found %u entries", (unsigned)count);

    CHECK(hal_storage_delete(path) == SYS_OK, "delete");
    CHECK(!hal_storage_exists(path), "file gone after delete");

    CHECK(hal_storage_remove_dir("/sketches/selftest") == SYS_OK, "remove dir");
}

static void test_crypto(void)
{
    printf("crypto:\n");

    /* SHA-256 of "abc" — the standard NIST test vector. */
    uint8_t digest[HAL_CRYPTO_SHA256_LEN];
    const char *abc = "abc";
    CHECK(hal_crypto_sha256(abc, 3, digest) == SYS_OK, "sha256 runs");

    char hex[HAL_CRYPTO_SHA256_LEN * 2 + 1];
    hal_crypto_to_hex(digest, HAL_CRYPTO_SHA256_LEN, hex);
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223"
                        "b00361a396177a9cb410ff61f20015ad") == 0,
          "sha256(\"abc\") matches the known vector");

    CHECK(hal_crypto_sha256("", 0, digest) == SYS_OK, "sha256 of empty string");
    hal_crypto_to_hex(digest, HAL_CRYPTO_SHA256_LEN, hex);
    CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb924"
                        "27ae41e4649b934ca495991b7852b855") == 0,
          "sha256(\"\") matches the known vector");

    /* Ed25519, where the backend supports it. */
    if (hal_crypto_ed25519_available()) {
        uint8_t priv[HAL_CRYPTO_ED25519_PRIV_LEN];
        uint8_t pub[HAL_CRYPTO_ED25519_PUB_LEN];
        uint8_t sig[HAL_CRYPTO_ED25519_SIG_LEN];

        CHECK(hal_crypto_ed25519_keygen(priv, pub) == SYS_OK, "ed25519 keygen");
        CHECK(hal_crypto_ed25519_sign(priv, pub, abc, 3, sig) == SYS_OK,
              "ed25519 sign");
        CHECK(hal_crypto_ed25519_verify(pub, abc, 3, sig) == SYS_OK,
              "ed25519 verify");

        sig[0] ^= 0xFF;
        CHECK(hal_crypto_ed25519_verify(pub, abc, 3, sig) != SYS_OK,
              "tampered signature is rejected");
    } else {
        printf("  skip ed25519 (not available on this backend)\n");
    }

    const uint8_t a[4] = { 1, 2, 3, 4 };
    const uint8_t b[4] = { 1, 2, 3, 4 };
    const uint8_t c[4] = { 1, 2, 3, 5 };
    CHECK(hal_crypto_equal(a, b, 4), "equal buffers compare equal");
    CHECK(!hal_crypto_equal(a, c, 4), "different buffers compare unequal");
}

static void test_network(void)
{
    printf("network:\n");
    CHECK(hal_net_init() == SYS_OK, "init");
    CHECK(hal_net_get_state() != HAL_NET_STATE_OFF, "backend is up");
    CHECK(hal_net_set_mode(HAL_NET_MODE_STA) == SYS_OK, "select STA mode");
    CHECK(hal_net_get_mode() == HAL_NET_MODE_STA, "STA mode active");
    CHECK(hal_net_set_mode(HAL_NET_MODE_AP) == SYS_OK, "switch to AP mode");
    CHECK(hal_net_get_mode() == HAL_NET_MODE_AP, "AP mode active");

    hal_net_info_t info;
    CHECK(hal_net_get_info(&info) == SYS_OK, "get info");
    CHECK(info.ip[0] != '\0', "ip reported: %s", info.ip);

    /* A pinned CA must be present before any TLS request. */
    CHECK(hal_net_set_ca_pem("-----BEGIN CERTIFICATE-----\n"
                             "not-a-real-cert\n"
                             "-----END CERTIFICATE-----\n", 0) == SYS_OK,
          "set pinned CA");
}

static int run_selftest(void)
{
    printf("\n=== Ravine Phoenix HAL self-test (host simulator) ===\n\n");

    failures = 0;

    test_log();
    test_display();
    test_input();
    test_audio();
    test_storage();
    test_crypto();
    test_network();

    printf("\n=== %s ===\n", (failures == 0) ? "ALL PASS" : "FAILURES");
    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}

/* ─── Entry point ───────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    bool selftest = (argc > 1 && strcmp(argv[1], "--selftest") == 0);

    hal_log_init();

    sys_status_t st = hal_display_init();
    if (st != SYS_OK) {
        fprintf(stderr, "display init failed: %s\n", sys_status_str(st));
        return 1;
    }

    if (selftest) {
        int rc = run_selftest();
        hal_display_deinit();
        return rc;
    }

    int rc = run_interactive();
    hal_display_deinit();
    return rc;
}

#endif /* BOARD_SIMULATOR */
