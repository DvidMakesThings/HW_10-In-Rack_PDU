/**
 * @file ILI9488_driver.h
 * @author DvidMakesThings - David Sipos
 * @brief ILI9488 TFT LCD Driver API
 * @version 1.4
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 *
 * @details
 * This header defines the API for controlling the ILI9488 TFT LCD display on the ENERGIS platform.
 * It provides functions for hardware initialization, pixel and rectangle drawing, text rendering,
 * DMA-accelerated graphics, backlight control, and BMP image display. All functions are designed
 * for use on the Raspberry Pi Pico and similar microcontrollers.
 */

#ifndef ILI9488_DRIVER_H
#define ILI9488_DRIVER_H

#include "../CONFIG.h" // Include user-defined configuration
#include "hardware/spi.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// ==== Function Prototypes ====


/**
 * @brief Initializes the ILI9488 display.
 *
 * Performs hardware and software initialization, including reset sequence and configuration
 * of pixel format and memory access control.
 */
void ILI9488_Init(void);

/**
 * @brief Sends a command byte to the ILI9488 display.
 *
 * Sets the display to command mode and transmits a single command byte over SPI.
 *
 * @param cmd Command byte to send.
 */
void ILI9488_SendCommand(uint8_t cmd);

/**
 * @brief Sends a block of data to the ILI9488 display.
 *
 * Sets the display to data mode and transmits a buffer of bytes over SPI.
 *
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to send.
 */
void ILI9488_SendData(const uint8_t *data, uint32_t len);

/**
 * @brief Sets the drawing window for subsequent graphics operations.
 *
 * Defines the rectangular region for pixel, rectangle, or image drawing operations.
 *
 * @param x0 Start X-coordinate.
 * @param y0 Start Y-coordinate.
 * @param x1 End X-coordinate.
 * @param y1 End Y-coordinate.
 */
void ILI9488_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Draws a single pixel at the specified position.
 *
 * Draws a pixel at (x, y) with the specified RGB565 color value.
 *
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param color 16-bit RGB565 color value.
 */
void ILI9488_DrawPixel(uint16_t x, uint16_t y, uint32_t color);

/**
 * @brief Fills the entire screen with a single color using DMA.
 *
 * Uses DMA to rapidly fill the entire display with the specified color.
 *
 * @param color 16-bit RGB565 color value.
 */
void ILI9488_FillScreenDMA(uint32_t color);

/**
 * @brief Draws a filled rectangle (bar) at the specified position.
 *
 * Uses DMA to fill a rectangle of given width and height at (x, y) with the specified color.
 *
 * @param x X-coordinate of the top-left corner.
 * @param y Y-coordinate of the top-left corner.
 * @param width Rectangle width in pixels.
 * @param height Rectangle height in pixels.
 * @param color Fill color (RGB565).
 */
void ILI9488_DrawBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color);

/**
 * @brief Draws a text string at the specified position.
 *
 * Renders a null-terminated string at (x, y) using the built-in 8x12 font and color.
 *
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param text Pointer to the null-terminated string to display.
 * @param color Text color in RGB565 format.
 */
void ILI9488_DrawText(uint16_t x, uint16_t y, const char *text, uint32_t color);

/**
 * @brief Draws a single character using an 8x12 font.
 *
 * Renders a character at (x, y) using the built-in 8x12 font and color.
 *
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param c Character to draw.
 * @param color 16-bit RGB565 color value.
 */
static void ILI9488_DrawChar(uint16_t x, uint16_t y, char c, uint32_t color);

/**
 * @brief Draws a straight horizontal or vertical line using DMA.
 *
 * Uses DMA to draw a line from (x0, y0) to (x1, y0) with the specified color.
 *
 * @param x0 Start X coordinate.
 * @param y0 Start Y coordinate.
 * @param x1 End X coordinate.
 * @param y1 End Y coordinate.
 * @param color 24-bit RGB color value.
 */
void ILI9488_DrawLineDMA(uint16_t x0, uint16_t y0, uint16_t x1, uint32_t color);

/**
 * @brief Sets the LCD backlight brightness using PWM.
 *
 * Configures and sets the PWM duty cycle for the LCD backlight pin.
 *
 * @param brightness Brightness level (0-100%).
 */
void ILI9488_SetBacklight(uint8_t brightness);

/**
 * @brief Draws a BMP image from a file at the specified position.
 *
 * Reads a 24-bit uncompressed BMP file and renders it at (x, y) on the display.
 *
 * @param x Start X-coordinate.
 * @param y Start Y-coordinate.
 * @param filename BMP file path (must be 24-bit uncompressed).
 */
void ILI9488_DrawBitmap(uint16_t x, uint16_t y, const char *filename);

#endif // ILI9488_DRIVER_H