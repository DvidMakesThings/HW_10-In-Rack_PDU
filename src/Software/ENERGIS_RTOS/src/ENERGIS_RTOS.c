/**
 * @file src/ENERGIS_RTOS.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup main Main
 * @brief Main files for the Energis PDU firmware.
 * @{
 *
 * @defgroup main01 1. ENERGIS Main file - Core 0
 * @ingroup main
 * @brief Main entry point for the ENERGIS PDU firmware.
 * @{
 *
 * @version 2.0.0
 * @date 2025-11-08
 *
 * @details Main entry point using InitTask pattern for controlled bring-up.
 * Minimal initialization in main() context - all hardware init happens in InitTask.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "CONFIG.h"

/**
 * @brief Global variable to trigger BOOTSEL mode.
 *
 * This variable is used to trigger the BOOTSEL mode on the next reboot.
 */
__attribute__((section(".uninitialized_data"))) uint32_t bootloader_trigger;

w5500_NetConfig eth_netcfg;

/**
 * @brief Main entry point
 *
 * Performs minimal initialization:
 * 1. Initialize stdio (USB-CDC) for early debug output
 * 2. Create InitTask (high priority bring-up task)
 * 3. Start FreeRTOS scheduler
 *
 * All hardware initialization and task creation happens in InitTask context.
 */
int main(void) {
    /* ===== MINIMAL INIT IN MAIN CONTEXT ===== */
    Helpers_EarlyBootSnapshot();

    // Set system clock to 200 MHz
    // set_sys_clock_khz(200000, true);

    /* Small delay to let USB enumerate */
    sleep_ms(1000);

    /* ===== CREATE INIT TASK ===== */

    /* InitTask will run at highest priority and handle all bring-up */
    InitTask_Create();

    /* ===== START RTOS SCHEDULER ===== */

    log_printf("[Main] Starting FreeRTOS scheduler...\r\n\r\n");
    vTaskStartScheduler(); /* Never returns */

    /* ===== SAFETY LOOP (should never reach here) ===== */

    log_printf("[FATAL] Scheduler returned - should never happen!\r\n");
    for (;;) {
        tight_loop_contents();
    }
}

/** @} */
/** @} */