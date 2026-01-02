/**
 * @file src/web_handlers/http_server.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-01-01
 *
 * @details Main HTTP server implementation using W5500 Ethernet controller.
 *          Handles all HTTP requests and routes to appropriate handlers.
 *          Uses spiMtx for thread-safe W5500 access.
 *
 * @note Due to size constraints of the preallocated W5500 TX buffer (8KB), and
 * the large size of HTML pages (some >8KB), this server implements chunked
 * sending of HTTP responses. For large pages, eg. help.html, the server uses
 * gzipped content stored in flash, which drastically reduces size and allows
 * fitting within TX buffer limits.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* Page content helpers implemented in page_content.c */
extern const char *get_page_content(const char *request);
extern int get_page_length(const char *request, int *is_gzip);

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

/* HTTP server configuration */
#define HTTP_PORT 80
#define HTTP_BUF_SIZE 2048
#define HTTP_SOCK_NUM 0

#define HTTP_SERVER_TAG "[SERVER]"

#define TX_WINDOW_MS 1200
#define MAX_SEND_CHUNK 4096 /* Send in 4KB chunks to stay safe within 8KB TX buffer */

/* Global variables for server state */
static int8_t http_sock;
static char *http_buf;

/**
 * @brief Send data in safe chunks within W5500 TX buffer capacity.
 *
 * Splits payload into up to `MAX_SEND_CHUNK` bytes per send to avoid
 * overflowing the 8KB W5500 TX buffer. Adds small delays between chunks
 * to allow hardware TX drain.
 *
 * @param sock Socket number.
 * @param data Pointer to data to send.
 * @param len Total length to send in bytes.
 * @return Number of bytes sent, or -1 on error.
 */
static int send_all(uint8_t sock, const uint8_t *data, int len) {
    int total_sent = 0;

    while (total_sent < len) {
        int remaining = len - total_sent;
        int to_send = (remaining > MAX_SEND_CHUNK) ? MAX_SEND_CHUNK : remaining;

        int sent = send(sock, (uint8_t *)(data + total_sent), (uint16_t)to_send);
        if (sent <= 0) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_SERVER, 0x0);
            ERROR_PRINT_CODE(errorcode, "%s Send failed on socket %u\r\n", HTTP_SERVER_TAG, sock);
            Storage_EnqueueErrorCode(errorcode);
#endif

            return -1;
        }

        total_sent += sent;
        net_beat();

        /* Small delay between chunks to allow TX buffer drain */
        if (total_sent < len) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    return total_sent;
}

/**
 * @brief Send a simple HTTP/1.1 response with headers and body.
 *
 * Emits headers with Content-Type and Content-Length, then the body.
 * Uses `send_all()` for chunked TX safety.
 *
 * @param sock Socket number.
 * @param content_type MIME type (e.g., "application/json").
 * @param body Pointer to response body (optional).
 * @param body_len Byte length of body (0 allowed).
 */
static void send_http_response(uint8_t sock, const char *content_type, const char *body,
                               int body_len) {
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %d\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Cache-Control: no-cache\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              content_type, body_len);
    if (header_len < 0)
        return;
    if (header_len > (int)sizeof(header))
        header_len = (int)sizeof(header);

    send_all(sock, (uint8_t *)header, header_len);

    if (body && body_len > 0) {
        send_all(sock, (const uint8_t *)body, body_len);
    }
}

/**
 * @brief Parse Content-Length from HTTP headers block.
 *
 * Scans CRLF-delimited header lines for "Content-Length:" and converts the value.
 *
 * @param headers Pointer to the header buffer.
 * @param header_len Number of bytes containing headers.
 * @return Parsed Content-Length (>0), or 0 if not found/invalid.
 */
static int parse_content_length(const char *headers, int header_len) {
    const char *p = headers;
    const char *end = headers + header_len;

    while (p < end) {
        const char *line_end = memchr(p, '\n', (size_t)(end - p));
        if (!line_end)
            line_end = end;

        if ((size_t)(line_end - p) > 16 && strncasecmp(p, "Content-Length:", 15) == 0) {
            const char *q = p + 15;
            while (q < line_end && (*q == ' ' || *q == '\t'))
                q++;
            long v = strtol(q, NULL, 10);
            if (v > 0 && v < (1 << 28))
                return (int)v;

            return 0;
        }

        p = line_end + 1;
    }

    return 0;
}

