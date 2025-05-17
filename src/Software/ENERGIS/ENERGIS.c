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

#include "CONFIG.h"

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

    sleep_ms(5000); // Delay for debugging

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

    // ————————— Hook in IRQ-driven UART command parser —————————

    // Launch Ethernet/HTTP server handling on Core 1.
    multicore_launch_core1(core1_task);

    while (true) {
        uart_command_loop(); // Blocks until a command is received
    }
}