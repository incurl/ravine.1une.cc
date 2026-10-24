/*
 * hal_input.h — matrix keypad, knobs and rotary encoder abstraction.
 *
 * The input HAL is event-driven. Hardware scanning happens inside the
 * backend; the application layer only ever consumes a queue of events.
 * No raw GPIO, ADC or PCNT handle is visible above this boundary.
 *
 * Event model (four distinct key states, replacing the touch model):
 *
 *   KEY_PRESS      the key transitioned to down
 *   KEY_RELEASE    the key transitioned to up
 *   KEY_LONG_PRESS the key has been held past SYNTH_KEY_LONG_PRESS_MS;
 *                  fires exactly once per hold
 *   KEY_HOLD       the key is still held; repeats every
 *                  SYNTH_KEY_HOLD_REPEAT_MS while it remains down
 *
 * A KEY_HOLD series always follows a KEY_PRESS, and the matching
 * KEY_RELEASE ends it. KEY_LONG_PRESS is emitted between the press and
 * the release.
 *
 * Threading: the backend owns an internal queue. On the firmware it is
 * a FreeRTOS queue, safe to push from the scan task and pop from the
 * UI task. On the simulator it is a single-threaded ring buffer. The
 * public API below never exposes the queue type.
 */
#ifndef SYNTH_HAL_INPUT_H
#define SYNTH_HAL_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HAL_INPUT_KEY_PRESS = 0,
    HAL_INPUT_KEY_RELEASE,
    HAL_INPUT_KEY_LONG_PRESS,
    HAL_INPUT_KEY_HOLD,
    HAL_INPUT_KNOB_DELTA,
    HAL_INPUT_ENCODER_DELTA
} hal_input_event_type_t;

/**
 * One input event.
 *
 * Field meaning by type:
 *   KEY_PRESS / KEY_RELEASE / KEY_LONG_PRESS / KEY_HOLD
 *       key_id       logical key id (SYNTH_KEY_*)
 *       row, column  matrix coordinates; 0xFF when the key is not part
 *                    of the matrix (the seven dedicated buttons)
 *       value        unused
 *   KNOB_DELTA
 *       key_id       SYNTH_KNOB_A / SYNTH_KNOB_B discriminator
 *       value        signed delta, 0..255 scale
 *   ENCODER_DELTA
 *       key_id       unused
 *       value        signed detent count, positive = clockwise
 */
typedef struct {
    hal_input_event_type_t type;
    uint8_t  row;
    uint8_t  column;
    uint8_t  key_id;
    int32_t  value;
    uint64_t timestamp_ms;
} hal_input_event_t;

/** Discriminator values for hal_input_event_t.key_id on KNOB_DELTA. */
#define SYNTH_KNOB_A 0
#define SYNTH_KNOB_B 1

/** Sentinel used for row/column when a key is not on the matrix. */
#define SYNTH_COORD_NONE 0xFF

/**
 * Initialise the input backend.
 *
 * On the firmware this configures the matrix GPIOs, the dedicated
 * buttons, the knob ADCs and the encoder PCNT. On the simulator it
 * prepares the keyboard mapping.
 *
 * NOTE (transition period): the legacy driver main/ui/buttons.c also
 * owns the scan. Exactly one component may call the scan function. If
 * the legacy driver is still ticking, this backend must not.
 *
 * Returns SYS_OK, SYS_ERR_NO_MEM, or SYS_ERR_NOT_SUPPORTED.
 */
sys_status_t hal_input_init(void);

/**
 * Release the input backend. Safe to call when not initialised.
 */
sys_status_t hal_input_deinit(void);

/**
 * Service the hardware scanner.
 *
 * On the firmware this reads the matrix, debounces, and pushes events
 * into the queue. On the simulator this drains SDL keyboard events.
 *
 * Must be called regularly (roughly every 10 ms on the firmware) by
 * whichever task owns the scanner.
 *
 * IMPORTANT: exactly one caller may drive this. During the transition
 * period the legacy driver may still own the scan; in that case the
 * HAL input backend must not be initialised at the same time.
 */
void hal_input_tick(void);

/**
 * Pop one event from the queue.
 *
 * @param out Receives the event. Must not be NULL.
 *
 * @return true if an event was produced, false if the queue was empty.
 *         When false, *out is left untouched.
 */
bool hal_input_pop(hal_input_event_t *out);

/**
 * Current debounced state of a key.
 *
 * @param key_id logical key id.
 *
 * @return true while the key is physically down.
 */
bool hal_input_is_pressed(uint8_t key_id);

/**
 * Queue depth. Useful for diagnostics and for detecting overrun.
 */
size_t hal_input_pending(void);

/**
 * Drop every queued event. Does not touch the physical key state.
 */
void hal_input_flush(void);

/**
 * Whether the user asked to quit.
 *
 * The simulator sets this when the window is closed or the user presses
 * the platform's quit key (Escape on the host). The firmware backend
 * always returns false: the device has no window to close and quits
 * through its own power-management path.
 *
 * The application loop should poll this and exit when it becomes true.
 */
bool hal_input_quit_requested(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_HAL_INPUT_H */
