/*
 * hal_audio.h — audio output control abstraction.
 *
 * This HAL is a *hardware control surface only*. It starts and stops
 * the output, sets the sample rate and master volume. It does not
 * synthesise, mix, filter or schedule anything.
 *
 * The synthesiser DSP lives entirely outside the HAL, in the AMY
 * library (components/amy). The DSP owns the I2S peripheral on the
 * firmware target and runs its own real-time render task. Nothing in
 * this interface is called from that task or from any ISR.
 *
 * Rules:
 *   - No HAL function may be called from an ISR.
 *   - The real-time audio path must not call into the HAL. If the
 *     hardware needs a change while audio is running, the change must
 *     be deferred to a non-real-time context.
 *   - The simulator backend is a silent stub. It performs no audio
 *     synthesis and produces no sound; it only tracks state so that
 *     application logic behaves identically on both targets.
 */
#ifndef SYNTH_HAL_AUDIO_H
#define SYNTH_HAL_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Master volume steps. Level 0 is silent, HAL_AUDIO_VOLUME_MAX is loudest. */
#define HAL_AUDIO_VOLUME_MAX 5

/**
 * Initialise the audio backend.
 *
 * On the firmware this boots the AMY engine and its I2S output. On the
 * simulator this is a no-op that only initialises state.
 *
 * Returns SYS_OK, SYS_ERR_NO_MEM, or SYS_ERR_IO.
 */
sys_status_t hal_audio_init(void);

/**
 * Stop the audio backend and release its resources.
 */
sys_status_t hal_audio_deinit(void);

/**
 * Start the audio output.
 *
 * Idempotent: starting an already-running output returns SYS_OK.
 */
sys_status_t hal_audio_start(void);

/**
 * Stop the audio output.
 *
 * Idempotent: stopping an already-stopped output returns SYS_OK.
 */
sys_status_t hal_audio_stop(void);

/**
 * @return true when the output is running.
 */
bool hal_audio_is_running(void);

/**
 * Set the output sample rate.
 *
 * @param rate_hz Requested rate. The backend may round to the nearest
 *                supported rate; query the effective value with
 *                hal_audio_get_sample_rate().
 *
 * Changing the rate while audio is running requires a restart; the
 * backend performs it internally and keeps the transport running.
 *
 * Returns SYS_OK or SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_audio_set_sample_rate(uint32_t rate_hz);

/**
 * @return the effective output sample rate in Hz.
 */
uint32_t hal_audio_get_sample_rate(void);

/**
 * Set the master volume.
 *
 * @param level 0 (silent) .. HAL_AUDIO_VOLUME_MAX (loudest).
 *
 * Returns SYS_OK or SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_audio_set_volume(uint8_t level);

/**
 * @return the current master volume level, 0..HAL_AUDIO_VOLUME_MAX.
 */
uint8_t hal_audio_get_volume(void);

/**
 * @return true if the backend is initialised.
 */
bool hal_audio_is_initialised(void);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_HAL_AUDIO_H */
