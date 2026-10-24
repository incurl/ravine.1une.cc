/*
 * hal_input_sim.c — SDL2 input backend (host simulator).
 *
 * Maps the PC keyboard onto the device's keypad, and produces the same
 * four-state event model as the firmware backend.
 *
 * Keyboard mapping
 * ----------------
 * The 4x4 matrix keys are laid out on the keyboard as a 4x4 grid:
 *
 *      1 2 3 4        (matrix row 0)
 *      Q W E R        (matrix row 1)
 *      A S D F        (matrix row 2)
 *      Z X C V        (matrix row 3)
 *
 * The seven dedicated buttons have their own keys:
 *
 *      SOUND   = Q          PATTERN = W        BPM = E
 *      REC     = R          FX      = A        PLAY = S
 *      WRITE   = D
 *
 * The two knobs are driven with the mouse wheel (Knob A) and the left
 * / right arrow keys (Knob B), and the rotary encoder with the up /
 * down arrow keys.
 *
 * Timing semantics
 * ----------------
 * The simulator reproduces the firmware's event timing: a key that is
 * held past SYNTH_KEY_LONG_PRESS_MS emits one KEY_LONG_PRESS, and
 * continues to emit KEY_HOLD every SYNTH_KEY_HOLD_REPEAT_MS until it
 * is released.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "hal/hal_input.h"

#include "boards/synth_board.h"
#include "common/event_queue.h"
#include "hal/hal_log.h"

#include <SDL2/SDL.h>

#include <string.h>

static const char *TAG = "hal_input_sim";

#define INPUT_QUEUE_DEPTH 64

/* Milliseconds since the simulator started. */
static uint32_t now_ms(void)
{
    return (uint32_t)SDL_GetTicks();
}

static event_queue_t *s_queue;
static bool            s_initialised;

/* Which keyboard key maps to which logical key id. */
static SDL_Scancode s_keymap[SYNTH_KEY_TOTAL_COUNT];
static bool         s_keymap_set[SYNTH_KEY_TOTAL_COUNT];

/* Live state of every logical key. */
static bool     s_pressed[SYNTH_KEY_TOTAL_COUNT];
static int64_t  s_press_at[SYNTH_KEY_TOTAL_COUNT];
static bool     s_long_fired[SYNTH_KEY_TOTAL_COUNT];
static int64_t  s_hold_at[SYNTH_KEY_TOTAL_COUNT];
static bool     s_holding[SYNTH_KEY_TOTAL_COUNT];

/* Knob simulation state. Knob A is driven by the mouse wheel; Knob B
 * is driven by the left/right arrow keys, which the application layer
 * reads back through the knob event's value. */
static uint8_t s_knob_a = 128;
static uint8_t s_knob_b = 128;

/* Set when the user closes the window or presses Escape. */
static bool s_quit_requested;

/* ─── Mapping ───────────────────────────────────────────────────── */

static void map_key(uint8_t key_id, SDL_Scancode sc)
{
    s_keymap[key_id]    = sc;
    s_keymap_set[key_id] = true;
}

static void build_keymap(void)
{
    /* The 4x4 matrix: logical key id = SYNTH_KEY_MATRIX_FIRST + r*4 + c */
    static const SDL_Scancode matrix[16] = {
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
        SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
        SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
        SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
    };
    for (int i = 0; i < 16; i++) {
        map_key((uint8_t)(SYNTH_KEY_MATRIX_FIRST + i), matrix[i]);
    }

    map_key(SYNTH_KEY_BTN_SOUND,   SDL_SCANCODE_G);
    map_key(SYNTH_KEY_BTN_PATTERN, SDL_SCANCODE_H);
    map_key(SYNTH_KEY_BTN_BPM,     SDL_SCANCODE_J);
    map_key(SYNTH_KEY_BTN_REC,     SDL_SCANCODE_K);
    map_key(SYNTH_KEY_BTN_FX,      SDL_SCANCODE_L);
    map_key(SYNTH_KEY_BTN_PLAY,    SDL_SCANCODE_T);
    map_key(SYNTH_KEY_BTN_WRITE,   SDL_SCANCODE_Y);
}

/* Find the logical key id bound to a scancode, or -1. */
static int key_for_scancode(SDL_Scancode sc)
{
    for (uint8_t id = 0; id < SYNTH_KEY_TOTAL_COUNT; id++) {
        if (s_keymap_set[id] && s_keymap[id] == sc) {
            return (int)id;
        }
    }
    return -1;
}

/* ─── Events ────────────────────────────────────────────────────── */

static void push_event(hal_input_event_type_t type,
                       uint8_t key_id,
                       int32_t value)
{
    hal_input_event_t ev;
    ev.type         = type;
    ev.key_id       = key_id;
    ev.value        = value;
    ev.timestamp_ms = now_ms();

    if (key_id >= SYNTH_KEY_MATRIX_FIRST &&
        key_id < SYNTH_KEY_MATRIX_FIRST + SYNTH_KEY_MATRIX_COUNT) {
        int idx = key_id - SYNTH_KEY_MATRIX_FIRST;
        ev.row   = (uint8_t)(idx / SYNTH_KEY_COLS);
        ev.column = (uint8_t)(idx % SYNTH_KEY_COLS);
    } else {
        ev.row    = SYNTH_COORD_NONE;
        ev.column = SYNTH_COORD_NONE;
    }

    if (event_queue_push(s_queue, &ev) != SYS_OK) {
        HAL_LOG_WARN(TAG, "input queue full, dropping event");
    }
}

/* ─── Init ──────────────────────────────────────────────────────── */

