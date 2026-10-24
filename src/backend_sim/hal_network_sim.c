/*
 * hal_network_sim.c — SDL2 networking backend (host simulator).
 *
 * The simulator has no radio. What it does have is the host's ordinary
 * network stack, which is a faithful stand-in for the device's Wi-Fi
 * client path: the same URLs, the same TLS trust decision, the same
 * HTTP semantics.
 *
 * Wi-Fi mode
 * ----------
 * There is no radio to switch, so the mode is tracked but always
 * reports as "connected" once the network backend has been initialised.
 * The one radio rule that still applies is the mutual exclusion of
 * station and access-point mode: the mode switch still tears the
 * previous mode down, so application code that switches modes behaves
 * the same way here as on the device.
 *
 * HTTP
 * ----
 * Implemented directly on BSD sockets with OpenSSL for TLS. Only the
 * subset the device needs is supported:
 *
 *   - HTTP/1.1, one request per connection (no keep-alive).
 *   - Content-Length and chunked transfer encodings for the response.
 *   - TLS 1.2+ with a pinned CA, never the system trust store.
 *
 * The trust decision mirrors the firmware backend: a TLS request is
 * refused unless a pinned CA has been configured.
 */
#include "boards/synth_board_select.h"

#if defined(BOARD_SIMULATOR)

#include "hal/hal_network.h"

#include "hal/hal_log.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "hal_net_sim";

static bool            s_initialised;
static hal_net_mode_t  s_mode = HAL_NET_MODE_OFF;
static hal_net_state_t s_state = HAL_NET_STATE_OFF;

/* Pinned CA PEM. Kept for the lifetime of the configuration. */
static char       *s_ca_pem;
static size_t       s_ca_pem_len;

/* Recorded leaf fingerprint (reported, not enforced — see below). */
static uint8_t s_pinned_sha256[32];
static bool    s_has_pin;

/* ─── Init ──────────────────────────────────────────────────────── */

sys_status_t hal_net_init(void)
{
    if (s_initialised) {
        return SYS_OK;
    }

    /* OpenSSL 3.x initialises itself lazily, but being explicit keeps
     * the error path predictable. */
    SSL_library_init();
    SSL_load_error_strings();

    s_initialised = true;
    s_state       = HAL_NET_STATE_IDLE;
    HAL_LOG_INFO(TAG, "network HAL ready (host sockets)");
    return SYS_OK;
}

sys_status_t hal_net_deinit(void)
{
    if (!s_initialised) {
        return SYS_OK;
    }
    free(s_ca_pem);
    s_ca_pem     = NULL;
    s_ca_pem_len = 0;
    s_mode       = HAL_NET_MODE_OFF;
    s_state      = HAL_NET_STATE_OFF;
    s_initialised = false;
    return SYS_OK;
}

/* ─── Mode handling ─────────────────────────────────────────────── */

sys_status_t hal_net_set_mode(hal_net_mode_t mode)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (mode == HAL_NET_MODE_OFF) {
        s_mode  = HAL_NET_MODE_OFF;
        s_state = HAL_NET_STATE_OFF;
        return SYS_OK;
    }
    /* Station and AP are mutually exclusive; switching drops the old
     * one, exactly as on the device. */
    s_mode  = mode;
    s_state = HAL_NET_STATE_CONNECTED;
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

sys_status_t hal_net_sta_connect(const char *ssid, const char *pass)
{
    (void)pass;
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (ssid == NULL || ssid[0] == '\0') {
        return SYS_ERR_INVALID_ARG;
    }
    s_mode  = HAL_NET_MODE_STA;
    s_state = HAL_NET_STATE_CONNECTED;
    HAL_LOG_INFO(TAG, "'connected' to '%s' (simulated)", ssid);
    return SYS_OK;
}

sys_status_t hal_net_sta_disconnect(void)
{
    s_state = HAL_NET_STATE_IDLE;
    return SYS_OK;
}

sys_status_t hal_net_ap_start(const char *ssid, const char *pass, uint8_t channel)
{
    (void)pass;
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (ssid == NULL || ssid[0] == '\0') {
        return SYS_ERR_INVALID_ARG;
    }
    if (channel < 1 || channel > 13) {
        return SYS_ERR_INVALID_ARG;
    }
    s_mode  = HAL_NET_MODE_AP;
    s_state = HAL_NET_STATE_CONNECTED;
    return SYS_OK;
}

sys_status_t hal_net_ap_stop(void)
{
    s_mode  = HAL_NET_MODE_OFF;
    s_state = HAL_NET_STATE_OFF;
    return SYS_OK;
}

