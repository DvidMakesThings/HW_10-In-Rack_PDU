/**
 * @file http_server.c
 * @author DvidMakesThings - David Sipos
 * @brief HTTP server functionality for the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "http_server.h"
#include "../CONFIG.h"
#include "../utils/helper_functions.h"
#include "api_handlers.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 

#define HTTP_PORT 80
#define HTTP_BUF_SIZE 2048

static int8_t http_sock;
static char *http_buf;

#include <ctype.h>   // ensure this is included

// Parse Content-Length case-insensitively without strcasestr
static int parse_content_length_ci(const char *headers, int header_len){
    const char *p = headers, *end = headers + header_len;
    static const char key[] = "content-length:"; // lower-case target
    const size_t klen = sizeof(key) - 1;

    while (p < end) {
        const char *line_end = memchr(p, '\n', (size_t)(end - p));
        if (!line_end) line_end = end;

        if ((size_t)(line_end - p) >= klen) {
            size_t i = 0;
            while (i < klen &&
                   (char)tolower((unsigned char)p[i]) == key[i]) i++;
            if (i == klen) {
                const char *q = p + klen;
                while (q < line_end && (*q == ' ' || *q == '\t')) q++;
                long v = strtol(q, NULL, 10);
                if (v > 0 && v < (1<<28)) return (int)v;
                return 0;
            }
        }
        p = line_end + 1;
    }
    return 0;
}

/* Read full HTTP request:
   - accumulate until headers complete ("\r\n\r\n")
   - parse Content-Length
   - read body fully
   Returns total bytes (buf NUL-terminated), sets body_off/body_len; -1 if headers incomplete. */
static int read_http_request(uint8_t sock, char *buf, int buflen, int *body_off, int *body_len){
    int total = 0, header_end = -1;

    // 1) read headers
    while (total < buflen - 1) {
        int n = recv(sock, (uint8_t*)buf + total, buflen - 1 - total);
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';

        char *p = strstr(buf, "\r\n\r\n");
        if (p) { header_end = (int)(p - buf) + 4; break; }
    }
    if (header_end < 0) return -1;

    // 2) read body to Content-Length
    int clen = parse_content_length_ci(buf, header_end);
    int have = total - header_end;
    while (have < clen && total < buflen - 1) {
        int n = recv(sock, (uint8_t*)buf + total, buflen - 1 - total);
        if (n <= 0) break;
        total += n;
        have  += n;
        buf[total] = '\0';
    }

    *body_off = header_end;
    *body_len = (have > 0) ? have : 0;
    return total;
}


/**
 * @brief Initializes the HTTP server.
 *
 * This function sets up the HTTP server by creating a socket and listening for incoming
 * connections. It also allocates memory for the HTTP buffer.
 */
void http_server_init(void) {
    http_buf = malloc(HTTP_BUF_SIZE);
    if (!http_buf) {
        ERROR_PRINT("Failed to allocate HTTP buffer\n");
        return;
    }

    http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
    if (http_sock < 0) {
        ERROR_PRINT("Failed to open HTTP socket\n");
        free(http_buf);
        return;
    }

    if (listen(http_sock) != SOCK_OK) {
        ERROR_PRINT("Failed to listen on HTTP socket\n");
        close(http_sock);
        free(http_buf);
        return;
    }

    INFO_PRINT("HTTP server listening on port %d\n", HTTP_PORT);
}

/**
 * @brief Processes incoming HTTP requests.
 *
 * This function checks the status of the HTTP socket and handles incoming requests.
 * It responds to various API endpoints and serves static content.
 */
void http_server_process(void) {
    switch (getSn_SR(http_sock)) {

    case SOCK_ESTABLISHED: 
        if (getSn_IR(http_sock) & Sn_IR_CON) setSn_IR(http_sock, Sn_IR_CON);

        int body_off = 0, body_len = 0;
        int len = read_http_request(http_sock, http_buf, HTTP_BUF_SIZE, &body_off, &body_len);
        if (len > 0) {
            http_buf[len] = '\0';

            if (memcmp(http_buf, "GET /api/status", 14) == 0) {
                handle_status_request(http_sock);

            } else if (memcmp(http_buf, "GET /api/settings", 16) == 0 ||
                    memcmp(http_buf, "GET /settings.html", 18) == 0) {
                handle_settings_request(http_sock);

            } else if (memcmp(http_buf, "POST /settings", 13) == 0) {
                handle_settings_post(http_sock, body_len ? (http_buf + body_off) : (char*)"");

            } else if (memcmp(http_buf, "POST /control", 12) == 0) {
                handle_control_request(http_sock, body_len ? (http_buf + body_off) : (char*)"");

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
                    "\r\n", plen);
                send(http_sock, (uint8_t*)header, hdr_len);
                send(http_sock, (uint8_t*)page,   plen);
            }
        }

        disconnect(http_sock);
        break;

    case SOCK_CLOSE_WAIT:
        // peer closed; ensure we drop it
        disconnect(http_sock);
        break;

    case SOCK_CLOSED:
        // reopen listener
        http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
        if ((int)http_sock >= 0) listen(http_sock);
        break;

    default:
        // INIT, FIN_WAIT, etc. — do nothing
        break;
    }
}
