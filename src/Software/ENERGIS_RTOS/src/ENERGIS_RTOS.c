/**
 * @file ENERGIS_RTOS.c
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
 * @brief FreeRTOS malloc failed hook
 * @note Called when pvPortMalloc() fails
 */
void vApplicationMallocFailedHook(void) {
    /* Heap exhausted - fatal error */
    printf("[FATAL] Heap allocation failed!\r\n");
    while (1) {
        tight_loop_contents();
    }
}

/**
 * @brief FreeRTOS stack overflow hook
 * @param t Task handle that overflowed
 * @param n Task name
 */
void vApplicationStackOverflowHook(TaskHandle_t t, char *n) {
    (void)t;
    /* Task stack overflow - fatal error */
    printf("[FATAL] Stack overflow in task: %s\r\n", n ? n : "unknown");
    while (1) {
        tight_loop_contents();
    }
}

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

    // Set system clock to 200 MHz
    set_sys_clock_khz(200000, true);

    // Early BOOTSEL check before anything touches USB
    if (bootloader_trigger == 0xDEADBEEF) {
        bootloader_trigger = 0;
        sleep_ms(100);
        reset_usb_boot(0, 0);
    }

    /* Small delay to let USB enumerate */
    sleep_ms(1000);

    printf("\r\n");
    printf("========================================\r\n");
    printf("\tNERGIS PDU - Bootloader\r\n");
    printf("========================================\r\n");
    printf("\tStarting FreeRTOS...\r\n");
    printf("========================================\r\n\r\n");

    /* ===== CREATE INIT TASK ===== */

    /* InitTask will run at highest priority and handle all bring-up */
    InitTask_Create();

    /* ===== START RTOS SCHEDULER ===== */

    printf("[Main] Starting FreeRTOS scheduler...\r\n\r\n");
    vTaskStartScheduler(); /* Never returns */

    /* ===== SAFETY LOOP (should never reach here) ===== */

    printf("[FATAL] Scheduler returned - should never happen!\r\n");
    for (;;) {
        tight_loop_contents();
    }
}

/** @} */
/** @} */