/**
 * @file ENERGIS.c
 * @author DvidMakesThings - David Sipos
 * @brief Main entry point for the ENERGIS PDU firmware.
 * @version 1.0
 * @date 2025-05-17
 *
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

    stdio_usb_init();

    if (!startup_init()) {
        ERROR_PRINT("Startup initialization failed.\n");
        return -1;
    }

    if (core0_init()) {
        mcp_display_write_pin(FAULT_LED, 0);
        PDU_Display_UpdateStatus("System ready.");
        DEBUG_PRINT("Core 0 initialized.\n\n");
    } else {
        ERROR_PRINT("Core 0 initialization failed.\n\n");
        return -1;
    }

    if (core1_init()) {
        DEBUG_PRINT("Core 1 initialized.\n\n");
    } else {
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

    uint32_t last_poll_ms = to_ms_since_boot(get_absolute_time());

    while (1) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if ((now_ms - last_poll_ms) >= POLL_INTERVAL_MS) {
            hlw8032_refresh_all();       // refresh ALL channels (blocking, once/5s)
            last_poll_ms = now_ms;
        }

        uart_command_loop();             // keep CLI responsive
        sleep_us(100);                   // tiny idle to be kind to CPU
    }
}

