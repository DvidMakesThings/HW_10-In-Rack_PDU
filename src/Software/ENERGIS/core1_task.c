/**
 * @file core1_snmp.c
 * @author David Sipos
 * @brief Core 1 task for handling SNMP and HTTP server.
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "core1_task.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "network/snmp.h"
#include "network/snmp_custom.h"
#include "pico/stdlib.h"
#include "socket.h"
#include <stdio.h>
#include <string.h>

#define HTTP_PORT 80
#define SNMP_AGENT_PORT PORT_SNMP_AGENT // 161
#define SNMP_TRAP_PORT PORT_SNMP_TRAP   // 162

/**
 * @brief Task for Core 1 to handle SNMP and HTTP server.
 */
void core1_task(void) {
    int8_t http_sock;
    uint8_t snmp_agent_sock, snmp_trap_sock;
    uint8_t managerIP[4] = {192, 168, 0, 100};
    uint8_t agentIP[4] = {192, 168, 0, 101};
    char http_buf[512];
    char header[128];

    // 1) Initialize SNMP on sockets 1 & 2
    snmp_agent_sock = 1;
    snmp_trap_sock = 2;
    snmpd_init(managerIP, agentIP, snmp_agent_sock, snmp_trap_sock);
    INFO_PRINT("SNMP agent ready on port %d, traps on %d\n", SNMP_AGENT_PORT, SNMP_TRAP_PORT);

    // 2) Open & listen on HTTP port
    http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
    if (http_sock < 0) {
        ERROR_PRINT("Failed to open HTTP socket\n");
        return;
    }
    if (listen(http_sock) != SOCK_OK) {
        ERROR_PRINT("Failed to listen on HTTP socket\n");
        return;
    }
    INFO_PRINT("HTTP server listening on port %d\n", HTTP_PORT);

    // 3) Main loop: interleave HTTP servicing and SNMP
    while (true) {
        // ----- HTTP handling -----
        uint8_t status = getSn_SR(http_sock);

        if (status == SOCK_ESTABLISHED) {
            // clear the CON flag
            if (getSn_IR(http_sock) & Sn_IR_CON) {
                setSn_IR(http_sock, Sn_IR_CON);
            }

            // read the request
            int32_t len = recv(http_sock, (uint8_t *)http_buf, sizeof(http_buf) - 1);
            if (len > 0) {
                http_buf[len] = '\0';

                // 1) GET /api/status → JSON
                if (memcmp(http_buf, "GET /api/status ", 16) == 0) {
                    char json[1024];
                    int pos = 0;

                    pos += snprintf(json + pos, sizeof(json) - pos, "{ \"channels\": [");
                    for (int i = 0; i < 8; i++) {
                        // your real sensor readers here:
                        float V = 230.0;    // read_channel_voltage(i);
                        float I = 2.1;      // read_channel_current(i);
                        uint32_t U = 32000; // read_channel_uptime(i);
                        float P = V * I;
                        pos += snprintf(json + pos, sizeof(json) - pos,
                                        "{ \"voltage\": %.2f, \"current\": %.2f, \"uptime\": %u, "
                                        "\"power\": %.2f }%s",
                                        V, I, U, P, (i < 7 ? "," : ""));
                    }
                    pos += snprintf(json + pos, sizeof(json) - pos,
                                    "], \"internalTemperature\": %.2f, \"systemStatus\": \"%s\" }",
                                    25.0, "OK"); // read_internal_temp(), get_system_status()

                    int hdr_len = snprintf(header, sizeof(header),
                                           "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: application/json\r\n"
                                           "Content-Length: %d\r\n"
                                           "\r\n",
                                           pos);
                    send(http_sock, (uint8_t *)header, hdr_len);
                    send(http_sock, (uint8_t *)json, pos);
                }
                // 2) Control POST
                else if (strncmp(http_buf, "POST /control ", 14) == 0) {
                    char *body = strstr(http_buf, "\r\n\r\n");
                    if (body) {
                        body += 4;
                        // all off
                        for (int i = 0; i < 8; i++) {
                            PDU_Display_ToggleRelay(i + 1);
                        }
                        // parse form
                        char *param = strtok(body, "&");
                        while (param) {
                            if (strncmp(param, "channel", 7) == 0) {
                                int ch = param[7] - '1'; // "channel1"→0
                                bool on = (strstr(param, "=on") != NULL);
                                PDU_Display_ToggleRelay(ch + 1);
                            }
                            param = strtok(NULL, "&");
                        }
                    }
                    // send 302 redirect
                    const char *resp = "HTTP/1.1 302 Found\r\n"
                                       "Location: /control.html\r\n"
                                       "Content-Length: 0\r\n"
                                       "\r\n";
                    send(http_sock, (uint8_t *)resp, strlen(resp));
                }
                // 3) Fallback: serve your HTML
                else {
                    const char *page = get_page_content(http_buf);
                    int plen = strlen(page);
                    int hdr_len = snprintf(header, sizeof(header),
                                           "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/html\r\n"
                                           "Content-Length: %d\r\n"
                                           "\r\n",
                                           plen);
                    send(http_sock, (uint8_t *)header, hdr_len);
                    send(http_sock, (uint8_t *)page, plen);
                }
            }

            // gracefully disconnect and resume listening
            disconnect(http_sock);
            listen(http_sock);
        } else if (status == SOCK_CLOSE_WAIT) {
            // peer closed; just clean up
            disconnect(http_sock);
            listen(http_sock);
        } else if (status == SOCK_CLOSED) {
            // socket died: reopen & re-listen
            socket(http_sock, Sn_MR_TCP, HTTP_PORT, 0);
            listen(http_sock);
        }

        // ----- SNMP servicing -----
        snmpd_run();
        SNMP_time_handler();

        sleep_ms(10);
    }
}