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
 * @version 2.1.0
 * @date 2025-12-10
 *
 * @details Main entry point using InitTask pattern for controlled bring-up.
 * Minimal initialization in main() context - all hardware init happens in InitTask.
 *
 * v2.1.0 Changes:
 * - PERFORMANCE: Enabled 200 MHz system clock for improved SNMP stress handling
 * - 60% faster CPU allows better handling of rapid network traffic
 * - Must be paired with matching configCPU_CLOCK_HZ in FreeRTOSConfig.h
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "CONFIG.h"

#define MAIN_TAG "[MAIN] "

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
 * 1. Set system clock to 200 MHz for better network performance
 * 2. Initialize stdio (USB-CDC) for early debug output
 * 3. Create InitTask (high priority bring-up task)
 * 4. Start FreeRTOS scheduler
 *
 * All hardware initialization and task creation happens in InitTask context.
 */
int main(void) {
    /* ===== MINIMAL INIT IN MAIN CONTEXT ===== */
    Helpers_EarlyBootSnapshot();

    /* Set system clock to 200 MHz for better network performance.
     * This provides ~60% more CPU headroom for handling rapid SNMP traffic
     * and reduces SPI transaction overhead proportionally.
     *
     * IMPORTANT: configCPU_CLOCK_HZ in FreeRTOSConfig.h must match this value
     * (200000000UL) for correct tick timing.
     */
    set_sys_clock_khz(200000, true);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, 200000000, 200000000);

    /* Small delay to let USB enumerate */
    sleep_ms(1000);

    /* ===== CREATE INIT TASK ===== */

    /* InitTask will run at highest priority and handle all bring-up */
    InitTask_Create();

    /* ===== START RTOS SCHEDULER ===== */

    log_printf("[Main] Starting FreeRTOS scheduler (CPU @ 200 MHz)...\r\n\r\n");
    vTaskStartScheduler(); /* Never returns */

    /* ===== SAFETY LOOP (should never reach here) ===== */
#if ERRORLOGGER
    uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_FATAL_ERROR, ERR_FID_INIT_MAIN, 0x0);
    ERROR_PRINT_CODE(errorcode, "%s Scheduler returned - should never happen!\r\n", MAIN_TAG);
    Storage_EnqueueErrorCode(errorcode);
#endif
    for (;;) {
        tight_loop_contents();
    }
}

/** @} */
/** @} */