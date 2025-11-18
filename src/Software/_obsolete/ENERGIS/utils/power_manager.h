/**
 * @file power_manager.h
 * @author
 *  DvidMakesThings - David Sipos
 * @brief Power gating and long-press handling for ENERGIS PDU.
 * @version 1.2
 * @date 2025-09-08
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 *
 * @details
 * Provides a simple on/off control layer:
 * - Detects a 1 s long-press on the power key (BUT_PWR) via button_driver.
 * - Toggles the external rail enable (VREG_EN) which powers all peripherals.
 * - Invokes user callbacks before power-off and after power-on.
 * Integration: the button driver detects a validated long-press on BUT_PWR and
 * calls power_manager_request_toggle_from_isr(). The main loop must call
 * power_manager_process() to perform the actual toggle outside IRQ context.
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Callback type for power transition hooks.
 * @return void
 */
typedef void (*pm_callback_t)(void);

/**
 * @brief Long-press duration threshold in milliseconds for the power key.
 * @return void
 */
#ifndef PWR_LONG_MS
#define PWR_LONG_MS 1000u
#endif

/**
 * @brief Debounce time for the power key in milliseconds.
 * @return void
 */
#ifndef PWR_DEBOUNCE_MS
#define PWR_DEBOUNCE_MS 40u
#endif

/**
 * @brief Button electrical polarity selector.
 *        Set to 1 if BUT_PWR is active-low (pressed pulls to GND), else 0.
 * @return void
 */
#ifndef PWR_ACTIVE_LOW
#define PWR_ACTIVE_LOW 1
#endif

/**
 * @brief Configure internal pulls for BUT_PWR.
 *        If active-low (default), enables pull-up; else enables pull-down.
 * @return void
 */
#ifndef PWR_CONFIGURE_PULLS
#define PWR_CONFIGURE_PULLS 1
#endif

/**
 * @brief Initialize the power manager (VREG_EN GPIO and state).
 * @return void
 */
void power_manager_init(void);

/**
 * @brief Register optional callbacks for power transitions.
 * @param before_off Callback invoked just before rails are turned OFF; may be NULL.
 * @param after_on   Callback invoked just after rails are turned ON; may be NULL.
 * @return void
 */
void power_manager_set_callbacks(pm_callback_t before_off, pm_callback_t after_on);

/**
 * @brief Signal a power-toggle request from ISR context (validated long-press).
 *        The actual toggle is deferred to power_manager_process().
 * @return void
 */
void power_manager_request_toggle_from_isr(void);

/**
 * @brief Perform any pending power toggles and handle transitions.
 *        Call this regularly from the main loop (non-IRQ context).
 * @return void
 */
void power_manager_process(void);

/**
 * @brief Query the current peripherals rail state.
 * @return true if peripherals are powered (VREG_EN = 1), otherwise false.
 */
bool power_is_on(void);

/**
 * @brief Service the power button: on first long-press turn peripherals OFF, on next long-press
 * turn them ON and reset MCU.
 * @return void
 * @note Call this function periodically (e.g., every 5–20 ms) from the main loop.
 *       Requires macros: BUT_PWR, VREG_EN, PWR_ACTIVE_LOW (0/1), PWR_LONG_MS, PWR_DEBOUNCE_MS.
 *       MCU stays powered; peripherals rail is controlled by VREG_EN (1=ON, 0=OFF).
 *       When turning back ON, the MCU is reset via watchdog_reboot() to re-run full init.
 */
void power_button_service(void);

#endif /* POWER_MANAGER_H */
