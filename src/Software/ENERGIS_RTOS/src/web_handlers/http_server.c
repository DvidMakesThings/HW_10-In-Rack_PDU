/**
 * @file src/web_handlers/http_server.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-05-24
 *
 * @details Main HTTP server implementation using W5500 Ethernet controller.
 *          Handles all HTTP requests and routes to appropriate handlers.
 *          Uses spiMtx for thread-safe W5500 access.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

/* HTTP server configuration */
#define HTTP_PORT 80
#define HTTP_BUF_SIZE 2048
#define HTTP_SOCK_NUM 0

#define HTTP_SERVER_TAG "[SERVER]"

#define TX_WINDOW_MS 1200

/* Global variables for server state */
static int8_t http_sock;
static char *http_buf;

/**
 * @brief Sends HTTP response with headers and body
 * @param sock The socket number
 * @param content_type Content-Type header value
 * @param body The body of the response
 * @param body_len Length of the body
 * @return None
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

    send(sock, (uint8_t *)header, header_len);
    net_beat();
    if (body && body_len > 0) {
        send(sock, (uint8_t *)body, body_len);
        net_beat();
    }
}

/**
 * @brief Parses Content-Length from HTTP headers
 * @param headers Pointer to the start of the headers
 * @param header_len Length of the headers
 * @return Content-Length value, or 0 if not found
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
 * @brief Reads HTTP request from socket
 * @param sock The socket number
 * @param buf Buffer to store the request
 * @param buflen Buffer length
 * @param body_off Output parameter for body offset
 * @param body_len Output parameter for body length
 * @return Total bytes read, or -1 if headers incomplete
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

    if (header_end < 0)
        return -1;

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
 * @brief Initializes the HTTP server
 *
 * @return true on success, false on failure
 *
 * @details
 * - Allocates the server RX buffer.
 * - Opens a TCP socket and starts listening on HTTP_PORT.
 * - Enables TCP keep-alive timer so idle peers are culled automatically.
 */
bool http_server_init(void) {
    /* Allocate HTTP buffer */
    http_buf = pvPortMalloc(HTTP_BUF_SIZE);
    if (!http_buf) {
        ERROR_PRINT("%s Failed to allocate HTTP buffer\n", HTTP_SERVER_TAG);
        return false;
    }

    /* Open TCP socket for HTTP */
    http_sock = socket(HTTP_SOCK_NUM, Sn_MR_TCP, HTTP_PORT, 0);
    if (http_sock < 0) {
        ERROR_PRINT("%s Failed to open HTTP socket\n", HTTP_SERVER_TAG);
        vPortFree(http_buf);
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
        ERROR_PRINT("%s Failed to listen on HTTP socket\n", HTTP_SERVER_TAG);
        closesocket(http_sock);
        vPortFree(http_buf);
        return false;
    }

    INFO_PRINT("%s Listening on port %d\n", HTTP_SERVER_TAG, HTTP_PORT);
    return true;
}

/**
 * @brief Processes incoming HTTP requests
 *
 * @return None
 *
 * @details
 * - Routes GET and POST to handlers.
 * - Drops stalled peers if TX FSR makes no progress for TX_WINDOW_MS.
 * - Always closes connection after response.
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

        if (!strncmp(http_buf, "GET /api/status", 15)) {
            handle_status_request(http_sock);
        } else if (!strncmp(http_buf, "GET /api/settings", 17)) {
            handle_settings_api(http_sock);
        } else if (!strncmp(http_buf, "POST /api/settings", 18)) {
            handle_settings_post(http_sock, body_ptr);
        } else if (!strncmp(http_buf, "POST /api/control", 17)) {
            handle_control_request(http_sock, body_ptr);
        } else if (!strncmp(http_buf, "GET /metrics", 12)) {
            handle_metrics_request(http_sock);
        } else if (!strncmp(http_buf, "GET /settings.html", 18)) {
            handle_settings_request(http_sock);
        } else {
            const char *page = get_page_content(http_buf);
            const int plen = (int)strlen(page);
            char header[256];
            const int hlen = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n"
                                      "Content-Length: %d\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Cache-Control: no-cache\r\n"
                                      "Connection: close\r\n"
                                      "\r\n",
                                      plen);
            int hsend = hlen;
            if (hsend < 0)
                hsend = 0;
            if (hsend > (int)sizeof(header))
                hsend = (int)sizeof(header);
            send(http_sock, (uint8_t *)header, hsend);
            net_beat();
            if (plen > 0) {
                send(http_sock, (uint8_t *)page, (uint16_t)plen);
                net_beat();
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