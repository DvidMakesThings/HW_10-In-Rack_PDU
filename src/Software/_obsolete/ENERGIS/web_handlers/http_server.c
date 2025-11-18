/**
 * @file http_server.c
 * @author DvidMakesThings - David Sipos
 * @defgroup webui3 3. HTTP Server
 * @ingroup webui
 * @brief HTTP server functionality for the ENERGIS PDU project.
 * @{
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "http_server.h"
#include "../CONFIG.h"
#include "../utils/helper_functions.h"
#include "control_handler.h"
#include "settings_handler.h"
#include "socket.h"
#include "status_handler.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_PORT 80
#define HTTP_BUF_SIZE 2048

static int8_t http_sock;
static char *http_buf;

#include <ctype.h> // ensure this is included

/**
 * @brief Parse the Content-Length header case-insensitively.
 * @return The Content-Length value, or 0 if not found.
 * @param headers Pointer to the start of the headers.
 * @param header_len Length of the headers.
 */
static int parse_content_length_ci(const char *headers, int header_len) {
    const char *p = headers, *end = headers + header_len;
    static const char key[] = "content-length:"; // lower-case target
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
 * @brief Reads a full HTTP request from the socket.
 * @return The total number of bytes read, or -1 if headers are incomplete.
 * @param sock The socket to read from.
 * @param buf The buffer to store the request.
 * @param buflen The length of the buffer.
 * @param body_off Pointer to store the offset of the body.
 * @param body_len Pointer to store the length of the body.
 */
static int read_http_request(uint8_t sock, char *buf, int buflen, int *body_off, int *body_len) {
    int total = 0, header_end = -1;

    // 1) read headers
    while (total < buflen - 1) {
        int n = recv(sock, (uint8_t *)buf + total, buflen - 1 - total);
        if (n <= 0)
            break;
        total += n;
        buf[total] = '\0';

        char *p = strstr(buf, "\r\n\r\n");
        if (p) {
            header_end = (int)(p - buf) + 4;
            break;
        }
    }
    if (header_end < 0)
        return -1;

    // 2) read body to Content-Length
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
 * @brief Initializes the HTTP server.
 * @return None
 * @param None
 */
void http_server_init(void) {
    http_buf = malloc(HTTP_BUF_SIZE);
    if (!http_buf) {
        setError(true);
        ERROR_PRINT("Failed to allocate HTTP buffer\n");
        return;
    }

    http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
    if (http_sock < 0) {
        setError(true);
        ERROR_PRINT("Failed to open HTTP socket\n");
        free(http_buf);
        return;
    }

    if (listen(http_sock) != SOCK_OK) {
        setError(true);
        ERROR_PRINT("Failed to listen on HTTP socket\n");
        close(http_sock);
        free(http_buf);
        return;
    }

    INFO_PRINT("HTTP server listening on port %d\n", HTTP_PORT);
}

/**
 * @brief Processes incoming HTTP requests.
 * @return None
 * @param None
 *
 * FIXES:
 *  1) Always pass a WRITABLE, NUL-terminated POST body to handlers.
 *     Previously, for empty bodies we forwarded a string literal (""), and
 *     handlers like handle_control_request() call urldecode() which writes
 *     in-place; that could crash when the literal is in RO memory. Now we
 *     always point into http_buf and explicitly NUL-terminate.
 *
 *  2) After serving a request, hard-close and immediately re-listen to avoid
 *     TIME_WAIT/FIN_WAIT stalls that can cause transient connect failures
 *     when tests hammer the endpoint.  :contentReference[oaicite:1]{index=1}
 *
 * Evidence in test log: repeated curl rc=7 "Could not connect to server"
 * appearing after a POST with empty --data (e.g., CH5/CH6 OFF), consistent
 * with the RO write hazard or listener stall.
 */
void http_server_process(void) {
    switch (getSn_SR(http_sock)) {

    case SOCK_ESTABLISHED: {
        if (getSn_IR(http_sock) & Sn_IR_CON)
            setSn_IR(http_sock, Sn_IR_CON);

        int body_off = 0, body_len = 0;
        int len = read_http_request(http_sock, http_buf, HTTP_BUF_SIZE, &body_off, &body_len);
        if (len > 0) {
            http_buf[len] = '\0';

            // --- Ensure handlers always receive a WRITABLE, NUL-terminated body ---
            char *body_ptr = http_buf; // safe default
            if (body_off >= 0 && body_off < HTTP_BUF_SIZE)
                body_ptr = http_buf + body_off;

            // clamp & terminate
            int max_w = (HTTP_BUF_SIZE - 1) - (int)(body_ptr - http_buf);
            if (max_w < 0) {
                body_ptr = http_buf + len; // points at the '\0' we set above
                body_len = 0;
            } else {
                if (body_len > max_w)
                    body_len = max_w;
                body_ptr[body_len] = '\0';
            }
            // ---------------------------------------------------------------------

            if (strncmp(http_buf, "GET /api/status", 15) == 0) {
                handle_status_request(http_sock);

            } else if (strncmp(http_buf, "GET /api/settings", 17) == 0) {
                handle_settings_api(http_sock);

            } else if (strncmp(http_buf, "GET /settings.html", 18) == 0) {
                handle_settings_request(http_sock);

            } else if (strncmp(http_buf, "POST /settings", 14) == 0) {
                handle_settings_post(http_sock, body_ptr);

            } else if (strncmp(http_buf, "POST /control", 13) == 0) {
                handle_control_request(http_sock, body_ptr); // urldecode() writes in-place.

            } else {
                const char *page = get_page_content(http_buf);
                int plen = (int)strlen(page);
                char header[256];
                int hdr_len = snprintf(header, sizeof(header),
                                       "HTTP/1.1 200 OK\r\n"
                                       "Content-Type: text/html\r\n"
                                       "Content-Length: %d\r\n"
                                       "Access-Control-Allow-Origin: *\r\n"
                                       "Cache-Control: no-cache\r\n"
                                       "Connection: close\r\n"
                                       "\r\n",
                                       plen);
                send(http_sock, (uint8_t *)header, hdr_len);
                send(http_sock, (uint8_t *)page, plen);
            }
        }

        // Proactively tear down and re-listen to keep the server snappy under load.
        disconnect(http_sock);
        close(http_sock);
        http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
        if ((int)http_sock >= 0)
            listen(http_sock);
        break;
    }

    case SOCK_CLOSE_WAIT:
        // Peer closed; drop quickly and reopen listener.
        disconnect(http_sock);
        close(http_sock);
        http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
        if ((int)http_sock >= 0)
            listen(http_sock);
        break;

    case SOCK_CLOSED:
        // Reopen listener if needed.
        http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
        if ((int)http_sock >= 0)
            listen(http_sock);
        break;

    default:
        // INIT, FIN_WAIT, etc. — nothing to do.
        break;
    }
}
/**
 * @}
 */