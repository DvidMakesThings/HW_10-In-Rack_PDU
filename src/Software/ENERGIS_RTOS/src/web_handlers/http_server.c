/**
 * @file http_server.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webhandlers Web-UI Handlers
 * @brief HTTP server functionality for the ENERGIS PDU project.
 * @{
 *
 * @defgroup webui1 1. HTTP Server
 * @ingroup webhandlers
 * @brief Main HTTP server implementation
 * @{
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

/* HTTP server configuration */
#define HTTP_PORT 80
#define HTTP_BUF_SIZE 2048
#define HTTP_SOCK_NUM 0

/* Static buffers and socket handle */
static int8_t http_sock;
static char *http_buf;

/* Tag for logging */
#define HTTP_SERVER_TAG "<HTTP Server>"

/**
 * @brief Parse the Content-Length header case-insensitively
 * @param headers Pointer to the start of the headers
 * @param header_len Length of the headers
 * @return The Content-Length value, or 0 if not found
 * @note Case-insensitive parsing for better HTTP compliance
 */
static int parse_content_length_ci(const char *headers, int header_len) {
    const char *p = headers;
    const char *end = headers + header_len;
    static const char key[] = "content-length:";
    const size_t klen = sizeof(key) - 1;

    while (p < end) {
        const char *line_end = memchr(p, '\n', (size_t)(end - p));
        if (!line_end)
            line_end = end;

        if ((size_t)(line_end - p) >= klen) {
            size_t i = 0;
            while (i < klen && (char)tolower((unsigned char)p[i]) == key[i])
                i++;
            if (i == klen) {
                const char *q = p + klen;
                while (q < line_end && (*q == ' ' || *q == '\t'))
                    q++;
                long v = strtol(q, NULL, 10);
                if (v > 0 && v < (1 << 28))
                    return (int)v;
                return 0;
            }
        }
        p = line_end + 1;
    }
    return 0;
}

/**
 * @brief Sends the entire buffer over the socket
 * @param sock The socket to send data on
 * @param buf The buffer containing data to send
 * @param len The length of the buffer
 * @return The total number of bytes sent
 * @note Handles partial sends and retries as needed
 */
static int send_all(uint8_t sock, const uint8_t *buf, int len) {
    int total = 0;
    while (total < len) {
        int n = send(sock, (uint8_t *)buf + total, len - total);
        if (n > 0) {
            total += n;
            continue;
        }
        /* If the peer closed or socket not established, stop */
        uint8_t st = getSn_SR(sock);
        if (st != SOCK_ESTABLISHED && st != SOCK_CLOSE_WAIT) {
            break;
        }
        /* brief yield to let W5500 free TX space */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return total;
}

/**
 * @brief Reads a full HTTP request from the socket
 * @param sock The socket to read from
 * @param buf The buffer to store the request
 * @param buflen The length of the buffer
 * @param body_off Pointer to store the offset of the body
 * @param body_len Pointer to store the length of the body
 * @return The total number of bytes read, or -1 if headers are incomplete
 * @note This function reads headers first, then body based on Content-Length
 */
static int read_http_request(uint8_t sock, char *buf, int buflen, int *body_off, int *body_len) {
    int total = 0;
    int header_end = -1;

    /* Read headers first */
    while (total < buflen - 1) {
        int n = recv(sock, (uint8_t *)buf + total, buflen - 1 - total);
        if (n <= 0)
            break;
        total += n;
        buf[total] = '\0';

        /* Look for end of headers */
        char *p = strstr(buf, "\r\n\r\n");
        if (p) {
            header_end = (int)(p - buf) + 4;
            break;
        }
    }

    if (header_end < 0)
        return -1;

    /* Read body according to Content-Length */
    int clen = parse_content_length_ci(buf, header_end);
    int have = total - header_end;

    while (have < clen && total < buflen - 1) {
        int n = recv(sock, (uint8_t *)buf + total, buflen - 1 - total);
        if (n <= 0)
            break;
        total += n;
        have += n;
        buf[total] = '\0';
    }

    *body_off = header_end;
    *body_len = (have > 0) ? have : 0;
    return total;
}

/**
 * @brief Initializes the HTTP server
 * @return true on success, false on failure
 * @note Allocates buffers and opens listening socket on port 80
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
 * @return None
 * @note Call this regularly from NetTask to handle incoming connections.
 *       Routes requests to appropriate handlers based on URL path.
 *       After serving a request, proactively closes and reopens the listener
 *       to avoid TIME_WAIT/FIN_WAIT stalls under load.
 */
void http_server_process(void) {
    switch (getSn_SR(http_sock)) {

    case SOCK_ESTABLISHED: {
        if (getSn_IR(http_sock) & Sn_IR_CON)
            setSn_IR(http_sock, Sn_IR_CON);

        int body_off = 0, body_len = 0;
        int n = read_http_request(http_sock, http_buf, HTTP_BUF_SIZE, &body_off, &body_len);
        if (n <= 0)
            break;

        http_buf[n] = '\0';
        char *body_ptr = http_buf + ((body_off > 0 && body_off < HTTP_BUF_SIZE) ? body_off : n);
        if (body_ptr + body_len > http_buf + HTTP_BUF_SIZE)
            body_len = (http_buf + HTTP_BUF_SIZE) - body_ptr;
        if (body_len < 0)
            body_len = 0;
        body_ptr[body_len] = '\0';

        if (!strncmp(http_buf, "GET /api/status", 15))
            handle_status_request(http_sock);
        else if (!strncmp(http_buf, "GET /api/settings", 17))
            handle_settings_api(http_sock);
        else if (!strncmp(http_buf, "GET /settings.html", 18))
            handle_settings_request(http_sock);
        else if (!strncmp(http_buf, "POST /settings", 14))
            handle_settings_post(http_sock, body_ptr);
        else if (!strncmp(http_buf, "POST /control", 13))
            handle_control_request(http_sock, body_ptr);
        else {
            /* Static page */
            const char *page = get_page_content(http_buf);
            const int plen = (int)strlen(page);
            char header[256];
            const int hlen = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html; charset=utf-8\r\n"
                                      "Content-Length: %d\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Cache-Control: no-cache\r\n"
                                      "Connection: close\r\n"
                                      "\r\n",
                                      plen);

            (void)send_all_blocking(http_sock, (const uint8_t *)header, hlen);
            (void)send_all_blocking(http_sock, (const uint8_t *)page, plen);
        }

        /* Drain until peer closes or nothing left to send */
        TickType_t tstart = xTaskGetTickCount();
        for (;;) {
            uint8_t sr = getSn_SR(http_sock);
            if (sr == SOCK_CLOSE_WAIT || sr == SOCK_CLOSED)
                break;

            /* When TX buffer empties, it’s safe to close even if peer is slow */
            if (getSn_TX_FSR(http_sock) == getSn_TxMAX(http_sock))
                break;

            if ((xTaskGetTickCount() - tstart) > pdMS_TO_TICKS(3000))
                break;
            vTaskDelay(pdMS_TO_TICKS(1));
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

    case SOCK_CLOSED:
        http_sock = socket(HTTP_SOCK_NUM, Sn_MR_TCP, HTTP_PORT, 0);
        if ((int)http_sock >= 0)
            listen(http_sock);
        break;

    default:
        break;
    }
}

/**
 * @}
 */
