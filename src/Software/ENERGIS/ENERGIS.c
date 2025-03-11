/**
 * @file main.c
 * @brief Main application for ENERGIS PDU using Wiznet ioLibrary.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 *
 * This example initializes the W5500 using the Wiznet ioLibrary and runs an HTTP
 * server on port 80 that serves the PDU UI pages (embedded via html_files.h).
 * The W5500 is used on SPI0 at 10MHz while the LCD continues to use SPI1 at 40MHz.
*/

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/resets.h"
#include <stdio.h>
#include <string.h>

#include "CONFIG.h"
#include "drivers/CAT24C512_driver.h"
#include "drivers/ILI9488_driver.h"
#include "drivers/MCP23017_display_driver.h"
#include "drivers/MCP23017_relay_driver.h"

// Use the Wiznet ioLibrary headers:
#include "drivers/W5500_RP2040_HTTP.h"         

// Use our embedded HTML pages.
#include "html/html_files.h"

#include "dummy_eeprom.h"
#include "PDU_display.h"
#include "startup.h"
#include "utils/helper_functions.h"

// Static IP configuration
const uint8_t ip_addr[4] = IP_ADDR;
const uint8_t subnet_mask[4] = SUBNET_MASK;
const uint8_t gateway[4] = GATEWAY;
const uint8_t mac_addr[6] = MAC_ADDR;

void core1_task(void);



//------------------------------------------------------------------------------
// main: Performs system initialization and launches core1_task on Core 1.
int main(void) {
    stdio_init_all();
    sleep_ms(10000); // Delay for debugging

    startup_init();
    /*
    i2c_scan_bus(i2c0, "I2C0");
    i2c_scan_bus(i2c1, "I2C1");
        
    // Initialize EEPROM.
    CAT24C512_Init();
    printf("EEPROM initialized\n");
    write_dummy_eeprom_content();

    // Initialize MCP23017 I/O expanders.
    mcp_display_init();
    printf("Display-board initialized\n");
    mcp_relay_init();
    printf("Relay-board initialized\n");

    // Little showoff.
    mcp_display_write_reg(MCP23017_OLATA, 0xFF);
    mcp_display_write_reg(MCP23017_OLATB, 0xFF);
    sleep_ms(500);
    for (uint8_t i = 16; i > 0; i--) {
        mcp_display_write_pin(i - 1, 0);
        sleep_ms(50);
    }
    mcp_display_write_pin(PWR_LED, 1);
    mcp_display_write_pin(FAULT_LED, 1);

    // Initialize the ILI9488 display.
    ILI9488_Init();
    PDU_Display_Init();
    sleep_ms(200);
    PDU_Display_UpdateStatus("System initialized.");
    mcp_display_write_pin(FAULT_LED, 0);
    sleep_ms(1000);
    PDU_Display_UpdateStatus("System ready.");
    */
    // Launch Ethernet/HTTP server handling on Core 1.
    multicore_launch_core1(core1_task);

    // Main loop on Core 0: for other tasks.
    while (true) {
        sleep_ms(1000);
    }
    
    return 0;
}

// core1_task: Runs on Core 1. Initializes Ethernet, sets network parameters,
// and starts the HTTP server.
void core1_task(void) {
    // Initialize the W5500
    if (!w5500_init()) {
        printf("W5500 Initialization Failed\n");
        return;
    }

    // Set network configurations (MAC, IP, Subnet, Gateway)
    w5500_set_mac(mac_addr);
    w5500_set_ip(ip_addr);
    w5500_set_subnet(subnet_mask);
    w5500_set_gateway(gateway);

    printf("W5500 IP set to: %d.%d.%d.%d\n", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    // Initialize the socket memory before opening the socket
    DEBUG_PRINT("[INFO] Set socket memory information\n");
    w5500_socket_memory_setup(0);  // Set memory for socket 0 (2KB RX and TX buffers)

    // Initialize the HTTP Server
    http_server_t http_server;
    http_server_init(&http_server, SERVER_PORT);  // Initialize HTTP server on port 8081

    // Open the socket and start the server
    if (!http_server_start(&http_server)) {
        printf("Failed to start HTTP server\n");
        return;
    }

    // Register routes, handle the HTTP server process
    http_server_register_route(&http_server, "/", HTTP_METHOD_GET, handle_root_request);

    printf("HTTP Server running on port %d...\n", SERVER_PORT);

    // Main loop to process incoming requests
    while (true) {
        http_server_process(&http_server);  // Process incoming HTTP requests
        sleep_ms(100);  // Adjust sleep time as necessary
    }
}


