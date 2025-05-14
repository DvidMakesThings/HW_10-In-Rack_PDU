/**
 * @file ENERGIS.c
 * @brief Main application for ENERGIS PDU.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 *
 * This file serves as the entry point for the ENERGIS PDU application.
 */

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "misc/uart_command_handler.h"
#include "pico/bootrom.h"
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
__attribute__((section(".uninitialized_data"))) uint32_t bootloader_trigger;

//------------------------------------------------------------------------------
// main: Performs system initialization on Core 0 and launches core1_task on Core 1.
int main(void) {
    // Early BOOTSEL check before anything touches USB
    if (bootloader_trigger == 0xDEADBEEF) {
        bootloader_trigger = 0;
        sleep_ms(100);
        reset_usb_boot(0, 0);
    }

    sleep_ms(2000); // Delay for debugging

    if (!startup_init()) {
        ERROR_PRINT("Startup initialization failed.\n");
        return -1;
    }

    if (core0_init()) {
        uart_command_init(); // sets up IRQ for command listening
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

    while (true) {
        if (bootsel_requested) {
            // Set trigger and reboot — BOOTSEL handled on next boot
            bootloader_trigger = 0xDEADBEEF;
            watchdog_reboot(0, 0, 0);
            while (1)
                __wfi(); // hang until reboot
        }

        sleep_ms(10);
    }
}