sys_status_t hal_input_init(void)
{
    if (s_initialised) {
        return SYS_OK;
    }

    sys_status_t st = event_queue_create(sizeof(hal_input_event_t),
                                         INPUT_QUEUE_DEPTH,
                                         &s_queue);
    if (st != SYS_OK) {
        return st;
    }

    memset(s_pressed, 0, sizeof(s_pressed));
    memset(s_long_fired, 0, sizeof(s_long_fired));
    memset(s_holding, 0, sizeof(s_holding));
    memset(s_keymap_set, 0, sizeof(s_keymap_set));
    build_keymap();

    s_initialised = true;
    HAL_LOG_INFO(TAG, "input HAL ready: keyboard mapped to %d keys",
                 SYNTH_KEY_TOTAL_COUNT);
    return SYS_OK;
}

sys_status_t hal_input_deinit(void)
{
    if (!s_initialised) {
        return SYS_OK;
    }
    event_queue_destroy(s_queue);
    s_queue       = NULL;
    s_initialised = false;
    return SYS_OK;
}


/* ─── Tick ──────────────────────────────────────────────────────── */

void hal_input_tick(void)
{
    if (!s_initialised) {
        return;
    }

    SDL_Event ev;
    const uint32_t now = now_ms();

    /* 1. Drain SDL's event queue. */
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_KEYDOWN: {
            /* Ignore auto-repeat: the firmware debounces, so a held key
             * must not produce a second press. */
            if (ev.key.repeat) {
                break;
            }
            /* Escape is the host's quit gesture. */
            if (ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                s_quit_requested = true;
                break;
            }
            /* The arrow keys drive Knob B and the rotary encoder. */
            if (ev.key.keysym.scancode == SDL_SCANCODE_LEFT ||
                ev.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
                int step = (ev.key.keysym.scancode == SDL_SCANCODE_RIGHT)
                               ? 4 : -4;
                int v = (int)s_knob_b + step;
                if (v < 0)   { v = 0; }
                if (v > 255) { v = 255; }
                s_knob_b = (uint8_t)v;
                push_event(HAL_INPUT_KNOB_DELTA, SYNTH_KNOB_B, step);
                break;
            }
            if (ev.key.keysym.scancode == SDL_SCANCODE_UP ||
                ev.key.keysym.scancode == SDL_SCANCODE_DOWN) {
                int detent = (ev.key.keysym.scancode == SDL_SCANCODE_UP)
                                 ? 1 : -1;
                push_event(HAL_INPUT_ENCODER_DELTA, 0, detent);
                break;
            }
            int id = key_for_scancode(ev.key.keysym.scancode);
            if (id >= 0) {
                uint8_t key_id = (uint8_t)id;
                s_pressed[key_id]   = true;
                s_long_fired[key_id] = false;
                s_holding[key_id]   = false;
                s_press_at[key_id]  = now;
                s_hold_at[key_id]   = now;
                push_event(HAL_INPUT_KEY_PRESS, key_id, 0);
            }
            break;
        }

        case SDL_KEYUP: {
            int id = key_for_scancode(ev.key.keysym.scancode);
            if (id >= 0) {
                uint8_t key_id = (uint8_t)id;
                s_pressed[key_id]   = false;
                s_holding[key_id]   = false;
                push_event(HAL_INPUT_KEY_RELEASE, key_id, 0);
            }
            break;
        }

        case SDL_MOUSEWHEEL: {
            /* Wheel drives Knob A, in steps of 4 (about the size of the
             * firmware's dead zone). */
            int delta = (ev.wheel.y > 0) ? 4 : -4;
            int v = (int)s_knob_a + delta;
            if (v < 0)   { v = 0; }
            if (v > 255) { v = 255; }
            s_knob_a = (uint8_t)v;
            push_event(HAL_INPUT_KNOB_DELTA, SYNTH_KNOB_A, delta);
            break;
        }

        case SDL_QUIT:
            /* The window was closed. */
            s_quit_requested = true;
            break;

        default:
            break;
        }
    }

    /* 2. Long-press and HOLD repeats for keys that are still down. */
    for (uint8_t id = 0; id < SYNTH_KEY_TOTAL_COUNT; id++) {
        if (!s_pressed[id]) {
            continue;
        }
        int64_t held = (int64_t)now - s_press_at[id];

        if (!s_long_fired[id] && held >= SYNTH_KEY_LONG_PRESS_MS) {
            s_long_fired[id] = true;
            push_event(HAL_INPUT_KEY_LONG_PRESS, id, 0);
            s_holding[id] = true;
            s_hold_at[id] = now;
            continue;
        }

        if (s_holding[id] && ((int64_t)now - s_hold_at[id]) >= SYNTH_KEY_HOLD_REPEAT_MS) {
            s_hold_at[id] = now;
            push_event(HAL_INPUT_KEY_HOLD, id, 0);
        }
    }
}

/* ─── Consumer API ──────────────────────────────────────────────── */

bool hal_input_pop(hal_input_event_t *out)
{
    if (out == NULL || !s_initialised) {
        return false;
    }
    return event_queue_pop(s_queue, out) == SYS_OK;
}

bool hal_input_is_pressed(uint8_t key_id)
{
    if (!s_initialised || key_id >= SYNTH_KEY_TOTAL_COUNT) {
        return false;
    }
    return s_pressed[key_id];
}

size_t hal_input_pending(void)
{
    if (!s_initialised) {
        return 0;
    }
    return event_queue_count(s_queue);
}

void hal_input_flush(void)
{
    if (!s_initialised) {
        return;
    }
    event_queue_clear(s_queue);
}

bool hal_input_quit_requested(void)
{
    return s_quit_requested;
}

#endif /* BOARD_SIMULATOR */
