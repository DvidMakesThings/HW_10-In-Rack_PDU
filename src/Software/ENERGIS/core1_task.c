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
#include "CONFIG.h"
#include "hardware/irq.h"
#include "hardware/watchdog.h"
#include "network/snmp.h"
#include "network/snmp_custom.h"
#include "pico/stdlib.h"
#include "socket.h"
#include "web_handlers/http_server.h"
#include <stdio.h>
#include <string.h>

#define SNMP_AGENT_PORT PORT_SNMP_AGENT // 161
#define SNMP_TRAP_PORT PORT_SNMP_TRAP   // 162

/**
 * @brief Task for Core 1 to handle SNMP and HTTP server.
 */
void core1_task(void) {
    uint8_t snmp_agent_sock, snmp_trap_sock;
    uint8_t managerIP[4] = {192, 168, 0, 100};
    uint8_t agentIP[4] = {192, 168, 0, 101};

    // Initialize SNMP
    snmp_agent_sock = 1;
    snmp_trap_sock = 2;
    snmpd_init(managerIP, agentIP, snmp_agent_sock, snmp_trap_sock);
    INFO_PRINT("SNMP agent ready on port %d, traps on %d\n", SNMP_AGENT_PORT, SNMP_TRAP_PORT);

    // Initialize HTTP server
    http_server_init();

    // Main loop
    while (true) {
        // Process HTTP requests
        http_server_process();

        // Service SNMP
        snmpd_run();
        SNMP_time_handler();

        sleep_ms(1);
    }
}