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
#include "network/w5500.h"   
#include "network/socket.h"         

// Use our embedded HTML pages.
#include "html/html_files.h"

#include "dummy_eeprom.h"
#include "PDU_display.h"
#include "startup.h"
#include "utils/helper_functions.h"

// Static IP configuration
uint8_t ip_addr[4] = {192, 168, 0, 11};
uint8_t subnet_mask[4] = {255, 255, 255, 0};
uint8_t gateway[4] = {192, 168, 0, 1};
uint8_t mac_addr[6] = {0x00, 0x08, 0xDC, 0xBE, 0xEF, 0x91};

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
    printf("[CORE1] Starting core1_task...\n");

    // Initialize the W5500 with network parameters.
    if (w5500_init(gateway, subnet_mask, mac_addr, ip_addr) != 0) {
        printf("[CORE1] ERROR: W5500 initialization failed!\n");
        while(1) sleep_ms(1000);
    }
    printf("[CORE1] W5500 initialized successfully with IP %d.%d.%d.%d\n",
           ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);

    // Open a TCP socket on SERVER_PORT.
    int sock = w5500_socket_open(W5500_SOCK_MR_TCP);
    if (sock < 0) {
        printf("[CORE1] ERROR: Failed to open TCP socket!\n");
        while(1) sleep_ms(1000);
    }
    printf("[CORE1] TCP socket %d opened.\n", sock);

    // Set the socket to LISTEN mode.
    if(w5500_socket_listen(sock) == false) {
        printf("[CORE1] ERROR: Failed to set socket %d to LISTEN mode!\n", sock);
        while(1) sleep_ms(1000);
    } else {
        printf("[CORE1] Socket %d set to LISTEN mode.\n", sock);
        printf("[CORE1] Test-NetConnection -ComputerName 192.168.0.11 -Port 80\n");
    }

    // Main loop: wait for an incoming HTTP connection.
    while (1) {
        // Print status every 1000 ms (or every 100 iterations of 10 ms)
        static int counter = 0;
        counter++;
        if (counter >= 100) {
            printf("[CORE1] Socket %d current status: 0x%02X\n", sock, w5500_read_reg(W5500_Sn_SR(sock)));
            counter = 0;
        }
        sleep_ms(10);

        // Poll the socket status.
        uint8_t status = w5500_read_reg(W5500_Sn_SR(sock));
        if (status == W5500_SR_ESTABLISHED) {  // Typically 0x17 for an established TCP connection.
            printf("[CORE1] Client connected on socket %d.\n", sock);

            // Optionally, receive the HTTP request (for debugging).
            uint8_t rx_buffer[1024] = {0};
            int bytes_received = w5500_socket_recv(sock, rx_buffer, sizeof(rx_buffer) - 1);
            if (bytes_received > 0) {
                rx_buffer[bytes_received] = '\0';  // Null-terminate
                printf("[CORE1] Received (%d bytes): %s\n", bytes_received, rx_buffer);
            }

            // Prepare a basic HTTP response.
            http_response_t response;
            http_response_set_status(&response, 200);
            const char *html_body = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/html\r\n"
                                    "Content-Length: 46\r\n"
                                    "\r\n"
                                    "<html><body><h1>Hello, World!</h1></body></html>";
            http_response_set_body(&response, html_body, strlen(html_body));
            http_response_send(&response, sock);
            printf("[CORE1] HTTP response sent on socket %d.\n", sock);

            // Close the current socket connection.
            w5500_socket_close(sock);
            printf("[CORE1] Socket %d closed.\n", sock);

            // Reopen a new TCP socket and set it to LISTEN mode for the next connection.
            sock = w5500_socket_open(W5500_SOCK_MR_TCP);
            if (sock < 0) {
                printf("[CORE1] ERROR: Failed to re-open TCP socket!\n");
                while(1) sleep_ms(1000);
            }
            w5500_socket_listen(sock);
            printf("[CORE1] Socket %d re-opened and set to LISTEN mode.\n", sock);
        }
        sleep_ms(10);
    }
}