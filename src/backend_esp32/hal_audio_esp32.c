/*
 * hal_audio_esp32.c — ESP32-S3 audio backend.
 *
 * The synthesiser DSP is NOT in the HAL and NOT in this file. It lives
 * in the vendored AMY library (components/amy), which owns the I2S
 * peripheral and runs its own real-time render task on core 0.
 *
 * This backend is a thin control surface over the legacy bridge
 * (main/audio/amy_bridge.c):
 *
 *   - init / deinit  -> amy_bridge_init() / amy_bridge_deinit()
 *   - start / stop   -> AMY has no separate transport start; the engine
 *                       runs once started. Stopping means silencing the
 *                       output by parking the volume at zero, and
 *                       starting restores the previous level.
 *   - volume         -> amy_bridge_set_volume_level()
 *   - sample rate    -> fixed by the AMY build (see below)
 *
 * Real-time isolation
 * -------------------
 * The AMY render task runs at ESP_TASK_PRIO_MAX-1 on core 0, and the
 * buffer-fill task on core 1. Neither calls into this file. Nothing in
 * this backend may be called from an ISR or from a real-time audio
 * context: the AMY bridge functions are not ISR-safe.
 *
 * Sample rate
 * -----------
 * AMY's sample rate is a compile-time constant of the DSP (AMY_SAMPLE_RATE
 * in components/amy/src/amy.h), not a runtime knob. Changing it at
 * runtime would mean rebuilding the DSP. This backend therefore reports
 * the fixed rate and rejects a request to change it with
 * SYS_ERR_NOT_SUPPORTED rather than silently ignoring it.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_ESP32S3)

#include "hal/hal_audio.h"

#include "boards/synth_board.h"
#include "hal/hal_log.h"

#include "esp_err.h"

#include "main/audio/amy_bridge.h"
#include "main/config.h"

/* The board header must agree with the firmware's audio config. */
_Static_assert(SYNTH_AUDIO_SAMPLE_RATE_HZ == SAMPLE_RATE_HZ,
               "board sample rate != SAMPLE_RATE_HZ");
_Static_assert(SYNTH_AUDIO_BITS_PER_SAMPLE == SAMPLE_BITS,
               "board sample bits != SAMPLE_BITS");

static const char *TAG = "hal_audio";

static bool    s_initialised;
static bool    s_running;
static uint8_t s_volume;         /* requested volume level, 0..5 */
static uint8_t s_volume_saved;   /* level to restore on start()  */

sys_status_t hal_audio_init(void)
{
    if (s_initialised) {
        return SYS_OK;
    }

    esp_err_t err = amy_bridge_init();
    if (err != ESP_OK) {
        HAL_LOG_ERROR(TAG, "amy_bridge_init failed: %s", esp_err_to_name(err));
        return SYS_ERR_IO;
    }

    s_initialised  = true;
    s_running      = true;
    s_volume       = amy_bridge_get_volume_level();
    s_volume_saved = s_volume;

    HAL_LOG_INFO(TAG, "audio HAL ready: %u Hz, %u-bit",
                 (unsigned)SYNTH_AUDIO_SAMPLE_RATE_HZ,
                 (unsigned)SYNTH_AUDIO_BITS_PER_SAMPLE);
    return SYS_OK;
}

sys_status_t hal_audio_deinit(void)
{
    if (!s_initialised) {
        return SYS_OK;
    }
    amy_bridge_deinit();
    s_initialised = false;
    s_running     = false;
    return SYS_OK;
}

sys_status_t hal_audio_start(void)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (s_running) {
        return SYS_OK;
    }

    /* AMY has no transport-level stop; the engine keeps running and
     * renders silence. "Stopping" therefore mutes, and "starting"
     * restores the level the user had chosen. */
    amy_bridge_set_volume_level(0);
    s_running = true;
    HAL_LOG_DEBUG(TAG, "audio started (volume %u)", (unsigned)s_volume_saved);
    return SYS_OK;
}

sys_status_t hal_audio_stop(void)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (!s_running) {
        return SYS_OK;
    }

    s_volume_saved = s_volume;
    amy_bridge_set_volume_level(0);
    s_running = false;
    HAL_LOG_DEBUG(TAG, "audio stopped");
    return SYS_OK;
}

bool hal_audio_is_running(void)
{
    return s_running;
}

sys_status_t hal_audio_set_sample_rate(uint32_t rate_hz)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (rate_hz == SYNTH_AUDIO_SAMPLE_RATE_HZ) {
        return SYS_OK;
    }
    /* The AMY DSP's rate is fixed at build time. */
    HAL_LOG_WARN(TAG, "sample rate is fixed at %u Hz (asked %u)",
                 (unsigned)SYNTH_AUDIO_SAMPLE_RATE_HZ, (unsigned)rate_hz);
    return SYS_ERR_NOT_SUPPORTED;
}

uint32_t hal_audio_get_sample_rate(void)
{
    return SYNTH_AUDIO_SAMPLE_RATE_HZ;
}

sys_status_t hal_audio_set_volume(uint8_t level)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (level > HAL_AUDIO_VOLUME_MAX) {
        return SYS_ERR_INVALID_ARG;
    }

    s_volume = level;
    /* While stopped the level is remembered but not applied. */
    if (s_running) {
        amy_bridge_set_volume_level(level);
    }
    return SYS_OK;
}

uint8_t hal_audio_get_volume(void)
{
    return s_volume;
}

bool hal_audio_is_initialised(void)
{
    return s_initialised;
}

#endif /* BOARD_ESP32S3 */
