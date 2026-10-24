/*
 * hal_crypto.h — hashing and digital signature abstraction.
 *
 * The device signing flows (DESIGN.md section 14.4) require SHA-256
 * digests and Ed25519 signatures. This interface exposes exactly those
 * two primitives, in a form that behaves identically on the firmware
 * and in the host simulator, so that the canonical-JSON signing code
 * can be shared rather than duplicated.
 *
 * Ed25519 support differs between the two backends today:
 *
 *   - The simulator uses OpenSSL and implements Ed25519 fully.
 *   - The firmware target has no Ed25519 implementation available in
 *     ESP-IDF v6.1: mbedTLS 4.x moved Ed25519 behind the PSA
 *     configuration, and IDF's build does not enable it. The firmware
 *     backend therefore returns SYS_ERR_NOT_SUPPORTED from the
 *     Ed25519 calls rather than failing silently.
 *
 * The interface is identical either way, so application code compiles
 * and runs on both targets; it simply cannot complete a signature on
 * the firmware until an Ed25519 provider is added. Enabling it on the
 * firmware requires either a PSA configuration that turns on
 * PSA_WANT_ALG_PURE_EDDSA together with a key store, or a small
 * standalone Ed25519 implementation; both are recorded as a TODO in
 * the firmware backend.
 *
 * Threading: the hash and sign calls are blocking and allocate their
 * own working state. Do not call them from the real-time audio task.
 */
#ifndef SYNTH_HAL_CRYPTO_H
#define SYNTH_HAL_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Length of a SHA-256 digest, bytes. */
#define HAL_CRYPTO_SHA256_LEN 32
/** Length of an Ed25519 private key, bytes. */
#define HAL_CRYPTO_ED25519_PRIV_LEN 32
/** Length of an Ed25519 public key, bytes. */
#define HAL_CRYPTO_ED25519_PUB_LEN 32
/** Length of an Ed25519 signature, bytes. */
#define HAL_CRYPTO_ED25519_SIG_LEN 64

/**
 * Compute a SHA-256 digest.
 *
 * @param data   Input bytes; may be NULL only if length is 0.
 * @param length Input length in bytes.
 * @param out    Receives 32 bytes of digest. Must not be NULL.
 *
 * Returns SYS_OK or SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_crypto_sha256(const void *data,
                                size_t length,
                                uint8_t out[HAL_CRYPTO_SHA256_LEN]);

/**
 * Generate an Ed25519 keypair.
 *
 * @param priv Receives 32 bytes of private key. Must not be NULL.
 * @param pub  Receives 32 bytes of public key. Must not be NULL.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, or SYS_ERR_NOT_SUPPORTED when
 * the backend has no Ed25519 provider.
 */
sys_status_t hal_crypto_ed25519_keygen(uint8_t priv[HAL_CRYPTO_ED25519_PRIV_LEN],
                                        uint8_t pub[HAL_CRYPTO_ED25519_PUB_LEN]);

/**
 * Sign a message with an Ed25519 private key.
 *
 * @param priv    32-byte private key.
 * @param pub     32-byte public key. May be NULL if the backend can
 *                derive it from the private key.
 * @param message Message bytes.
 * @param length  Message length.
 * @param sig     Receives 64 bytes of signature. Must not be NULL.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, or SYS_ERR_NOT_SUPPORTED.
 */
sys_status_t hal_crypto_ed25519_sign(const uint8_t *priv,
                                      const uint8_t *pub,
                                      const void *message,
                                      size_t length,
                                      uint8_t sig[HAL_CRYPTO_ED25519_SIG_LEN]);

/**
 * Verify an Ed25519 signature.
 *
 * @param pub     32-byte public key.
 * @param message Message bytes.
 * @param length  Message length.
 * @param sig     64-byte signature.
 *
 * Returns SYS_OK when the signature is valid, SYS_ERR_INVALID_ARG when
 * the arguments are unusable, SYS_ERR_NOT_SUPPORTED when the backend
 * has no Ed25519 provider, and SYS_ERR_IO when the signature does not
 * verify.
 */
sys_status_t hal_crypto_ed25519_verify(const uint8_t *pub,
                                        const void *message,
                                        size_t length,
                                        const uint8_t sig[HAL_CRYPTO_ED25519_SIG_LEN]);

/**
 * @return true when this backend can perform Ed25519 operations.
 */
bool hal_crypto_ed25519_available(void);

/**
 * Constant-time comparison of two buffers.
 *
 * @return true when the buffers are equal.
 *
 * Used to compare MAC addresses, digests and tokens without leaking
 * their contents through timing.
 */
bool hal_crypto_equal(const void *a, const void *b, size_t length);

/**
 * Lowercase hex encoding.
 *
 * @param in     Input bytes.
 * @param length Input length.
 * @param out    Destination buffer; needs 2*length+1 bytes.
 *
 * @return the number of hex characters written (excluding the NUL).
 */
size_t hal_crypto_to_hex(const void *in, size_t length, char *out);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_HAL_CRYPTO_H */