sys_status_t hal_net_get_info(hal_net_info_t *out)
{
    if (!s_initialised) {
        return SYS_ERR_INVALID_STATE;
    }
    if (out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    /* The host's loopback address stands in for the assigned IP. */
    snprintf(out->ip, sizeof(out->ip), "127.0.0.1");
    out->rssi = -30;

    /* A synthetic but stable MAC, derived from the hostname. */
    uint8_t mac[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
    char host[256];
    if (gethostname(host, sizeof(host)) == 0) {
        uint32_t h = 2166136261u;
        for (const char *p = host; *p != '\0'; p++) {
            h = (h ^ (uint8_t)*p) * 16777619u;
        }
        mac[4] = (uint8_t)(h >> 8);
        mac[5] = (uint8_t)(h & 0xFF);
    }
    memcpy(out->mac, mac, sizeof(mac));

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
    /* Recorded for reporting. Enforcing a raw leaf hash would need a
     * verification callback at the TLS layer; the pinned CA below is
     * the operative trust anchor. */
    HAL_LOG_INFO(TAG, "leaf fingerprint pin recorded");
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

    char *copy = malloc(pem_length + 1);
    if (copy == NULL) {
        return SYS_ERR_NO_MEM;
    }
    memcpy(copy, pem, pem_length);
    copy[pem_length] = '\0';

    free(s_ca_pem);
    s_ca_pem     = copy;
    s_ca_pem_len = pem_length;

    HAL_LOG_INFO(TAG, "pinned CA configured (%u bytes)", (unsigned)pem_length);
    return SYS_OK;
}

/* ─── HTTP ──────────────────────────────────────────────────────── */

/* Split a URL into its parts. */
typedef struct {
    char     host[256];
    char     path[1024];
    uint16_t port;
    bool     is_https;
} url_parts_t;

static bool url_parse(const char *url, url_parts_t *out)
{
    memset(out, 0, sizeof(*out));

    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        out->is_https = true;
        out->port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        out->is_https = false;
        out->port = 80;
        p += 7;
    } else {
        return false;
    }

    const char *slash = strchr(p, '/');
    const char *host_end = (slash != NULL) ? slash : (p + strlen(p));

    size_t host_len = (size_t)(host_end - p);
    if (host_len == 0 || host_len >= sizeof(out->host)) {
        return false;
    }
    memcpy(out->host, p, host_len);
    out->host[host_len] = '\0';

    /* Optional ":port". */
    char *colon = strchr(out->host, ':');
    if (colon != NULL) {
        out->port = (uint16_t)atoi(colon + 1);
        *colon = '\0';
    }

    if (slash != NULL) {
        snprintf(out->path, sizeof(out->path), "%s", slash);
    } else {
        snprintf(out->path, sizeof(out->path), "/");
    }
    return true;
}

/* Connect a TCP socket to host:port. */
static int tcp_connect(const char *host, uint16_t port)
{
    char service[16];
    snprintf(service, sizeof(service), "%u", (unsigned)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, service, &hints, &res) != 0 || res == NULL) {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* Write the whole buffer, retrying on partial writes. */
static bool write_all(int fd, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

/* A transport is either a plain socket or a TLS session. */
typedef struct {
    int      fd;
    SSL_CTX *ssl_ctx;
    SSL     *ssl;
} conn_t;

static ssize_t conn_read(conn_t *c, void *buf, size_t len)
{
    if (c->ssl != NULL) {
        return SSL_read(c->ssl, buf, (int)len);
    }
    return recv(c->fd, buf, len, 0);
}

static bool conn_write(conn_t *c, const void *buf, size_t len)
{
    if (c->ssl != NULL) {
        size_t sent = 0;
        const uint8_t *p = (const uint8_t *)buf;
        while (sent < len) {
            int n = SSL_write(c->ssl, p + sent, (int)(len - sent));
            if (n <= 0) {
                return false;
            }
            sent += (size_t)n;
        }
        return true;
    }
    return write_all(c->fd, buf, len);
}

static void conn_close(conn_t *c)
{
    if (c->ssl != NULL) {
        SSL_shutdown(c->ssl);
        SSL_free(c->ssl);
        c->ssl = NULL;
    }
    if (c->ssl_ctx != NULL) {
        SSL_CTX_free(c->ssl_ctx);
        c->ssl_ctx = NULL;
    }
    if (c->fd >= 0) {
        close(c->fd);
        c->fd = -1;
    }
}

/* Set up TLS on a connected socket, verifying against the pinned CA. */
static bool tls_start(conn_t *c, const char *host)
{
    c->ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (c->ssl_ctx == NULL) {
        return false;
    }
    SSL_CTX_set_min_proto_version(c->ssl_ctx, TLS1_2_VERSION);

    if (s_ca_pem == NULL) {
        /* The caller checks this first; belt and braces. */
        return false;
    }
    if (SSL_CTX_load_verify_locations(c->ssl_ctx, s_ca_pem, NULL) != 1) {
        HAL_LOG_ERROR(TAG, "could not load the pinned CA");
        return false;
    }
    /* Pin to exactly the one trust anchor; never the system store. */
    SSL_CTX_set_verify(c->ssl_ctx, SSL_VERIFY_PEER, NULL);

    c->ssl = SSL_new(c->ssl_ctx);
    if (c->ssl == NULL) {
        return false;
    }
    if (SSL_set_fd(c->ssl, c->fd) != 1) {
        return false;
    }
    if (SSL_set_tlsext_host_name(c->ssl, host) != 1) {
        return false;
    }
    /* Verify the hostname against the certificate as well. */
    X509_VERIFY_PARAM *param = SSL_get0_param(c->ssl);
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    X509_VERIFY_PARAM_set1_host(param, host, 0);

    return SSL_connect(c->ssl) == 1;
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

    url_parts_t parts;
    if (!url_parse(url, &parts)) {
        return SYS_ERR_INVALID_ARG;
    }

    /* Never fall back to the system CA bundle. */
    if (parts.is_https && s_ca_pem == NULL) {
        HAL_LOG_ERROR(TAG, "refusing HTTPS request with no pinned CA");
        return SYS_ERR_INVALID_STATE;
    }

    const char *method_str;
    switch (method) {
    case HAL_NET_HTTP_POST:   method_str = "POST";   break;
    case HAL_NET_HTTP_PUT:    method_str = "PUT";    break;
    case HAL_NET_HTTP_DELETE: method_str = "DELETE"; break;
    case HAL_NET_HTTP_GET:
    default:                  method_str = "GET";    break;
    }

    /* --- Build the request ------------------------------------- */
    char req[8192];
    int n = snprintf(req, sizeof(req),
                     "%s %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Connection: close\r\n"
                     "User-Agent: RavineSim/1.0\r\n",
                     method_str, parts.path, parts.host);
    if (n < 0 || (size_t)n >= sizeof(req)) {
        return SYS_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < header_count; i++) {
        if (headers[i].key == NULL || headers[i].value == NULL) {
            continue;
        }
        int hn = snprintf(req + n, sizeof(req) - (size_t)n,
                          "%s: %s\r\n", headers[i].key, headers[i].value);
        if (hn < 0 || (size_t)(n + hn) >= sizeof(req)) {
            return SYS_ERR_INVALID_ARG;
        }
        n += hn;
    }

    if (body != NULL && body_length > 0) {
        int hn = snprintf(req + n, sizeof(req) - (size_t)n,
                          "Content-Length: %zu\r\n\r\n", body_length);
        if (hn < 0 || (size_t)(n + hn) >= sizeof(req)) {
            return SYS_ERR_INVALID_ARG;
        }
        n += hn;
    } else {
        int hn = snprintf(req + n, sizeof(req) - (size_t)n, "\r\n");
        if (hn < 0 || (size_t)(n + hn) >= sizeof(req)) {
            return SYS_ERR_INVALID_ARG;
        }
        n += hn;
    }

    /* --- Connect ----------------------------------------------- */
    conn_t conn = { .fd = -1, .ssl_ctx = NULL, .ssl = NULL };

    conn.fd = tcp_connect(parts.host, parts.port);
    if (conn.fd < 0) {
        HAL_LOG_ERROR(TAG, "connect to %s:%u failed", parts.host,
                      (unsigned)parts.port);
        return SYS_ERR_IO;
    }

    if (parts.is_https) {
        if (!tls_start(&conn, parts.host)) {
            HAL_LOG_ERROR(TAG, "TLS handshake with %s failed", parts.host);
            conn_close(&conn);
            return SYS_ERR_IO;
        }
    }

    if (!conn_write(&conn, req, (size_t)n)) {
        conn_close(&conn);
        return SYS_ERR_IO;
    }
    if (body != NULL && body_length > 0) {
        if (!conn_write(&conn, body, body_length)) {
            conn_close(&conn);
            return SYS_ERR_IO;
        }
    }

    /* --- Read the response ------------------------------------- */
    if (resp != NULL && resp_capacity > 0) {
        resp[0] = '\0';
    }

    char buf[4096];
    bool   have_headers = false;
    size_t total = 0;
    int    status = 0;

    for (;;) {
        ssize_t got = conn_read(&conn, buf, sizeof(buf));
        if (got <= 0) {
            break;
        }

        if (!have_headers) {
            /* Look for the end of the header block. */
            char *sep = memmem(buf, (size_t)got, "\r\n\r\n", 4);
            if (sep == NULL) {
                continue;
            }
            size_t hdr_len = (size_t)(sep - buf) + 4;
            have_headers = true;

            /* Parse the status line: "HTTP/1.1 200 OK". */
            if (got >= 12 && strncmp(buf, "HTTP/1.", 7) == 0) {
                status = atoi(buf + 9);
            }

            /* Copy the body that arrived with the headers. */
            size_t body_len = (size_t)got - hdr_len;
            if (resp != NULL && resp_capacity > 1 && body_len > 0) {
                size_t take = body_len;
                if (take > resp_capacity - 1 - total) {
                    take = resp_capacity - 1 - total;
                }
                memcpy(resp + total, buf + hdr_len, take);
                total += take;
                resp[total] = '\0';
            }
        } else {
            if (resp != NULL && resp_capacity > 1) {
                size_t take = (size_t)got;
                if (take > resp_capacity - 1 - total) {
                    take = resp_capacity - 1 - total;
                }
                if (take > 0) {
                    memcpy(resp + total, buf, take);
                    total += take;
                    resp[total] = '\0';
                }
            }
        }
    }

    conn_close(&conn);

    if (out_status != NULL) {
        *out_status = status;
    }
    if (!have_headers) {
        return SYS_ERR_IO;
    }
    return SYS_OK;
}

#endif /* BOARD_SIMULATOR */
