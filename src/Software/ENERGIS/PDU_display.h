/**
 * @file PDU_display.h
 * @author DvidMakesThings - David Sipos
 * @brief Display UI handling for the Energis PDU.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef PDU_DISPLAY_H
#define PDU_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#define SYS_INFO_LEN 31

/**
 * @brief Initializes the PDU display.
 */
void PDU_Display_Init(void);

/**
 * @brief Draws the static UI elements (table, labels, and lines).
 */
void PDU_Display_DrawStaticUI(void);

/**
 * @brief Updates the ON/OFF state of a given channel.
 * @param channel The channel number (1-8).
 * @param state The state to set (ON or OFF).
 */
void PDU_Display_UpdateState(uint8_t channel, const char *state);

/**
 * @brief Updates the voltage display for a given channel.
 * @param channel The channel number (1-8).
 * @param voltage The voltage value to display.
 */
void PDU_Display_UpdateVoltage(uint8_t channel, float voltage);

/**
 * @brief Updates the current display for a given channel.
 * @param channel The channel number (1-8).
 * @param current The current value to display.
 */
void PDU_Display_UpdateCurrent(uint8_t channel, float current);

/**
 * @brief Updates the displayed IP address.
 * @param ip The IP address to display (4 bytes).
 */
void PDU_Display_UpdateIP(const uint8_t ip[4]);

/**
 * @brief Updates the system status message.
 * @param status The status message to display.
 */
void PDU_Display_UpdateStatus(const char *status);

/**
 * @brief Shows the connection status.
 * @param connected 1 if connected, 0 otherwise.
 */
void PDU_Display_UpdateConnectionStatus(uint8_t connected);

/**
 * @brief Displays a "saved" message for 5 seconds when EEPROM save occurs.
 */
void PDU_Display_ShowEEPROM_Saved(void);

/**
 * @brief Draws the background image on the display.
 */
void PDU_Display_DrawBackground(void);

/**
 * @brief Updates the selection indicator (*) in the menu.
 * @param row The row number to highlight (0-7).
 * @note The row number corresponds to the relay channel (1-8).
 */
void PDU_Display_UpdateSelection(uint8_t row);

/**
 * @brief Toggles the selected relay ON/OFF.
 * @param channel The channel number (1-8).
 * @note This function is obsolete since the dual MCP driver was introduced.
 */
void PDU_Display_ToggleRelay(uint8_t channel);

/**
 * @brief Handles the power button behavior (short press = sleep, long press = wake-up).
 * @param long_press True if the button was long-pressed, false if short-pressed.
 * @note This function is called when the power button is pressed.
 */
void PDU_Display_HandlePowerButton(bool long_press);

#endif // PDU_DISPLAY_H
