/*
 * sys_status.h — shared status/error codes for every HAL module.
 *
 * Contract:
 *   - Every HAL entry point returns a sys_status_t. Success is SYS_OK (0).
 *   - Codes are stable across both backends: the same logical failure
 *     maps to the same code on ESP32-S3 and on the SDL2 host simulator.
 *   - No backend-specific types (esp_err_t, errno, SDL_Error) leak in
 *     here; the HAL is the translation boundary.
 *
 * Threading: the enum and sys_status_str() are immutable after load and
 * therefore safe to call from any task or ISR.
 */
#ifndef SYNTH_SYS_STATUS_H
#define SYNTH_SYS_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYS_OK = 0,                /* Operation succeeded.                     */
    SYS_ERR_INVALID_ARG,       /* A parameter was NULL, out of range, or
                                * otherwise unusable.                      */
    SYS_ERR_INVALID_STATE,     /* Call is legal but the subsystem is not in
                                * a state that permits it.                */
    SYS_ERR_NO_MEM,            /* Allocation failed.                      */
    SYS_ERR_NOT_FOUND,         /* Requested object does not exist.        */
    SYS_ERR_IO,                /* Underlying I/O failed.                   */
    SYS_ERR_NOT_SUPPORTED,     /* Feature is not implemented on this
                                * backend (see hal_crypto.h for the
                                * Ed25519 note).                         */
    SYS_ERR_TIMEOUT,           /* Operation exceeded its deadline.        */
    SYS_ERR_BUSY,              /* Resource is already in use by another
                                * owner (see the transition notes on
                                * hal_display_init()).                    */
    SYS_ERR_STATUS_COUNT       /* Not a status code; number of codes.     */
} sys_status_t;

/*
 * Stable, human-readable name of a status code.
 *
 * Returns a short static string such as "SYS_ERR_INVALID_ARG". Never
 * returns NULL; unknown values map to "SYS_ERR_UNKNOWN". The returned
 * pointer is to static storage and must not be freed.
 */
const char *sys_status_str(sys_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_SYS_STATUS_H */