/**
 * @brief Read an HTTP request into buffer and locate body.
 *
 * Reads headers until CRLFCRLF, then reads body based on Content-Length.
 * Returns total bytes read and sets body offset/length outputs.
 *
 * @param sock Socket number.
 * @param buf Destination buffer for full request.
 * @param buflen Buffer capacity in bytes.
 * @param body_off Out: offset to start of body within `buf`.
 * @param body_len Out: number of bytes in body.
 * @return Total bytes read, or -1 if headers incomplete.
 */
static int read_http_request(uint8_t sock, char *buf, int buflen, int *body_off, int *body_len) {
    int total = 0;
    int header_end = -1;

    /* Read headers first */
    while (total < buflen - 1) {
        int n = recv(sock, (uint8_t *)buf + total, buflen - 1 - total);
        if (n <= 0)
            break;
        net_beat();
        total += n;
        buf[total] = '\0';

        /* Look for end of headers */
        char *p = strstr(buf, "\r\n\r\n");
        if (p) {
            header_end = (int)(p - buf) + 4;
            break;
        }
        net_beat();
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (header_end < 0) {
        return -1;
    }

    /* Read body according to Content-Length */
    int content_length = parse_content_length(buf, header_end);
    int have = total - header_end;

    while (have < content_length && total < buflen - 1) {
        int n = recv(sock, (uint8_t *)buf + total, buflen - 1 - total);
        if (n <= 0)
            break;
        net_beat();
        total += n;
        have += n;
        buf[total] = '\0';
        net_beat();
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    *body_off = header_end;
    *body_len = (have > 0) ? have : 0;
    return total;
}

/**
 * @brief Initialize and start the HTTP server (socket listen).
 *
 * Allocates RX buffer, opens TCP socket on port 80, enables keep-alive,
 * and begins listening. On failure, logs error and returns false.
 *
 * @return true on success; false on allocation/socket/listen failure.
 */
bool http_server_init(void) {
    /* Allocate HTTP buffer */
    http_buf = pvPortMalloc(HTTP_BUF_SIZE);
    if (!http_buf) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_SERVER, 0x3);
        ERROR_PRINT_CODE(errorcode, "%s HTTP buffer allocation failed\r\n", HTTP_SERVER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return false;
    }

    /* Open TCP socket for HTTP */
    http_sock = socket(HTTP_SOCK_NUM, Sn_MR_TCP, HTTP_PORT, 0);
    if (http_sock < 0) {
        vPortFree(http_buf);

#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_SERVER, 0x4);
        ERROR_PRINT_CODE(errorcode, "%s HTTP socket open failed\r\n", HTTP_SERVER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return false;
    }

    /* Enable TCP keep-alive auto timer (value is in 5 s units on W5500) */
    {
        uint8_t ka_units = (uint8_t)((W5500_KEEPALIVE_TIME + 4) / 5);
        if (ka_units) {
            setsockopt((uint8_t)http_sock, SO_KEEPALIVEAUTO, &ka_units);
        }
    }

    /* Start listening */
    if (listen(http_sock) != SOCK_OK) {
        closesocket(http_sock);
        vPortFree(http_buf);
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_SERVER, 0x5);
        ERROR_PRINT_CODE(errorcode, "%s HTTP listen failed\r\n", HTTP_SERVER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return false;
    }

    INFO_PRINT("%s Listening on port %d\n", HTTP_SERVER_TAG, HTTP_PORT);
    return true;
}

/**
 * @brief Process incoming HTTP connections and route requests.
 *
 * Handles GET/POST routes for status, settings, control, presets, apply/startup config,
 * and metrics. Serves static pages for other requests. Always closes connection
 * after responding and re-listens.
 */
void http_server_process(void) {
    /* TX progress watchdog for idle or stalled peers */
    static TickType_t s_last_tx_check = 0;
    static uint16_t s_last_fsr = 0;

    switch (getSn_SR(http_sock)) {

    case SOCK_ESTABLISHED: {
        if (getSn_IR(http_sock) & Sn_IR_CON)
            setSn_IR(http_sock, Sn_IR_CON);

        int body_off = 0, body_len = 0;
        int n = read_http_request(http_sock, http_buf, HTTP_BUF_SIZE, &body_off, &body_len);
        if (n <= 0) {
            /* Guard: if TX FSR is stuck and peer is not reading, reap the socket */
            uint16_t fsr = getSn_TX_FSR(http_sock);
            uint16_t fmax = getSn_TxMAX(http_sock);
            TickType_t now = xTaskGetTickCount();

            if (fsr == s_last_fsr && fsr != fmax) {
                if ((now - s_last_tx_check) >= pdMS_TO_TICKS(TX_WINDOW_MS)) {
                    disconnect(http_sock);
                    closesocket(http_sock);
                    http_sock = socket(HTTP_SOCK_NUM, Sn_MR_TCP, HTTP_PORT, 0);
                    if ((int)http_sock >= 0)
                        listen(http_sock);
                    s_last_tx_check = now;
                    s_last_fsr = 0;
                }
            } else {
                s_last_tx_check = now;
                s_last_fsr = fsr;
            }
            break;
        }

        http_buf[n] = '\0';
        metrics_inc_http_requests();
        char *body_ptr = http_buf + ((body_off > 0 && body_off < HTTP_BUF_SIZE) ? body_off : n);
        if (body_ptr + body_len > http_buf + HTTP_BUF_SIZE)
            body_len = (http_buf + HTTP_BUF_SIZE) - body_ptr;
        if (body_len < 0)
            body_len = 0;
        body_ptr[body_len] = '\0';

        /* ==================== API Route Matching ==================== */

        /* Status API */
        if (!strncmp(http_buf, "GET /api/status", 15)) {
            handle_status_request(http_sock);
        }
        /* Settings API */
        else if (!strncmp(http_buf, "GET /api/settings", 17)) {
            handle_settings_api(http_sock);
        } else if (!strncmp(http_buf, "POST /api/settings", 18)) {
            handle_settings_post(http_sock, body_ptr);
        }
        /* Control API */
        else if (!strncmp(http_buf, "POST /api/control", 17)) {
            handle_control_request(http_sock, body_ptr);
        }
        /* Config Presets API */
        else if (!strncmp(http_buf, "GET /api/config-presets", 23)) {
            handle_config_presets_get(http_sock);
        } else if (!strncmp(http_buf, "POST /api/config-presets", 24)) {
            handle_config_presets_post(http_sock, body_ptr);
        }
        /* Apply Config API */
        else if (!strncmp(http_buf, "POST /api/apply-config", 22)) {
            handle_apply_config_post(http_sock, body_ptr);
        }
        /* Startup Config API */
        else if (!strncmp(http_buf, "POST /api/startup-config", 24)) {
            handle_startup_config_post(http_sock, body_ptr);
        }
        /* Metrics endpoint */
        else if (!strncmp(http_buf, "GET /metrics", 12)) {
            handle_metrics_request(http_sock);
        }
        /* Settings HTML page */
        else if (!strncmp(http_buf, "GET /settings.html", 18)) {
            handle_settings_request(http_sock);
        }
        /* Default: serve static pages */
        else {
            int is_gzip = 0;
            const char *page = get_page_content(http_buf);
            int plen = get_page_length(http_buf, &is_gzip);
            char header[256];
            int hlen;

            if (plen < 0)
                plen = 0;

            if (is_gzip) {
                hlen = snprintf(header, sizeof(header),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Encoding: gzip\r\n"
                                "Content-Length: %d\r\n"
                                "Access-Control-Allow-Origin: *\r\n"
                                "Cache-Control: no-cache\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                plen);
            } else {
                hlen = snprintf(header, sizeof(header),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/html\r\n"
                                "Content-Length: %d\r\n"
                                "Access-Control-Allow-Origin: *\r\n"
                                "Cache-Control: no-cache\r\n"
                                "Connection: close\r\n"
                                "\r\n",
                                plen);
            }
            int hsend = hlen;
            if (hsend < 0)
                hsend = 0;
            if (hsend > (int)sizeof(header))
                hsend = (int)sizeof(header);

            /* Send using chunked send to handle files larger than 8KB TX buffer */
            send_all(http_sock, (uint8_t *)header, hsend);
            if (plen > 0 && page != NULL) {
                send_all(http_sock, (const uint8_t *)page, plen);
            }
        }

        disconnect(http_sock);
        closesocket(http_sock);
        http_sock = socket(HTTP_SOCK_NUM, Sn_MR_TCP, HTTP_PORT, 0);
        if ((int)http_sock >= 0)
            listen(http_sock);
        break;
    }

    case SOCK_CLOSE_WAIT:
        disconnect(http_sock);
        closesocket(http_sock);
        http_sock = socket(HTTP_SOCK_NUM, Sn_MR_TCP, HTTP_PORT, 0);
        if ((int)http_sock >= 0)
            listen(http_sock);
        break;

    case SOCK_INIT:
        listen(http_sock);
        break;

    default:
        break;
    }
}