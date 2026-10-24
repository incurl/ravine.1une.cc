/*
 * hal_network_esp32.c — ESP32-S3 networking backend.
 *
 * Implements the Wi-Fi client path over ESP-IDF's esp_wifi / esp_netif
 * stack, plus HTTPS requests through esp_http_client.
 *
 * Radio modes
 * -----------
 * The radio carries exactly one mode at a time. Station and access
 * point are mutually exclusive, matching the Wi-Fi glossary entry in
 * docs/DESIGN.md. Switching modes stops the previous one first.
 *
 * The radio is off by default: hal_net_init() brings up the event loop
 * and the network stack but does not start Wi-Fi. The caller must
 * explicitly select a mode, which keeps the "opt in" behaviour that
 * DESIGN.md requires of every Wi-Fi-backed feature.
 *
 * Certificate pinning
 * -------------------
 * DESIGN.md section 14.9 requires the device to pin the server's leaf
 * certificate by SHA-256 so that a compromised CA cannot impersonate
 * the server.
 *
 * ESP-IDF v6.1's esp_http_client does not expose a public
 * certificate-verify callback, so a raw leaf-hash comparison cannot be
 * installed from outside the SDK. The strongest control available
 * through the public API is a pinned CA PEM (config.cert_pem), which
 * makes the client verify the server chain against exactly one
 * trusted anchor rather than against the system store.
 *
 * This backend therefore:
 *   - refuses an HTTPS request when no trust anchor has been set at
 *     all, rather than falling back to the system CA bundle;
 *   - accepts a pinned CA PEM via hal_net_set_ca_pem();
 *   - records a leaf-hash pin via hal_net_set_pinned_cert_sha256() and
 *     reports it, but documents that the hash comparison itself needs
 *     the verify callback that IDF does not expose.
 *
 * TODO(hal-network): implement true leaf-hash pinning by either
 *   (a) installing a custom esp_tls verify callback through the
 *       esp_http_client transport configuration, or
 *   (b) moving server calls to a bespoke esp_tls_conn_new() path.
 *   Until then, the pinned CA PEM is the operative trust anchor.
 *
 * Threading
 * ---------
 * The request path blocks and runs on the caller's task. It must never
 * run on the real-time audio task (AMY renders at
 * ESP_TASK_PRIO_MAX-1 on core 0) or in an ISR.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_ESP32S3)

#include "hal/hal_network.h"

#include "boards/synth_board.h"
#include "hal/hal_log.h"

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

static const char *TAG = "hal_net";

/* Bits for the event group the Wi-Fi driver signals on. */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT    BIT1

/* How long hal_net_sta_connect() waits for an association. */
#define STA_CONNECT_TIMEOUT_MS 15000

static bool               s_initialised;
static bool               s_event_loop_created;
static bool               s_sta_netif_created;
static bool               s_ap_netif_created;
static hal_net_mode_t     s_mode = HAL_NET_MODE_OFF;
static hal_net_state_t    s_state = HAL_NET_STATE_OFF;
static EventGroupHandle_t s_events;
static esp_netif_t       *s_sta_netif;
static esp_netif_t       *s_ap_netif;

/* Trust anchors. */
static const char *s_ca_pem;             /* pinned CA PEM, or NULL */
static uint8_t     s_pinned_sha256[32];  /* recorded leaf fingerprint */
static bool        s_has_pin;

/* ─── Event handling ────────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        s_state = HAL_NET_STATE_IDLE;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_state = HAL_NET_STATE_IDLE;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        s_state = HAL_NET_STATE_CONNECTED;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START) {
        s_state = HAL_NET_STATE_CONNECTED;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    }
}

/* ─── Init ──────────────────────────────────────────────────────── */

