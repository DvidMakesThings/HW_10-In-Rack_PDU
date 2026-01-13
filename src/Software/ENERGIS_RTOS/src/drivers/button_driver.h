/**
 * @file src/drivers/button_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers Drivers
 * @brief HAL drivers for the Energis PDU firmware.
 * @{
 *
 * @defgroup driver01 1. Button Driver
 * @ingroup drivers
 * @brief Hardware abstraction layer for front-panel pushbutton controls and selection LEDs.
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * This driver provides low-level access to the ENERGIS PDU front panel controls:
 * - Three pushbuttons (PLUS, MINUS, SET) with active-low inputs and pull-ups
 * - Eight channel selection LEDs driven via MCP23017 GPIO expander
 *
 * The driver uses direct GPIO access for button reads and delegates LED control
 * to SwitchTask for thread-safe I2C operations. All functions are non-blocking
 * and safe to call from any task context.
 *
 * Hardware Configuration:
 * - Buttons are active-low with internal pull-ups
 * - Selection LEDs are controlled via SwitchTask message queue
 * - No debouncing is performed (handled by ButtonTask)
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ENERGIS_BUTTON_DRIVER_H
#define ENERGIS_BUTTON_DRIVER_H

#include "../CONFIG.h"

/** @name Public API
 *  @ingroup driver01
 *  @{ */
/**
 * @brief Initialize button GPIO pins and selection LED hardware.
 *
 * @details
 * Configures button GPIOs as inputs with pull-ups enabled:
 * - BUT_PLUS: Channel increment button
 * - BUT_MINUS: Channel decrement button
 * - BUT_SET: Channel selection/action button
 *
 * All selection LEDs are turned off after initialization.
 * This function must be called once during system startup.
 *
 * @param None
 * @return None
 *
 * @note Thread-safe, but should only be called once during initialization
 * @note Requires SwitchTask to be ready for LED control
 */
void ButtonDrv_InitGPIO(void);

/**
 * @brief Read current state of PLUS button.
 *
 * @param None
 * @return true if button is not pressed (pin HIGH)
 * @return false if button is pressed (pin LOW, active-low)
 *
 * @note No debouncing performed, call periodically for edge detection
 * @note Direct GPIO read, no I2C access
 */
bool ButtonDrv_ReadPlus(void);

/**
 * @brief Read current state of MINUS button.
 *
 * @param None
 * @return true if button is not pressed (pin HIGH)
 * @return false if button is pressed (pin LOW, active-low)
 *
 * @note No debouncing performed, call periodically for edge detection
 * @note Direct GPIO read, no I2C access
 */
bool ButtonDrv_ReadMinus(void);

/**
 * @brief Read current state of SET button.
 *
 * @param None
 * @return true if button is not pressed (pin HIGH)
 * @return false if button is pressed (pin LOW, active-low)
 *
 * @note No debouncing performed, call periodically for edge detection
 * @note Direct GPIO read, no I2C access
 */
bool ButtonDrv_ReadSet(void);

/**
 * @brief Turn off all eight channel selection LEDs.
 *
 * @details
 * Sends command to SwitchTask to clear all selection indicators.
 * Operation is queued and non-blocking.
 *
 * @param None
 * @return None
 *
 * @note Requires SwitchTask to be initialized and running
 */
void ButtonDrv_SelectAllOff(void);

/**
 * @brief Control visibility of a single channel selection LED.
 *
 * @details
 * Sends command to SwitchTask to update LED state. Operation is queued
 * and returns immediately. Invalid channel indices are rejected with
 * error logging.
 *
 * @param index Channel index [0..7]
 * @param on LED state: true = illuminate, false = extinguish
 *
 * @return None
 *
 * @note Non-blocking, queued operation via SwitchTask
 * @note Errors logged if index out of range or SwitchTask not ready
 */
void ButtonDrv_SelectShow(uint8_t index, bool on);

/**
 * @brief Move selection indicator one position left with wraparound.
 *
 * @details
 * Decrements channel index with wraparound (0 wraps to 7).
 * Updates LED display if led_on is true. Index is modified in-place.
 *
 * @param io_index Pointer to current channel index [0..7], updated to new position
 * @param led_on If true, illuminate LED at new position; if false, leave LEDs unchanged
 *
 * @return None
 *
 * @note NULL pointer check performed with error logging
 * @note Wraparound behavior: 0 -> 7
 */
void ButtonDrv_SelectLeft(uint8_t *io_index, bool led_on);

/**
 * @brief Move selection indicator one position right with wraparound.
 *
 * @details
 * Increments channel index with wraparound (7 wraps to 0).
 * Updates LED display if led_on is true. Index is modified in-place.
 *
 * @param io_index Pointer to current channel index [0..7], updated to new position
 * @param led_on If true, illuminate LED at new position; if false, leave LEDs unchanged
 *
 * @return None
 *
 * @note NULL pointer check performed with error logging
 * @note Wraparound behavior: 7 -> 0
 */
void ButtonDrv_SelectRight(uint8_t *io_index, bool led_on);

/**
 * @brief Execute short-press action for SET button on specified channel.
 *
 * @details
 * Toggles relay state for the given channel via SwitchTask. The relay
 * control is queued and non-blocking. Channel LED mirrors the new relay state.
 *
 * @param index Channel index [0..7] to toggle
 *
 * @return None
 *
 * @note Uses Switch_Toggle for thread-safe relay control
 * @note Invalid indices (>7) are silently ignored
 */
void ButtonDrv_DoSetShort(uint8_t index);

/**
 * @brief Execute long-press action for SET button.
 *
 * @details
 * Clears the fault LED on the display MCP if FAULT_LED is defined.
 * Routes command through SwitchTask to prevent I2C bus contention.
 *
 * @param None
 * @return None
 *
 * @note Only active if FAULT_LED is defined in hardware configuration
 * @note Fallback to direct MCP access if SwitchTask not ready (early boot)
 */
void ButtonDrv_DoSetLong(void);

/**
 * @brief Get monotonic millisecond timestamp since boot.
 *
 * @details
 * Provides system uptime in milliseconds for button timing and debounce logic.
 * Uses Pico SDK absolute time functions for microsecond precision.
 *
 * @param None
 * @return uint32_t Milliseconds since system boot
 *
 * @note Wraps after ~49.7 days
 * @note Suitable for relative timing comparisons
 */
uint32_t ButtonDrv_NowMs(void);
/** @} */

#endif /* ENERGIS_BUTTON_DRIVER_H */

/** @} */
/** @} */