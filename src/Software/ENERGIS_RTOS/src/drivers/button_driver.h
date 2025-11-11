/**
 * @file src/drivers/button_driver.h
 * @author DvidMakesThings - David Sipos
 * 
 * @defgroup driver3 3. Button Task Driver
 * @ingroup drivers
 * @brief Header file for pushbutton driver
 * @{
 * 
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details Low-level driver implementation for buttons.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ENERGIS_BUTTON_DRIVER_H
#define ENERGIS_BUTTON_DRIVER_H

#include "../CONFIG.h"

/**
 * @brief Initialize button GPIOs (inputs, pull-ups) and any selection outputs.
 * @details
 *   - Configures BUT_PLUS, BUT_MINUS, BUT_SET as inputs with pull-ups.
 *   - Leaves all selection LEDs OFF.
 */
void ButtonDrv_InitGPIO(void);

/**
 * @brief Read PLUS button raw logic level.
 * @return true if pin is HIGH (not pressed), false if LOW (pressed, active-low).
 */
bool ButtonDrv_ReadPlus(void);

/**
 * @brief Read MINUS button raw logic level.
 * @return true if pin is HIGH (not pressed), false if LOW (pressed, active-low).
 */
bool ButtonDrv_ReadMinus(void);

/**
 * @brief Read SET button raw logic level.
 * @return true if pin is HIGH (not pressed), false if LOW (pressed, active-low).
 */
bool ButtonDrv_ReadSet(void);

/**
 * @brief Turn OFF all selection LEDs.
 */
void ButtonDrv_SelectAllOff(void);

/**
 * @brief Show or hide a specific selection LED.
 * @param index 0..7 channel index.
 * @param on    true to turn LED on, false to turn it off.
 */
void ButtonDrv_SelectShow(uint8_t index, bool on);

/**
 * @brief Move selection one step LEFT with wrap-around and update LEDs.
 * @param io_index [in,out] current index (0..7) updated in place.
 * @param led_on   true to display the new selection LED, false to keep off.
 */
void ButtonDrv_SelectLeft(uint8_t *io_index, bool led_on);

/**
 * @brief Move selection one step RIGHT with wrap-around and update LEDs.
 * @param io_index [in,out] current index (0..7) updated in place.
 * @param led_on   true to display the new selection LED, false to keep off.
 */
void ButtonDrv_SelectRight(uint8_t *io_index, bool led_on);

/**
 * @brief Perform the short-press action for SET on a channel.
 * @param index Channel index (0..7) to toggle.
 * @note Toggles relay state and mirrors the channel LED accordingly.
 */
void ButtonDrv_DoSetShort(uint8_t index);

/**
 * @brief Perform the long-press action for SET.
 * @note Clears fault LED on display MCP, if present.
 */
void ButtonDrv_DoSetLong(void);

/**
 * @brief Monotonic milliseconds since boot (driver-provided).
 * @return Milliseconds counter as uint32_t.
 */
uint32_t ButtonDrv_NowMs(void);

#endif /* ENERGIS_BUTTON_DRIVER_H */

/** @} */