sys_status_t hal_net_init(void)
{
    if (s_initialised) {
        return SYS_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        HAL_LOG_ERROR(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
        return SYS_ERR_IO;
    }

    if (!s_event_loop_created) {
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            HAL_LOG_ERROR(TAG, "event loop create failed: %s",
                          esp_err_to_name(err));
            return SYS_ERR_IO;
        }
        s_event_loop_created = true;
    }

    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return SYS_ERR_NO_MEM;
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) {
        HAL_LOG_ERROR(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return SYS_ERR_IO;
    }

    /* Keep the Wi-Fi calibration data in RAM rather than re-reading it
     * from flash on every boot. */
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return SYS_ERR_IO;
    }
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        return SYS_ERR_IO;
    }

    s_initialised = true;
    s_state       = HAL_NET_STATE_OFF;
    s_mode        = HAL_NET_MODE_OFF;

    HAL_LOG_INFO(TAG, "network HAL ready (radio off; call hal_net_set_mode)");
    return SYS_OK;
}

sys_status_t hal_net_deinit(void)
{
    if (!s_initialised) {
        return SYS_OK;
    }

    esp_wifi_stop();
    if (s_sta_netif != NULL) {
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
        s_sta_netif_created = false;
    }
    if (s_ap_netif != NULL) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
        s_ap_netif_created = false;
    }
    esp_wifi_deinit();

    if (s_events != NULL) {
        vEventGroupDelete(s_events);
        s_events = NULL;
    }

    s_initialised = false;
    s_mode        = HAL_NET_MODE_OFF;
    s_state       = HAL_NET_STATE_OFF;
    return SYS_OK;
}

/* ─── Mode handling ─────────────────────────────────────────────── */

/* Tear down whichever mode is currently active. */
static void stop_current_mode(void)
{
    if (s_mode == HAL_NET_MODE_STA) {
        esp_wifi_disconnect();
    }
    esp_wifi_stop();
    s_mode  = HAL_NET_MODE_OFF;
    s_state = HAL_NET_STATE_OFF;
}

sys_status_t hal_net_set_mode(hal_net_mode_t mode)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (mode == s_mode) {
        return SYS_OK;
    }

    stop_current_mode();

    switch (mode) {
    case HAL_NET_MODE_OFF:
        return SYS_OK;

    case HAL_NET_MODE_STA:
        if (!s_sta_netif_created) {
            s_sta_netif = esp_netif_create_default_wifi_sta();
            if (s_sta_netif == NULL) {
                return SYS_ERR_IO;
            }
            s_sta_netif_created = true;
        }
        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
            return SYS_ERR_IO;
        }
        break;

    case HAL_NET_MODE_AP:
        if (!s_ap_netif_created) {
            s_ap_netif = esp_netif_create_default_wifi_ap();
            if (s_ap_netif == NULL) {
                return SYS_ERR_IO;
            }
            s_ap_netif_created = true;
        }
        if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
            return SYS_ERR_IO;
        }
        break;

    default:
        return SYS_ERR_INVALID_ARG;
    }

    s_mode  = mode;
    s_state = HAL_NET_STATE_IDLE;
    return SYS_OK;
}

hal_net_mode_t hal_net_get_mode(void)
{
    return s_mode;
}

hal_net_state_t hal_net_get_state(void)
{
    return s_state;
}

/* ─── Station mode ──────────────────────────────────────────────── */

sys_status_t hal_net_sta_connect(const char *ssid, const char *pass)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (ssid == NULL || ssid[0] == '\0') {
        return SYS_ERR_INVALID_ARG;
    }

    if (s_mode != HAL_NET_MODE_STA) {
        sys_status_t st = hal_net_set_mode(HAL_NET_MODE_STA);
        if (st != SYS_OK) {
            return st;
        }
    }

    wifi_config_t cfg = { 0 };
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    if (pass != NULL && pass[0] != '\0') {
        strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    cfg.sta.pmf_cfg.capable = true;

    if (esp_wifi_set_config(WIFI_IF_STA, &cfg) != ESP_OK) {
        return SYS_ERR_IO;
    }

    xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);

    if (esp_wifi_start() != ESP_OK) {
        return SYS_ERR_IO;
    }

    s_state = HAL_NET_STATE_CONNECTING;

    if (esp_wifi_connect() != ESP_OK) {
        s_state = HAL_NET_STATE_FAILED;
        return SYS_ERR_IO;
    }

    EventBits_t bits = xEventGroupWaitBits(s_events,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(STA_CONNECT_TIMEOUT_MS));

    if ((bits & WIFI_CONNECTED_BIT) == 0) {
        s_state = HAL_NET_STATE_FAILED;
        HAL_LOG_ERROR(TAG, "association with '%s' timed out", ssid);
        return SYS_ERR_TIMEOUT;
    }

    HAL_LOG_INFO(TAG, "connected to '%s'", ssid);
    return SYS_OK;
}

