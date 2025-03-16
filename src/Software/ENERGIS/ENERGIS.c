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
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/resets.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#include "CONFIG.h"
#include "core0_task.h"
#include "core1_task.h"

// Wiznet IoLibrary headers
#include "network/socket.h"
#include "network/w5x00_spi.h"
#include "network/wizchip_conf.h"

// Drivers & utilities
#include "PDU_display.h"
#include "drivers/CAT24C512_driver.h"
#include "drivers/ILI9488_driver.h"
#include "drivers/MCP23017_display_driver.h"
#include "drivers/MCP23017_relay_driver.h"
#include "startup.h"
#include "utils/EEPROM_MemoryMap.h"
#include "utils/helper_functions.h"

wiz_NetInfo g_net_info;

void core1_task(void);

//------------------------------------------------------------------------------
// main: Performs system initialization and launches core1_task on Core 1.
int main(void) {
    stdio_init_all();
    sleep_ms(2000); // Delay for debugging

    if (!startup_init()) {
        ERROR_PRINT("Startup initialization failed.\n");
        return -1;
    }

    if (core0_init()) {
        mcp_display_write_pin(FAULT_LED, 0);
        PDU_Display_UpdateStatus("System ready.");
        INFO_PRINT("Core 0 initialized.\n\n");
    } else {
        ERROR_PRINT("Core 0 initialization failed.\n\n");
        return -1;
    }

    if (core1_init()) {
        INFO_PRINT("Core 1 initialized.\n\n");
    } else {
        ERROR_PRINT("Core 1 initialization failed.\n\n");
        return -1;
    }

    // Launch Ethernet/HTTP server handling on Core 1.
    multicore_launch_core1(core1_task);

    // Main loop on Core 0: for other tasks.
    while (true) {
        sleep_ms(1000);
    }

    return 0;
}
