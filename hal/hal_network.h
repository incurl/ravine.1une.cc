/*
 * hal_network.h — wireless networking abstraction.
 *
 * Covers the device's Wi-Fi client path: joining a network, serving a
 * provisioning access point, and issuing HTTPS requests. This is the
 * platform seam for the server-mediated sketch upload and device
 * binding flows described in docs/DESIGN.md sections 14 and 15.
 *
 * Scope notes:
 *   - The radio has exactly one mode at a time. Station (client) mode
 *     and access-point (provisioning) mode are mutually exclusive; see
 *     the Wi-Fi entry in DESIGN.md's glossary. Switching modes tears
 *     the previous one down first.
 *   - Peer-to-peer ESP-NOW transfer (DESIGN.md section 15) is a
 *     different radio protocol with no IP layer. It is intentionally
 *     NOT part of this interface; it needs its own module.
 *   - This interface carries no audio. Sketch upload is a file
 *     transfer, never a live audio stream.
 *
 * Threading: the request calls block. They must not run on the
 * real-time audio task or in an ISR. The firmware backend performs the
 * network work on the caller's task, which is the UI task.
 */
#ifndef SYNTH_HAL_NETWORK_H
#define SYNTH_HAL_NETWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Radio mode. Exactly one is active at a time. */
typedef enum {
    HAL_NET_MODE_OFF = 0,   /* Radio disabled.                          */
    HAL_NET_MODE_STA,       /* Station: join an existing network.       */
    HAL_NET_MODE_AP         /* Access point: device-hosted provisioning. */
} hal_net_mode_t;

/** Connection state, as reported by the backend. */
typedef enum {
    HAL_NET_STATE_OFF = 0,
    HAL_NET_STATE_IDLE,          /* Radio up, not associated.         */
    HAL_NET_STATE_CONNECTING,
    HAL_NET_STATE_CONNECTED,
    HAL_NET_STATE_FAILED
} hal_net_state_t;

/** Current network parameters. */
typedef struct {
    char     ssid[33];
    char     ip[16];
    uint8_t  mac[6];
    int8_t   rssi;            /* dBm; 0 when not connected.             */
    uint8_t  channel;
} hal_net_info_t;

/** HTTP methods used by the device binding and upload flows. */
typedef enum {
    HAL_NET_HTTP_GET = 0,
    HAL_NET_HTTP_POST,
    HAL_NET_HTTP_PUT,
    HAL_NET_HTTP_DELETE
} hal_net_http_method_t;

/** One request header. */
typedef struct {
    const char *key;
    const char *value;
} hal_net_http_header_t;

/**
 * Initialise the networking backend.
 *
 * On the firmware this brings up the event loop, the network stack and
 * the Wi-Fi driver. On the host it prepares a socket layer.
 *
 * Returns SYS_OK or SYS_ERR_NO_MEM.
 */
sys_status_t hal_net_init(void);

/**
 * Shut the networking backend down and turn the radio off.
 */
sys_status_t hal_net_deinit(void);

/**
 * Select the radio mode, tearing down any previous mode first.
 *
 * HAL_NET_MODE_AP starts an access point using the supplied
 * credentials. HAL_NET_MODE_STA leaves the radio in disassociated idle
 * until hal_net_sta_connect() is called.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, or SYS_ERR_IO.
 */
sys_status_t hal_net_set_mode(hal_net_mode_t mode);

/** @return the current radio mode. */
hal_net_mode_t hal_net_get_mode(void);

/** @return the current connection state. */
hal_net_state_t hal_net_get_state(void);

/**
 * Join a network in station mode.
 *
 * Blocks until the association completes or the attempt fails.
 *
 * @param ssid  Network name.
 * @param pass  WPA2 passphrase; NULL or empty for an open network.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, SYS_ERR_TIMEOUT, or SYS_ERR_IO.
 */
sys_status_t hal_net_sta_connect(const char *ssid, const char *pass);

/**
 * Disassociate from the current network. Returns to idle.
 */
sys_status_t hal_net_sta_disconnect(void);

/**
 * Start an access point (provisioning mode).
 *
 * @param ssid    AP name.
 * @param pass    WPA2 passphrase, or NULL/empty for an open AP.
 * @param channel Wi-Fi channel, 1..13.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, or SYS_ERR_IO.
 */
sys_status_t hal_net_ap_start(const char *ssid, const char *pass, uint8_t channel);

/**
 * Stop the access point and return the radio to off.
 */
sys_status_t hal_net_ap_stop(void);

/**
 * Query the current network parameters.
 *
 * @param out Receives the information. Must not be NULL.
 *
 * Returns SYS_OK or SYS_ERR_INVALID_STATE.
 */
sys_status_t hal_net_get_info(hal_net_info_t *out);

/**
 * Pin the server's TLS certificate by its SHA-256 leaf fingerprint.
 *
 * The device never trusts the system CA store for server calls: a
 * pinned trust anchor is the only accepted basis for verification.
 *
 * @param sha256 32-byte fingerprint of the server's leaf certificate.
 *
 * Backend note: the ESP32 HTTP client in ESP-IDF v6.1 does not expose a
 * public certificate-verify callback, so the firmware backend cannot
 * enforce a raw leaf hash directly. It instead requires a pinned CA
 * PEM for the same trust decision, and refuses a request when no trust
 * anchor has been configured at all. See the implementation for the
 * full rationale.
 *
 * Returns SYS_OK or SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_net_set_pinned_cert_sha256(const uint8_t sha256[32]);

/**
 * Supply the pinned CA certificate (PEM) used to verify the server.
 *
 * The simulator backend uses this for its TLS verification.
 *
 * @param pem        PEM text, NUL-terminated. Must remain valid for the
 *                   lifetime of the configuration.
 * @param pem_length Length of @p pem, excluding the NUL. Pass 0 to
 *                   let the backend use strlen().
 *
 * Returns SYS_OK or SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_net_set_ca_pem(const char *pem, size_t pem_length);

/**
 * Issue an HTTP(S) request.
 *
 * @param method        Request method.
 * @param url           Absolute URL. "https://" is required for any
 *                      call that carries credentials.
 * @param headers       Optional array of headers; may be NULL.
 * @param header_count  Number of headers.
 * @param body          Optional request body; may be NULL.
 * @param body_length   Length of @p body.
 * @param resp          Optional response buffer; may be NULL.
 * @param resp_capacity Size of @p resp in bytes. The response is
 *                      truncated to fit and NUL-terminated.
 * @param out_status    Optional; receives the HTTP status code.
 *
 * The call blocks until the response completes or the transport times
 * out.
 *
 * Returns SYS_OK when the exchange completed (the HTTP status code may
 * still indicate an application-level error, which the caller inspects
 * through @p out_status), SYS_ERR_TIMEOUT, SYS_ERR_IO, or
 * SYS_ERR_INVALID_ARG.
 */
sys_status_t hal_net_http_request(hal_net_http_method_t method,
                                   const char *url,
                                   const hal_net_http_header_t *headers,
                                   size_t header_count,
                                   const void *body,
                                   size_t body_length,
                                   char *resp,
                                   size_t resp_capacity,
                                   int32_t *out_status);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_HAL_NETWORK_H */
