/**
 * @file ILI9488_driver.h
 * @author David Sipos
 * @brief Header file for the ILI9488 TFT LCD Driver
 * @version 1.4
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ILI9488_DRIVER_H
#define ILI9488_DRIVER_H

#include <stdint.h>
#include "../CONFIG.h"  // Include CONFIG.h for definitions
#include "hardware/spi.h"

// ==== Function Prototypes ==== 

/**
 * @brief Initializes the ILI9488 display.
 * 
 * Configures SPI1, initializes GPIOs, resets the display, and enables it.
 */
void ILI9488_Init(void);

/**
 * @brief Sends a command to the display.
 * @param cmd Command byte to be sent.
 */
void ILI9488_SendCommand(uint8_t cmd);

/**
 * @brief Sends a block of data to the display.
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to send.
 */
void ILI9488_SendData(const uint8_t *data, uint32_t len);

/**
 * @brief Fills the entire screen with a single color.
 * @param color 16-bit RGB565 color value.
 */
void ILI9488_FillScreen(uint16_t color);

/**
 * @brief Sets the drawing window for partial updates.
 * @param x0 Start X-coordinate.
 * @param y0 Start Y-coordinate.
 * @param x1 End X-coordinate.
 * @param y1 End Y-coordinate.
 */
void ILI9488_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Draws a single pixel at a specified position.
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param color 16-bit RGB565 color.
 */
void ILI9488_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief Draws a string of text at a given position.
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param text Pointer to the string to be displayed.
 * @param color Text color in RGB565 format.
 */
void ILI9488_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color);

/**
 * @brief Draws a filled rectangle (UI elements).
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param width Rectangle width.
 * @param height Rectangle height.
 * @param color Fill color (RGB565).
 */
void ILI9488_DrawBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/**
 * @brief Updates only changed regions of the display (e.g., IP address, system status).
 * @param ip New IP address string.
 * @param status New system status string.
 */
void ILI9488_UpdateDisplay(const char *ip, const char *status);

/**
 * @brief Sets the LCD backlight brightness using PWM.
 * @param brightness Brightness level (0-100%).
 */
void ILI9488_SetBacklight(uint8_t brightness);

#endif // ILI9488_DRIVER_H