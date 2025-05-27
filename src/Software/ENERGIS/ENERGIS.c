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
    // Set core voltage to 1.25V (must be first)
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

    // sleep_ms(4000); // Delay for debugging

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

    // Launch Ethernet/HTTP server handling on Core 1.
    multicore_launch_core1(core1_task);

    uint64_t last_ts = to_ms_since_boot(get_absolute_time());

    while (1) {
        // 1) UART
        uart_command_loop();
        /*
        // 2) time check
        uint64_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_ts >= POLL_INTERVAL_MS) {
            last_ts = now;
            for (uint8_t ch = 1; ch <= NUM_CHANNELS; ch++) {
                bool ok = hlw8032_read(ch - 1); // driver takes 0–7
                float v = ok ? hlw8032_get_voltage() : 9999.0f;
                float i = ok ? hlw8032_get_current() : 9999.0f;

                PDU_Display_UpdateVoltage(ch, v);
                PDU_Display_UpdateCurrent(ch, i);
            }
        }
        */
        // 3) tiny yield
        sleep_us(100);
    }
}