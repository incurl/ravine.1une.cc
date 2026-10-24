/*
 * hal_crypto_esp32.c — ESP32-S3 crypto backend.
 *
 * SHA-256
 * -------
 * Provided by mbedTLS, through the public PSA Crypto API
 * (psa/crypto.h), which is what ESP-IDF v6.1 exposes. The classic
 * mbedtls/sha256.h header is no longer a public header in this
 * layout, so the PSA one-shot psa_hash_compute() is used instead.
 *
 * Ed25519
 * -------
 * NOT AVAILABLE on this target, and the gap is a property of the
 * toolchain rather than of this design.
 *
 * ESP-IDF v6.1 ships mbedTLS 4.x, which restructured the library
 * around TF-PSA-Crypto. In that layout the classic
 * mbedtls_ed25519_keypair() / mbedtls_ed25519_sign() API is gone:
 *
 *   - There is no ed25519.h anywhere in the IDF tree.
 *   - The PSA algorithm PSA_ALG_PURE_EDDSA exists in the headers, but
 *     the PSA crypto configuration in this project's sdkconfig does
 *     not enable it, and the corresponding driver source is not
 *     compiled in. Only SHA-256 (PSA_WANT_ALG_SHA_256) is enabled.
 *
 * So the firmware backend returns SYS_ERR_NOT_SUPPORTED from the
 * Ed25519 calls rather than pretending to work. The interface is
 * unchanged, so application code compiles and runs identically on both
 * targets; it simply cannot complete a signature on the device yet.
 *
 * TODO(hal-crypto): provide an Ed25519 provider on the firmware. Two
 *   viable routes:
 *   (a) Rebuild the TF-PSA-Crypto configuration with
 *       PSA_WANT_ALG_PURE_EDDSA, PSA_WANT_ECC_ED25519 and
 *       PSA_WANT_KEY_TYPE_ECC_KEY_PAIR enabled, plus a key store that
 *       can hold an ECC key pair. This needs a custom
 *       TF_PSA_CRYPTO_CONFIG_FILE, which IDF exposes for exactly this
 *       purpose, and it adds flash.
 *   (b) Vendor a compact standalone Ed25519 implementation (e.g. the
 *       public-domain ed25519-donna or TweetNaCl-derived code) into
 *       the project. Roughly 2 KB of flash, no configuration change.
 *
 * Until either lands, the sketch-upload signing flow (DESIGN.md
 * section 14.4) cannot run on the device. The simulator backend
 * implements it fully via OpenSSL, so the flow can be developed and
 * tested on the host in the meantime.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_ESP32S3)

#include "hal/hal_crypto.h"

#include "hal/hal_log.h"

#include "psa/crypto.h"

#include <string.h>

static const char *TAG = "hal_crypto";

sys_status_t hal_crypto_sha256(const void *data,
                                size_t length,
                                uint8_t out[HAL_CRYPTO_SHA256_LEN])
{
    if (out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (data == NULL && length > 0) {
        return SYS_ERR_INVALID_ARG;
    }

    /* The PSA library must be initialised before its first use. Doing
     * it here keeps the caller's job to one function call. */
    static bool s_psa_ready;
    if (!s_psa_ready) {
        psa_status_t st = psa_crypto_init();
        if (st != PSA_SUCCESS) {
            HAL_LOG_ERROR(TAG, "psa_crypto_init failed: %ld", (long)st);
            return SYS_ERR_IO;
        }
        s_psa_ready = true;
    }

    psa_status_t st = psa_hash_compute(PSA_ALG_SHA_256,
                                       data, length,
                                       out, HAL_CRYPTO_SHA256_LEN,
                                       &length);
    if (st != PSA_SUCCESS) {
        HAL_LOG_ERROR(TAG, "sha256 failed: %ld", (long)st);
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

/* ─── Ed25519: unavailable on this target ───────────────────────── */

sys_status_t hal_crypto_ed25519_keygen(uint8_t priv[HAL_CRYPTO_ED25519_PRIV_LEN],
                                        uint8_t pub[HAL_CRYPTO_ED25519_PUB_LEN])
{
    (void)priv;
    (void)pub;
    HAL_LOG_WARN(TAG, "Ed25519 is not available on ESP-IDF v6.1 (see file header)");
    return SYS_ERR_NOT_SUPPORTED;
}

sys_status_t hal_crypto_ed25519_sign(const uint8_t *priv,
                                      const uint8_t *pub,
                                      const void *message,
                                      size_t length,
                                      uint8_t sig[HAL_CRYPTO_ED25519_SIG_LEN])
{
    (void)priv;
    (void)pub;
    (void)message;
    (void)length;
    (void)sig;
    return SYS_ERR_NOT_SUPPORTED;
}

sys_status_t hal_crypto_ed25519_verify(const uint8_t *pub,
                                        const void *message,
                                        size_t length,
                                        const uint8_t sig[HAL_CRYPTO_ED25519_SIG_LEN])
{
    (void)pub;
    (void)message;
    (void)length;
    (void)sig;
    return SYS_ERR_NOT_SUPPORTED;
}

bool hal_crypto_ed25519_available(void)
{
    return false;
}

/* ─── Shared helpers ────────────────────────────────────────────── */

bool hal_crypto_equal(const void *a, const void *b, size_t length)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    /* Constant-time compare: the loop must not exit early on the
     * first differing byte. */
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint8_t diff = 0;
    for (size_t i = 0; i < length; i++) {
        diff |= (uint8_t)(pa[i] ^ pb[i]);
    }
    return diff == 0;
}

size_t hal_crypto_to_hex(const void *in, size_t length, char *out)
{
    static const char digits[] = "0123456789abcdef";

    if (in == NULL || out == NULL) {
        return 0;
    }
    const uint8_t *p = (const uint8_t *)in;
    for (size_t i = 0; i < length; i++) {
        out[i * 2]     = digits[(p[i] >> 4) & 0x0F];
        out[i * 2 + 1] = digits[p[i] & 0x0F];
    }
    out[length * 2] = '\0';
    return length * 2;
}

#endif /* BOARD_ESP32S3 */
