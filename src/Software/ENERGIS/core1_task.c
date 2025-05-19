/**
 * @file core1_task.c
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
#include <stdlib.h>
#include <string.h>

#define HTTP_PORT 80
#define SNMP_AGENT_PORT PORT_SNMP_AGENT // 161
#define SNMP_TRAP_PORT PORT_SNMP_TRAP   // 162
#define HTTP_BUF_SIZE 2048              // Increased buffer size

// Channel data structure
typedef struct {
    float voltage;
    float current;
    uint32_t uptime;
    float power;
    bool state;
    uint32_t last_toggle; // Timestamp of last toggle
} channel_data_t;

static channel_data_t channels[8];
static float internal_temperature = 25.0f;
static char system_status[32] = "OK";

/**
 * @brief Updates channel measurements and state
 * @param channel Channel number (0-7)
 */
static void update_channel_measurements(uint8_t channel) {
    if (channel >= 8)
        return;

    // Read actual relay state from hardware
    bool hw_state = mcp_relay_read_pin(channel);

    // If hardware state differs from our tracked state, update our state
    if (hw_state != channels[channel].state) {
        channels[channel].state = hw_state;
        channels[channel].last_toggle = to_ms_since_boot(get_absolute_time());
    }

    // Read actual values from HLW8032 here
    channels[channel].voltage = 230.0f; // Example value
    channels[channel].current = channels[channel].state ? 2.1f : 0.0f;
    channels[channel].power = channels[channel].voltage * channels[channel].current;

    if (channels[channel].state) {
        channels[channel].uptime += 10; // Increment by 10 seconds
    }
}

/**
 * @brief Synchronizes all channel states with hardware
 */
static void sync_all_channels(void) {
    for (int i = 0; i < 8; i++) {
        update_channel_measurements(i);
    }
}

/**
 * @brief Handles /api/status endpoint
 * @param sock Socket number
 */
static void handle_status_request(uint8_t sock) {
    char json[1024];
    int pos = 0;

    // Update all channel measurements
    sync_all_channels();

    // Build JSON response
    pos += snprintf(json + pos, sizeof(json) - pos, "{\"channels\":[");
    for (int i = 0; i < 8; i++) {
        pos += snprintf(
            json + pos, sizeof(json) - pos,
            "{\"voltage\":%.2f,\"current\":%.2f,\"power\":%.2f,\"uptime\":%u,\"state\":%s}%s",
            channels[i].voltage, channels[i].current, channels[i].power, channels[i].uptime,
            channels[i].state ? "true" : "false", (i < 7) ? "," : "");
    }
    pos += snprintf(json + pos, sizeof(json) - pos,
                    "],\"internalTemperature\":%.2f,\"systemStatus\":\"%s\"}", internal_temperature,
                    system_status);

    // Send response
    char header[256];
    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Cache-Control: no-cache\r\n"
                           "\r\n",
                           pos);

    send(sock, (uint8_t *)header, hdr_len);
    send(sock, (uint8_t *)json, pos);
}

/**
 * @brief Handles POST /control endpoint
 * @param sock Socket number
 * @param body Request body
 */
static void handle_control_request(uint8_t sock, char *body) {
    if (!body) {
        const char *resp = "HTTP/1.1 400 Bad Request\r\n"
                           "Content-Length: 0\r\n"
                           "\r\n";
        send(sock, (uint8_t *)resp, strlen(resp));
        return;
    }

    // First, set all channels to OFF
    bool new_states[8] = {false};

    // Parse form data and update states
    char *param = strtok(body, "&");
    while (param) {
        if (strncmp(param, "channel", 7) == 0 && strstr(param, "=on")) {
            int ch = param[7] - '1'; // "channel1" -> 0
            if (ch >= 0 && ch < 8) {
                new_states[ch] = true;
            }
        }
        param = strtok(NULL, "&");
    }

    // Apply changes
    for (int i = 0; i < 8; i++) {
        bool current_state = mcp_relay_read_pin(i);
        if (current_state != new_states[i]) {
            PDU_Display_ToggleRelay(i + 1);
            channels[i].state = new_states[i];
            channels[i].last_toggle = to_ms_since_boot(get_absolute_time());
        }
    }

    // Re-sync all channels after changes
    sync_all_channels();

    // Send success response with current states
    char json[1024];
    int pos = 0;

    pos += snprintf(json + pos, sizeof(json) - pos, "{\"channels\":[");
    for (int i = 0; i < 8; i++) {
        pos += snprintf(json + pos, sizeof(json) - pos, "{\"state\":%s}%s",
                        channels[i].state ? "true" : "false", (i < 7) ? "," : "");
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}");

    char header[256];
    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Cache-Control: no-cache\r\n"
                           "\r\n",
                           pos);

    send(sock, (uint8_t *)header, hdr_len);
    send(sock, (uint8_t *)json, pos);
}

/**
 * @brief Task for Core 1 to handle SNMP and HTTP server.
 */
void core1_task(void) {
    int8_t http_sock;
    uint8_t snmp_agent_sock, snmp_trap_sock;
    uint8_t managerIP[4] = {192, 168, 0, 100};
    uint8_t agentIP[4] = {192, 168, 0, 101};
    char *http_buf = malloc(HTTP_BUF_SIZE);
    char header[256];

    if (!http_buf) {
        ERROR_PRINT("Failed to allocate HTTP buffer\n");
        return;
    }

    // Initialize SNMP on sockets 1 & 2
    snmp_agent_sock = 1;
    snmp_trap_sock = 2;
    snmpd_init(managerIP, agentIP, snmp_agent_sock, snmp_trap_sock);
    INFO_PRINT("SNMP agent ready on port %d, traps on %d\n", SNMP_AGENT_PORT, SNMP_TRAP_PORT);

    // Initialize channel data
    memset(channels, 0, sizeof(channels));

    // Initialize channel states from actual relay states
    sync_all_channels();

    // Open HTTP socket
    http_sock = socket(0, Sn_MR_TCP, HTTP_PORT, 0);
    if (http_sock < 0) {
        ERROR_PRINT("Failed to open HTTP socket\n");
        free(http_buf);
        return;
    }

    // Start listening
    if (listen(http_sock) != SOCK_OK) {
        ERROR_PRINT("Failed to listen on HTTP socket\n");
        close(http_sock);
        free(http_buf);
        return;
    }

    INFO_PRINT("HTTP server listening on port %d\n", HTTP_PORT);

    while (true) {
        uint8_t status = getSn_SR(http_sock);

        switch (status) {
        case SOCK_ESTABLISHED:
            if (getSn_IR(http_sock) & Sn_IR_CON) {
                setSn_IR(http_sock, Sn_IR_CON);
            }

            int32_t len = recv(http_sock, (uint8_t *)http_buf, HTTP_BUF_SIZE - 1);
            if (len > 0) {
                http_buf[len] = '\0';

                // Handle GET /api/status
                if (memcmp(http_buf, "GET /api/status", 14) == 0) {
                    handle_status_request(http_sock);
                }
                // Handle POST /control
                else if (memcmp(http_buf, "POST /control", 12) == 0) {
                    char *body = strstr(http_buf, "\r\n\r\n");
                    if (body)
                        body += 4;
                    handle_control_request(http_sock, body);
                }
                // Handle static files
                else {
                    const char *page = get_page_content(http_buf);
                    int plen = strlen(page);
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

        // Service SNMP
        snmpd_run();
        SNMP_time_handler();

        sleep_ms(1); // Reduced sleep time for better responsiveness
    }

    free(http_buf); // Clean up
}