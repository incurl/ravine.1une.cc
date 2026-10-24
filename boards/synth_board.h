/*
 * synth_board.h — unified hardware description for the Ravine Phoenix.
 *
 * Single source of truth for everything that differs between the two
 * build targets: the ESP32-S3 firmware and the SDL2 host simulator.
 * Both targets compile against this exact header, so a pin or a panel
 * dimension can never be described twice and drift apart.
 *
 * Design rules enforced here:
 *   - Platform-neutral only. No GPIO_NUM_*, no esp_err_t, no SDL types,
 *     no ESP-IDF or SDL2 headers. This header must stay includable from
 *     any layer, including the host simulator.
 *   - GPIO numbers are plain integers. On ESP32-S3 the GPIO_NUM_* enum
 *     values are numerically identical to the raw pin index, so the
 *     firmware backend can use these directly; the backend's
 *     _Static_asserts cross-check them against main/config.h.
 *   - No mutable state, no function bodies.
 *
 * Provenance: the values below mirror main/config.h (the firmware's
 * original pin map). The ESP32 backend asserts equality at compile
 * time, so this header cannot silently drift from the hardware.
 */
#ifndef SYNTH_BOARD_H
#define SYNTH_BOARD_H

/* ─── Identity ────────────────────────────────────────────────── */

#define SYNTH_BOARD_NAME            "Ravine Phoenix"

/* ─── Display: 2.4" TFT, 240x320, RGB565 ──────────────────────── */

#define SYNTH_DISPLAY_WIDTH         240
#define SYNTH_DISPLAY_HEIGHT        320
#define SYNTH_DISPLAY_BITS_PER_PIXEL 16

/** Bytes of one full-screen RGB565 framebuffer. */
#define SYNTH_DISPLAY_FB_BYTES \
    (SYNTH_DISPLAY_WIDTH * SYNTH_DISPLAY_HEIGHT * (SYNTH_DISPLAY_BITS_PER_PIXEL / 8))

/** Default backlight brightness, percent (0..100). */
#define SYNTH_DISPLAY_BACKLIGHT_DEFAULT_PCT 70

/**
 * SPI bus instance carrying the panel.
 *
 * This is the ESP-IDF host id (spi_host_device_t): SPI1_HOST is 0 and
 * SPI2_HOST is 1. The firmware's own TFT_SPI_HOST is SPI2_HOST, and
 * the ESP32 backend asserts that the two agree at compile time.
 */
#define SYNTH_DISPLAY_SPI_HOST      1

/** Panel SPI clock, Hz. 40 MHz is the conservative limit for a 2.4"
 * ILI9341 on a short ribbon. */
#define SYNTH_DISPLAY_SPI_CLK_HZ    (40 * 1000 * 1000)

/* Panel pins (raw GPIO index). */
#define SYNTH_DISPLAY_PIN_MOSI      6
#define SYNTH_DISPLAY_PIN_SCK       7
#define SYNTH_DISPLAY_PIN_CS        5
#define SYNTH_DISPLAY_PIN_DC        4
#define SYNTH_DISPLAY_PIN_RST       48
#define SYNTH_DISPLAY_PIN_BL        47

/** Backlight is a PWM-driven channel on LEDC. */
#define SYNTH_DISPLAY_BL_PWM_TIMER  0
#define SYNTH_DISPLAY_BL_PWM_CHANNEL 0
#define SYNTH_DISPLAY_BL_PWM_HZ     5000

/* ─── Keypad: 4x4 matrix + 7 dedicated modifier buttons ───────── */

/**
 * The keypad is two hardware groups, exactly as in main/config.h:
 *
 *   (A) A 4x4 matrix holding the 16 polymorphic step buttons. Row-major
 *       ordering: key index = row * SYNTH_KEY_COLS + column.
 *   (B) Seven dedicated momentary buttons wired to individual GPIOs.
 *
 * Both groups use the same debounce and long-press timing.
 */
#define SYNTH_KEY_ROWS              4
#define SYNTH_KEY_COLS              4

/** Matrix row pins (driven as outputs). */
#define SYNTH_KEY_ROW_PINS          { 35, 36, 37, 38 }
/** Matrix column pins (read as inputs, internal pull-up). */
#define SYNTH_KEY_COL_PINS          { 33, 34, 39, 40 }

/** Dedicated modifier button pins. */
#define SYNTH_KEY_BTN_SOUND_PIN     11
#define SYNTH_KEY_BTN_PATTERN_PIN   44
#define SYNTH_KEY_BTN_BPM_PIN       13
#define SYNTH_KEY_BTN_REC_PIN       41
#define SYNTH_KEY_BTN_FX_PIN        12
#define SYNTH_KEY_BTN_PLAY_PIN      42
#define SYNTH_KEY_BTN_WRITE_PIN     43

