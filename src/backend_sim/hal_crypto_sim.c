/*
 * hal_crypto_sim.c — SDL2 crypto backend (host simulator).
 *
 * Uses OpenSSL 3.x, which provides both SHA-256 and Ed25519. This is
 * the backend that makes the sketch-upload signing flow (DESIGN.md
 * section 14.4) developable and testable on the host while the
 * firmware-side Ed25519 provider is still outstanding.
 *
 * Ed25519 key material
 * --------------------
 * The raw private key is a 32-byte seed (OpenSSL's native format for
 * Ed25519), and the raw public key is 32 bytes. Both are exactly what
 * the HAL exposes, so no conversion layer is needed at the interface.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "hal/hal_crypto.h"

#include "hal/hal_log.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <string.h>

static const char *TAG = "hal_crypto_sim";

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

    unsigned int out_len = 0;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        return SYS_ERR_NO_MEM;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, data, length) != 1 ||
        EVP_DigestFinal_ex(ctx, out, &out_len) != 1) {
        EVP_MD_CTX_free(ctx);
        HAL_LOG_ERROR(TAG, "sha256 failed");
        return SYS_ERR_IO;
    }
    EVP_MD_CTX_free(ctx);

    if (out_len != HAL_CRYPTO_SHA256_LEN) {
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

/* ─── Ed25519 ───────────────────────────────────────────────────── */

sys_status_t hal_crypto_ed25519_keygen(uint8_t priv[HAL_CRYPTO_ED25519_PRIV_LEN],
                                        uint8_t pub[HAL_CRYPTO_ED25519_PUB_LEN])
{
    if (priv == NULL || pub == NULL) {
        return SYS_ERR_INVALID_ARG;
    }

    /* The 32-byte seed is the raw private key for Ed25519. */
    if (RAND_bytes(priv, HAL_CRYPTO_ED25519_PRIV_LEN) != 1) {
        HAL_LOG_ERROR(TAG, "RAND_bytes failed");
        return SYS_ERR_IO;
    }

    /* Derive the public key from that exact seed, so that the pair is
     * guaranteed to be consistent with the private key that the sign
     * call will use. */
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                                  priv, HAL_CRYPTO_ED25519_PRIV_LEN);
    if (pkey == NULL) {
        HAL_LOG_ERROR(TAG, "could not load the Ed25519 seed");
        return SYS_ERR_IO;
    }

    size_t pub_len = HAL_CRYPTO_ED25519_PUB_LEN;
    if (EVP_PKEY_get_raw_public_key(pkey, pub, &pub_len) != 1 ||
        pub_len != HAL_CRYPTO_ED25519_PUB_LEN) {
        EVP_PKEY_free(pkey);
        HAL_LOG_ERROR(TAG, "raw public key export failed");
        return SYS_ERR_IO;
    }
    EVP_PKEY_free(pkey);

    return SYS_OK;
}

sys_status_t hal_crypto_ed25519_sign(const uint8_t *priv,
                                      const uint8_t *pub,
                                      const void *message,
                                      size_t length,
                                      uint8_t sig[HAL_CRYPTO_ED25519_SIG_LEN])
{
    (void)pub;   /* the public key is derived from the seed */

    if (priv == NULL || sig == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (message == NULL && length > 0) {
        return SYS_ERR_INVALID_ARG;
    }

    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL,
                                                  priv, HAL_CRYPTO_ED25519_PRIV_LEN);
    if (pkey == NULL) {
        HAL_LOG_ERROR(TAG, "could not load the Ed25519 private key");
        return SYS_ERR_IO;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        EVP_PKEY_free(pkey);
        return SYS_ERR_NO_MEM;
    }

    size_t sig_len = HAL_CRYPTO_ED25519_SIG_LEN;
    bool ok = (EVP_DigestSignInit(ctx, NULL, NULL, NULL, pkey) == 1) &&
              (EVP_DigestSign(ctx, sig, &sig_len, message, length) == 1) &&
              (sig_len == HAL_CRYPTO_ED25519_SIG_LEN);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    if (!ok) {
        HAL_LOG_ERROR(TAG, "Ed25519 signing failed");
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

sys_status_t hal_crypto_ed25519_verify(const uint8_t *pub,
                                        const void *message,
                                        size_t length,
                                        const uint8_t sig[HAL_CRYPTO_ED25519_SIG_LEN])
{
    if (pub == NULL || sig == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (message == NULL && length > 0) {
        return SYS_ERR_INVALID_ARG;
    }

    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
                                                  pub, HAL_CRYPTO_ED25519_PUB_LEN);
    if (pkey == NULL) {
        return SYS_ERR_IO;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (ctx == NULL) {
        EVP_PKEY_free(pkey);
        return SYS_ERR_NO_MEM;
    }

    int rc = EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, pkey);
    int verified = 0;
    if (rc == 1) {
        verified = EVP_DigestVerify(ctx, sig, HAL_CRYPTO_ED25519_SIG_LEN,
                                    message, length);
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    /* verified == 1 means the signature matches; 0 means it does not.
     * Both non-1 results are reported as a verification failure. */
    return (verified == 1) ? SYS_OK : SYS_ERR_IO;
}

bool hal_crypto_ed25519_available(void)
{
    return true;
}

/* ─── Shared helpers ────────────────────────────────────────────── */

bool hal_crypto_equal(const void *a, const void *b, size_t length)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    /* CRYPTO_memcmp is constant-time. */
    return CRYPTO_memcmp(a, b, length) == 0;
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

#endif /* BOARD_SIMULATOR */

