/**
 * @file PDU_display.h
 * @author David Sipos
 * @brief Display UI handling for the Energis PDU.
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */

 #ifndef PDU_DISPLAY_H
 #define PDU_DISPLAY_H
 
 #include <stdint.h>
 #include <stdbool.h>
 
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
 */
void PDU_Display_UpdateState(uint8_t channel, const char *state);

/**
 * @brief Updates the voltage display for a given channel.
 */
void PDU_Display_UpdateVoltage(uint8_t channel, float voltage);

/**
 * @brief Updates the current display for a given channel.
 */
void PDU_Display_UpdateCurrent(uint8_t channel, float current);

/**
 * @brief Updates the displayed IP address.
 */
void PDU_Display_UpdateIP(const uint8_t ip[4]);

/**
 * @brief Updates the system status message.
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


#endif // PDU_DISPLAY_H