sys_status_t hal_net_sta_disconnect(void)
{
    if (!s_initialised || s_mode != HAL_NET_MODE_STA) {
        return SYS_OK;
    }
    esp_wifi_disconnect();
    s_state = HAL_NET_STATE_IDLE;
    return SYS_OK;
}

sys_status_t hal_net_ap_start(const char *ssid, const char *pass, uint8_t channel)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (ssid == NULL || ssid[0] == '\0') {
        return SYS_ERR_INVALID_ARG;
    }
    if (channel < 1 || channel > 13) {
        return SYS_ERR_INVALID_ARG;
    }

    if (s_mode != HAL_NET_MODE_AP) {
        sys_status_t st = hal_net_set_mode(HAL_NET_MODE_AP);
        if (st != SYS_OK) {
            return st;
        }
    }

    wifi_config_t cfg = { 0 };
    strncpy((char *)cfg.ap.ssid, ssid, sizeof(cfg.ap.ssid) - 1);
    cfg.ap.ssid_len = (uint8_t)strlen(ssid);
    cfg.ap.channel = channel;
    cfg.ap.max_connection = 4;

    if (pass != NULL && pass[0] != '\0') {
        strncpy((char *)cfg.ap.password, pass, sizeof(cfg.ap.password) - 1);
        cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    if (esp_wifi_set_config(WIFI_IF_AP, &cfg) != ESP_OK) {
        return SYS_ERR_IO;
    }
    if (esp_wifi_start() != ESP_OK) {
        return SYS_ERR_IO;
    }

    HAL_LOG_INFO(TAG, "AP '%s' started on channel %u", ssid, (unsigned)channel);
    return SYS_OK;
}

sys_status_t hal_net_ap_stop(void)
{
    if (!s_initialised || s_mode != HAL_NET_MODE_AP) {
        return SYS_OK;
    }
    stop_current_mode();
    return SYS_OK;
}

/* ─── Info ──────────────────────────────────────────────────────── */

sys_status_t hal_net_get_info(hal_net_info_t *out)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return SYS_ERR_IO;
    }

    if (mode == WIFI_MODE_STA) {
        /* IDF v6.1 has no esp_wifi_get_sta_info(); the SSID of the
         * associated AP is read back from the stored STA config. */
        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
            strncpy(out->ssid, (const char *)cfg.sta.ssid,
                    sizeof(out->ssid) - 1);
        }
        if (s_sta_netif != NULL) {
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(s_sta_netif, &ip) == ESP_OK &&
                ip.ip.addr != 0) {
                snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&ip.ip));
            }
        }
        if (esp_wifi_get_mac(WIFI_IF_STA, out->mac) != ESP_OK) {
            memset(out->mac, 0, sizeof(out->mac));
        }
    } else if (mode == WIFI_MODE_AP) {
        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_AP, &cfg) == ESP_OK) {
            strncpy(out->ssid, (const char *)cfg.ap.ssid,
                    sizeof(out->ssid) - 1);
            out->channel = cfg.ap.channel;
        }
        if (esp_wifi_get_mac(WIFI_IF_AP, out->mac) != ESP_OK) {
            memset(out->mac, 0, sizeof(out->mac));
        }
        if (s_ap_netif != NULL) {
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(s_ap_netif, &ip) == ESP_OK &&
                ip.ip.addr != 0) {
                snprintf(out->ip, sizeof(out->ip), IPSTR, IP2STR(&ip.ip));
            }
        }
    }

    return SYS_OK;
}

/* ─── Trust anchors ─────────────────────────────────────────────── */

