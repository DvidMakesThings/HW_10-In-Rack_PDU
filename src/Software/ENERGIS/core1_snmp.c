/**
 * @file core1_task.c
 * @brief Ethernet + SNMP server task for ENERGIS PDU.
 * @version 1.1
 * @date 2025-05-17
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

void core1_task(void) {
    uint8_t http_sock = 0;
    uint8_t snmp_agent_sock = 1;
    uint8_t snmp_trap_sock = 2;
    uint8_t managerIP[4] = {192, 168, 0, 100}; // NMS
    uint8_t agentIP[4] = {192, 168, 0, 101};   // this device

    char http_buf[256];
    char header[128];
    int hdr_len;
    uint8_t last_status = 0xFF;

    // --- 1) Initialize SNMP agent/trap ---
    snmpd_init(managerIP, agentIP, snmp_agent_sock, snmp_trap_sock);
    INFO_PRINT("SNMP agent initialized on port %d, trap on %d\n", SNMP_AGENT_PORT, SNMP_TRAP_PORT);

    // --- 2) Initialize HTTP TCP socket ---
    while (socket(http_sock, Sn_MR_TCP, HTTP_PORT, 0) != http_sock) {
        ERROR_PRINT("HTTP socket creation failed. Retrying...\n");
        sleep_ms(500);
    }
    if (listen(http_sock) != SOCK_OK) {
        ERROR_PRINT("HTTP listen failed\n");
        return;
    }
    INFO_PRINT("HTTP server listening on port %d\n", HTTP_PORT);

    // --- 3) Main loop: handle HTTP and SNMP ---
    while (1) {
        // ---- HTTP server handling ----
        uint8_t status = getSn_SR(http_sock);
        if (status != last_status) {
            DEBUG_PRINT("HTTP socket status: 0x%02X\n", status);
            last_status = status;
        }

        if (status == SOCK_ESTABLISHED) {
            // clear CON flag
            if (getSn_IR(http_sock) & Sn_IR_CON) {
                setSn_IR(http_sock, Sn_IR_CON);
            }

            // recv request
            int32_t len = recv(http_sock, (uint8_t *)http_buf, sizeof(http_buf) - 1);
            if (len > 0) {
                http_buf[len] = '\0';
                DEBUG_PRINT("HTTP Request: %s\n", http_buf);

                const char *page = get_page_content(http_buf);
                int page_len = strlen(page);

                //                                   "Connection: close\r\n"
                hdr_len = snprintf(header, sizeof(header),
                                   "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/html\r\n"
                                   "Content-Length: %d\r\n"
                                   "\r\n",
                                   page_len);

                send(http_sock, (uint8_t *)header, hdr_len);
                send(http_sock, (uint8_t *)page, page_len);
            }

            close(http_sock);
            INFO_PRINT("HTTP connection closed, reopening...\n");
            sleep_ms(50);
            // reopen & listen again
            while (socket(http_sock, Sn_MR_TCP, HTTP_PORT, 0) != http_sock) {
                ERROR_PRINT("Reopen HTTP socket failed. Retrying...\n");
                sleep_ms(500);
            }
            listen(http_sock);
        } else if (status == SOCK_CLOSE_WAIT) {
            // ensure all data drained
            while (recv(http_sock, (uint8_t *)http_buf, sizeof(http_buf)) > 0) {}
            close(http_sock);
            sleep_ms(50);
            while (socket(http_sock, Sn_MR_TCP, HTTP_PORT, 0) != http_sock) {
                sleep_ms(500);
            }
            listen(http_sock);
        } else if (status == SOCK_CLOSED) {
            WARNING_PRINT("HTTP socket closed unexpectedly, restarting...\n");
            sleep_ms(50);
            while (socket(http_sock, Sn_MR_TCP, HTTP_PORT, 0) != http_sock) {
                sleep_ms(500);
            }
            listen(http_sock);
        }

        // ---- SNMP handling ----
        // Poll for any incoming SNMP requests and respond
        snmpd_run();
        // Advance the SNMP time base (for SNMP TimeTicks)
        SNMP_time_handler();

        // small delay to avoid 100% CPU
        sleep_ms(10);
    }
}
