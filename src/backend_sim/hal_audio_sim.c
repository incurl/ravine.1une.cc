/*
 * hal_audio_sim.c — SDL2 audio backend (host simulator).
 *
 * A SILENT STUB. It performs no audio synthesis and produces no sound.
 *
 * The simulator's purpose is to exercise the application logic: menus,
 * parameter edits, the sequencer's step machine, storage round-trips.
 * The audio DSP (AMY) is real-time, deterministic and already covered
 * by the firmware's own tests; duplicating it in the simulator would
 * add a second implementation to keep in sync for no benefit.
 *
 * What this stub does do is keep the *state* honest: it tracks whether
 * the output is running, what the volume level is, and what sample
 * rate was requested, so that application code which queries those
 * values behaves the same way on both targets.
 *
 * A consequence worth knowing: code that depends on audible output
 * (for instance a recording level meter fed from the mixer) will not
 * see data in the simulator. The HAL interface deliberately offers no
 * read-back of PCM data, so nothing above the HAL depends on it.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "hal/hal_audio.h"

#include "boards/synth_board.h"
#include "hal/hal_log.h"

static const char *TAG = "hal_audio_sim";

static bool    s_initialised;
static bool    s_running;
static uint8_t s_volume;
static uint32_t s_sample_rate = SYNTH_AUDIO_SAMPLE_RATE_HZ;

sys_status_t hal_audio_init(void)
{
    if (s_initialised) {
        return SYS_OK;
    }
    s_initialised = true;
    s_running     = true;
    s_volume      = HAL_AUDIO_VOLUME_MAX;

    HAL_LOG_INFO(TAG, "audio HAL ready (silent stub, %u Hz)",
                 (unsigned)s_sample_rate);
    return SYS_OK;
}

sys_status_t hal_audio_deinit(void)
{
    s_initialised = false;
    s_running     = false;
    return SYS_OK;
}

sys_status_t hal_audio_start(void)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    s_running = true;
    return SYS_OK;
}

sys_status_t hal_audio_stop(void)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    s_running = false;
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
    if (rate_hz == 0) {
        return SYS_ERR_INVALID_ARG;
    }
    /* The stub accepts any rate so that application code paths which
     * change the rate behave the same on both targets. The firmware
     * backend rejects it, because AMY's rate is a build-time constant;
     * that difference is deliberate and documented in each backend. */
    s_sample_rate = rate_hz;
    return SYS_OK;
}

uint32_t hal_audio_get_sample_rate(void)
{
    return s_sample_rate;
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

#endif /* BOARD_SIMULATOR */
