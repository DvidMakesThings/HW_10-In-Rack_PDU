/**
 * @file ENERGIS.c
 * @author DvidMakesThings - David Sipos
 * @defgroup main Main
 * @brief Main files for the Energis PDU firmware.
 * @{
 *
 * @defgroup main01 1. ENERGIS Main file - Core 0
 * @ingroup main
 * @brief Main entry point for the ENERGIS PDU firmware.
 * @{
 * @version 1.0
 * @date 2025-05-17
 *
 * @details This file contains the main entry point for the ENERGIS PDU firmware.
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "CONFIG.h"
#include "hardware/vreg.h"

wiz_NetInfo g_net_info;

/**
 * @brief Global variable to trigger BOOTSEL mode.
 *
 * This variable is used to trigger the BOOTSEL mode on the next reboot.
 */
__attribute__((section(".uninitialized_data"))) uint32_t bootloader_trigger;

/**
 * @brief Main function for the ENERGIS PDU firmware.
 * @return int
 * @param None
 * @note This function is the entry point for the ENERGIS PDU firmware.
 */
int main(void) {
    // Set core voltage to 1.20V (must be first)
    vreg_set_voltage(VREG_VOLTAGE_1_20);

    // Let voltage settle properly
    sleep_ms(1000); // do not replace with volatile delay, this is critical

    // Set system clock to 200 MHz
    set_sys_clock_khz(200000, true);

    // Early BOOTSEL check before anything touches USB
    if (bootloader_trigger == 0xDEADBEEF) {
        bootloader_trigger = 0;
        sleep_ms(100);
        reset_usb_boot(0, 0);
    }

    // replace stdio_usb_init();
    stdio_usb_init();

    if (!startup_init()) {
        setError(true);
        ERROR_PRINT("Startup initialization failed.\n");
        return -1;
    }

    if (core0_init()) {
        setError(false);
        if (HAS_SCREEN)
            PDU_Display_UpdateStatus("System ready.");
        DEBUG_PRINT("Core 0 initialized.\n\n");
    } else {
        setError(true);
        ERROR_PRINT("Core 0 initialization failed.\n\n");
        return -1;
    }

    if (core1_init()) {
        DEBUG_PRINT("Core 1 initialized.\n\n");
        printf("SYSTEM READY\n");
    } else {
        setError(true);
        ERROR_PRINT("Core 1 initialization failed.\n\n");
        return -1;
    }

    /*  NOTE:
     *  - hlw8032_init() is already called from startup_init().
     *  - Core0 is the PRODUCER: every POLL_INTERVAL_MS it refreshes ALL 8 channels
     *    in a blocking loop and stores results in SRAM cache.
     *  - Core1 (launched below) is the CONSUMER: it serves Web/SNMP using only
     *    the cached values (non‑blocking).
     */
    multicore_launch_core1(core1_task);

    uint32_t last_poll_us = time_us_64();

    while (1) {
        uart_command_loop(); // stay responsive always
        // mcp_dual_guardian_poll(); // << watchdog for relay/display divergence
        uint64_t now_us = time_us_64();
        if ((now_us - last_poll_us) >= 100000) { // 100ms per channel
            hlw8032_poll_once();                 // non-blocking: one channel per loop
            last_poll_us = now_us;
        }
    }
}