#define SYNTH_KEY_DEDICATED_COUNT   7

/**
 * Logical key ids. The seven dedicated buttons occupy 0..6; the sixteen
 * matrix step buttons occupy 7..22. This mirrors BTN_* in main/config.h
 * so the legacy driver and the HAL agree on numbering.
 */
#define SYNTH_KEY_BTN_SOUND         0
#define SYNTH_KEY_BTN_PATTERN       1
#define SYNTH_KEY_BTN_BPM           2
#define SYNTH_KEY_BTN_REC           3
#define SYNTH_KEY_BTN_FX            4
#define SYNTH_KEY_BTN_PLAY          5
#define SYNTH_KEY_BTN_WRITE         6
#define SYNTH_KEY_STEP1             7
#define SYNTH_KEY_TOTAL_COUNT       23  /* 7 dedicated + 16 matrix */

/** First matrix key id. */
#define SYNTH_KEY_MATRIX_FIRST      SYNTH_KEY_STEP1
/** Number of matrix keys. */
#define SYNTH_KEY_MATRIX_COUNT      16

/** Debounce window, ms. A press shorter than this is discarded. */
#define SYNTH_KEY_DEBOUNCE_MS       30
/** Long-press threshold, ms. Fires KEY_LONG_PRESS once while held. */
#define SYNTH_KEY_LONG_PRESS_MS     600
/** Repeat cadence for KEY_HOLD events, ms. */
#define SYNTH_KEY_HOLD_REPEAT_MS    100

/* ─── Analog controls: two knobs + one rotary encoder ─────────── */

#define SYNTH_KNOB_A_GPIO           2
#define SYNTH_KNOB_B_GPIO           46
/** ADC channel indices for the two knobs. */
#define SYNTH_KNOB_A_ADC_CHANNEL    1
#define SYNTH_KNOB_B_ADC_CHANNEL    5
/** Moving-average depth per knob read. */
#define SYNTH_KNOB_SAMPLES          8
/** Dead zone in 0..255 knob units; deltas below this are ignored. */
#define SYNTH_KNOB_DEADZONE         4

/** Rotary encoder (Alps EC11E): two phases plus a click switch. */
#define SYNTH_ENCODER_A_GPIO        20
#define SYNTH_ENCODER_B_GPIO        26
#define SYNTH_ENCODER_CLICK_GPIO    45

/* ─── Audio: I2S out + I2S in ──────────────────────────────────── */

#define SYNTH_AUDIO_SAMPLE_RATE_HZ  44100
#define SYNTH_AUDIO_BITS_PER_SAMPLE 16

#define SYNTH_AUDIO_PIN_OUT_BCLK    9
#define SYNTH_AUDIO_PIN_OUT_LRCK    10
#define SYNTH_AUDIO_PIN_OUT_DATA    8
#define SYNTH_AUDIO_PIN_IN_BCLK     15
#define SYNTH_AUDIO_PIN_IN_LRCK     16
#define SYNTH_AUDIO_PIN_IN_DATA     17

/* ─── Status LEDs ──────────────────────────────────────────────── */

#define SYNTH_LED_REC_GPIO          21
#define SYNTH_LED_PLAY_GPIO         14

/* ─── Jam-sync ─────────────────────────────────────────────────── */

#define SYNTH_SYNC_OUT_GPIO         18
#define SYNTH_SYNC_IN_GPIO          19

/* ─── Storage ──────────────────────────────────────────────────── */

/**
 * Root of the sketch store on the device. The firmware mounts LittleFS
 * here; the simulator maps it into its local flash directory.
 */
#define SYNTH_STORAGE_ROOT          "/sketches"

/* ─── Runtime ──────────────────────────────────────────────────── */

/** FreeRTOS stack depth for the HAL UI/event task, in words. */
#define SYNTH_UI_TASK_STACK_WORDS   4096
/** Priority of the HAL UI/event task. Deliberately below the audio
 * render task so UI work can never starve the DSP. */
#define SYNTH_UI_TASK_PRIORITY      5
/** Priority of the keypad scan task. */
#define SYNTH_KEY_SCAN_TASK_PRIORITY 10

/** Simulator host-side flash directory, relative to the process CWD. */
#define SYNTH_SIM_FLASH_DIR         "sim_flash"

#endif /* SYNTH_BOARD_H */