sys_status_t hal_net_set_pinned_cert_sha256(const uint8_t sha256[32])
{
    if (sha256 == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    memcpy(s_pinned_sha256, sha256, sizeof(s_pinned_sha256));
    s_has_pin = true;

    /* The fingerprint is recorded so that the application layer can
     * report it, but ESP-IDF v6.1 gives no public hook to compare it
     * during the handshake. See the header note and the TODO above: the
     * operative trust anchor is the pinned CA PEM. */
    HAL_LOG_INFO(TAG, "leaf fingerprint pin recorded (enforcement pending)");
    return SYS_OK;
}

sys_status_t hal_net_set_ca_pem(const char *pem, size_t pem_length)
{
    if (pem == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (pem_length == 0) {
        pem_length = strlen(pem);
    }
    if (pem_length == 0) {
        return SYS_ERR_INVALID_ARG;
    }
    s_ca_pem = pem;
    HAL_LOG_INFO(TAG, "pinned CA configured (%u bytes)", (unsigned)pem_length);
    return SYS_OK;
}

/* ─── HTTP ──────────────────────────────────────────────────────── */

/* Response accumulation callback for esp_http_client. */
typedef struct {
    char  *buf;
    size_t capacity;
    size_t length;
} resp_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data != NULL) {
        resp_ctx_t *ctx = (resp_ctx_t *)evt->user_data;

        /* esp_http_client may deliver the body in several chunks. */
        int copy = evt->data_len;
        if (copy < 0) {
            copy = 0;
        }
        if (ctx->buf != NULL && ctx->capacity > 0) {
            size_t room = ctx->capacity - 1 - ctx->length;
            size_t take = ((size_t)copy < room) ? (size_t)copy : room;
            if (take > 0) {
                memcpy(ctx->buf + ctx->length, evt->data, take);
                ctx->length += take;
                ctx->buf[ctx->length] = '\0';
            }
        }
    }
    return ESP_OK;
}

sys_status_t hal_net_http_request(hal_net_http_method_t method,
                                   const char *url,
                                   const hal_net_http_header_t *headers,
                                   size_t header_count,
                                   const void *body,
                                   size_t body_length,
                                   char *resp,
                                   size_t resp_capacity,
                                   int32_t *out_status)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (url == NULL) {
        return SYS_ERR_INVALID_ARG;
    }

    const bool is_https = (strncmp(url, "https://", 8) == 0);

    /* Never fall back to the system CA bundle. Either a pinned CA is
     * configured, or the request is refused. */
    if (is_https && s_ca_pem == NULL) {
        HAL_LOG_ERROR(TAG, "refusing HTTPS request with no pinned CA");
        return SYS_ERR_INVALID_STATE;
    }

    esp_http_client_config_t cfg = {
        .url           = url,
        .method        = (method == HAL_NET_HTTP_POST)  ? HTTP_METHOD_POST
                        : (method == HAL_NET_HTTP_PUT)   ? HTTP_METHOD_PUT
                        : (method == HAL_NET_HTTP_DELETE)? HTTP_METHOD_DELETE
                                                        : HTTP_METHOD_GET,
        .timeout_ms    = 15000,
        .event_handler = http_event_handler,
        .transport_type = is_https ? HTTP_TRANSPORT_OVER_SSL
                                   : HTTP_TRANSPORT_OVER_TCP,
    };
    if (is_https && s_ca_pem != NULL) {
        cfg.cert_pem = s_ca_pem;
    }

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return SYS_ERR_IO;
    }

    resp_ctx_t ctx = { .buf = resp, .capacity = resp_capacity, .length = 0 };
    if (resp != NULL && resp_capacity > 0) {
        resp[0] = '\0';
    }
    esp_http_client_set_user_data(client, &ctx);

    for (size_t i = 0; i < header_count; i++) {
        if (headers[i].key != NULL && headers[i].value != NULL) {
            esp_http_client_set_header(client, headers[i].key, headers[i].value);
        }
    }

    if (body != NULL && body_length > 0) {
        esp_http_client_set_post_field(client, (const char *)body, (int)body_length);
    }

    esp_err_t err = esp_http_client_perform(client);

    if (out_status != NULL) {
        *out_status = (int32_t)esp_http_client_get_status_code(client);
    }

    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        HAL_LOG_ERROR(TAG, "http request failed: %s", esp_err_to_name(err));
        if (err == ESP_ERR_TIMEOUT) {
            return SYS_ERR_TIMEOUT;
        }
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

#endif /* BOARD_ESP32S3 */
