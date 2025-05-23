/**
 * @file http_server.c
 * @author David Sipos
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

#define HTTP_PORT 80
#define HTTP_BUF_SIZE 2048

static int8_t http_sock;
static char *http_buf;

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
    uint8_t status = getSn_SR(http_sock);

    switch (status) {
    case SOCK_ESTABLISHED:
        if (getSn_IR(http_sock) & Sn_IR_CON) {
            setSn_IR(http_sock, Sn_IR_CON);
        }

        int32_t len = recv(http_sock, (uint8_t *)http_buf, HTTP_BUF_SIZE - 1);
        if (len > 0) {
            http_buf[len] = '\0';

            if (memcmp(http_buf, "GET /api/status", 14) == 0) {
                handle_status_request(http_sock);

            } else if (memcmp(http_buf, "GET /api/settings", 16) == 0) {
                handle_settings_request(http_sock);

            } else if (memcmp(http_buf, "GET /settings.html", 18) == 0) {
                // Render the HTML form with real EEPROM values
                handle_settings_request(http_sock);

            } else if (memcmp(http_buf, "POST /settings", 13) == 0) {
                char *body = strstr(http_buf, "\r\n\r\n");
                if (body) {
                    body += 4;
                    handle_settings_post(http_sock, body);
                }

            } else if (memcmp(http_buf, "POST /control", 12) == 0) {
                char *body = strstr(http_buf, "\r\n\r\n");
                if (body) {
                    body += 4;
                    handle_control_request(http_sock, body);
                }

            } else {
                // All other GETs fall back to static content
                const char *page = get_page_content(http_buf);
                int plen = strlen(page);
                char header[256];
                int hdr_len = snprintf(header, sizeof(header),
                                       "HTTP/1.1 200 OK\r\n"
                                       "Content-Type: text/html\r\n"
                                       "Content-Length: %d\r\n"
                                       "Access-Control-Allow-Origin: *\r\n"
                                       "Cache-Control: no-cache\r\n"
                                       "\r\n",
                                       plen);
                send(http_sock, (uint8_t *)header, hdr_len);
                send(http_sock, (uint8_t *)page, plen);
            }
        }

        disconnect(http_sock);
        break;

    case SOCK_CLOSE_WAIT:
        disconnect(http_sock);
        break;

    case SOCK_CLOSED:
        http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
        if (http_sock >= 0) {
            listen(http_sock);
        }
        break;
    }
}
