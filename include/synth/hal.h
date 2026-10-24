/*
 * synth/hal.h — umbrella include for the hardware abstraction layer.
 *
 * Application code includes this one header and gets the whole HAL
 * surface. Nothing above the HAL needs to know which backend is
 * compiled in.
 *
 * The HAL is split into seven modules:
 *
 *   hal_display   colour TFT framebuffer and dirty-rectangle push
 *   hal_input     matrix keypad, knobs and rotary encoder events
 *   hal_audio     audio output control (start/stop/rate/volume)
 *   hal_storage   persistent file storage
 *   hal_log       logging
 *   hal_network   Wi-Fi station/AP and HTTPS requests
 *   hal_crypto    SHA-256 and Ed25519
 *
 * Layering rules that this header helps enforce:
 *   - No ESP-IDF header and no SDL2 header is reachable from here.
 *   - Application code must not call a backend function directly.
 *   - The audio DSP lives outside the HAL entirely.
 */
#ifndef SYNTH_HAL_UMBRELLA_H
#define SYNTH_HAL_UMBRELLA_H

#include "common/sys_status.h"
#include "common/event_queue.h"
#include "boards/synth_board.h"

#include "hal/hal_display.h"
#include "hal/hal_input.h"
#include "hal/hal_audio.h"
#include "hal/hal_storage.h"
#include "hal/hal_log.h"
#include "hal/hal_network.h"
#include "hal/hal_crypto.h"

#endif /* SYNTH_HAL_UMBRELLA_H */
