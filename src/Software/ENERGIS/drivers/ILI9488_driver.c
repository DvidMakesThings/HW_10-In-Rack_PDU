/**
 * @file ILI9488_driver.c
 * @author David Sipos
 * @brief Optimized ILI9488 TFT LCD Driver for 16-bit color (RGB565)
 *        Speed is critical—this version minimizes per-pixel overhead.
 * @version 1.4
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */

 #include "ILI9488_driver.h"
 #include "pico/stdlib.h"
 #include "hardware/spi.h"
 #include "hardware/pwm.h"
 #include "../CONFIG.h"
 #include "utils/font_8x12.h"  // Make sure this file is in your include path
 #include <string.h>
 
 // Local static variables for caching previous status text.
 static uint8_t pwm_slice;
 static char prev_ip[16] = "";
 static char prev_status[32] = "";
 
 /**
  * @brief Sends a command or a data byte to the display.
  *        This function uses spi_write_blocking which always transmits 8-bit chunks.
  * @param data The byte to send.
  * @param isCommand True if sending a command; false for data.
  */
 static inline void ILI9488_Write(uint8_t data, bool isCommand) {
     gpio_put(LCD_DC, isCommand ? 0 : 1);
     gpio_put(LCD_CS, 0);
     spi_write_blocking(ILI9488_SPI_INSTANCE, &data, 1);
     gpio_put(LCD_CS, 1);
 }
 
 void ILI9488_SendCommand(uint8_t cmd) {
     ILI9488_Write(cmd, true);
 }
 
 void ILI9488_SendData(const uint8_t *data, uint32_t len) {
     gpio_put(LCD_DC, 1);
     gpio_put(LCD_CS, 0);
     spi_write_blocking(ILI9488_SPI_INSTANCE, data, len);
     gpio_put(LCD_CS, 1);
 }
 
 void ILI9488_Init(void) {
     // Initialize GPIOs
     gpio_init(LCD_CS);
     gpio_set_dir(LCD_CS, GPIO_OUT);
     gpio_put(LCD_CS, 1);
 
     gpio_init(LCD_DC);
     gpio_set_dir(LCD_DC, GPIO_OUT);
 
     gpio_init(LCD_RESET);
     gpio_set_dir(LCD_RESET, GPIO_OUT);
 
     // Configure SPI at the speed defined in CONFIG.h (62.5 MHz)
     spi_init(ILI9488_SPI_INSTANCE, SPI_SPEED);
     gpio_set_function(LCD_SCLK, GPIO_FUNC_SPI);
     gpio_set_function(LCD_MOSI, GPIO_FUNC_SPI);
     gpio_set_function(LCD_MISO, GPIO_FUNC_SPI);
 
     // Hardware Reset
     gpio_put(LCD_RESET, 1);
     sleep_ms(10);
     gpio_put(LCD_RESET, 0);
     sleep_ms(50);
     gpio_put(LCD_RESET, 1);
     sleep_ms(10);
 
     // Wake up display and turn it on
     ILI9488_SendCommand(ILI9488_CMD_SLEEP_OUT);
     sleep_ms(120);
     ILI9488_SendCommand(ILI9488_CMD_DISPLAY_ON);
     sleep_ms(50);
 
     // Set backlight to full brightness by default
     ILI9488_SetBacklight(100);
 }
 
 void ILI9488_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
     uint8_t data[4];
     // Set column address
     ILI9488_SendCommand(ILI9488_CMD_COLUMN_ADDR);
     data[0] = x0 >> 8; data[1] = x0 & 0xFF;
     data[2] = x1 >> 8; data[3] = x1 & 0xFF;
     ILI9488_SendData(data, 4);
 
     // Set page address
     ILI9488_SendCommand(ILI9488_CMD_PAGE_ADDR);
     data[0] = y0 >> 8; data[1] = y0 & 0xFF;
     data[2] = y1 >> 8; data[3] = y1 & 0xFF;
     ILI9488_SendData(data, 4);
 }
 
 void ILI9488_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
     uint8_t data[2] = { color >> 8, color & 0xFF };
     ILI9488_SetAddressWindow(x, y, x, y);
     ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);
     ILI9488_SendData(data, 2);
 }
 
 /**
  * @brief Draws a single character using an 8x12 font.
  * @param x The x-coordinate for the character.
  * @param y The y-coordinate for the character.
  * @param c The character to draw.
  * @param color The color to use (RGB565).
  */
 static void ILI9488_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color) {
     // Only support printable characters 32-126
     if (c < 32 || c > 126) return;
     const unsigned char *charData = font_8x12[c - 32];
     for (int row = 0; row < 12; row++) {
         for (int col = 0; col < 8; col++) {
             if (charData[row] & (1 << (7 - col))) {
                 ILI9488_DrawPixel(x + col, y + row, color);
             }
         }
     }
 }
 
 /**
  * @brief Draws a string of text on the display.
  * @param x The starting x-coordinate.
  * @param y The starting y-coordinate.
  * @param text The null-terminated string to display.
  * @param color The text color (RGB565).
  */
 void ILI9488_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color) {
     while (*text) {
         ILI9488_DrawChar(x, y, *text, color);
         x += 8;  // Advance by the width of a character (8 pixels)
         text++;
     }
 }
 
 /**
  * @brief Optimized FillScreen function.
  *        Instead of sending each pixel individually, this function builds a row buffer and sends it for every row.
  */
 void ILI9488_FillScreen(uint16_t color) {
     uint16_t width = ILI9488_WIDTH;
     uint16_t height = ILI9488_HEIGHT;
     uint16_t bytesPerRow = width * 2;
     
     // Build a row buffer on the stack (assume maximum width <= 320 pixels, adjust if needed)
     uint8_t rowBuffer[640];
     uint8_t high = color >> 8;
     uint8_t low = color & 0xFF;
     for (uint16_t i = 0; i < width; i++) {
         rowBuffer[i * 2] = high;
         rowBuffer[i * 2 + 1] = low;
     }
     
     ILI9488_SetAddressWindow(0, 0, width - 1, height - 1);
     ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);
     
     gpio_put(LCD_DC, 1);
     gpio_put(LCD_CS, 0);
     for (uint16_t y = 0; y < height; y++) {
         spi_write_blocking(ILI9488_SPI_INSTANCE, rowBuffer, bytesPerRow);
     }
     gpio_put(LCD_CS, 1);
 }
 
 /**
  * @brief Draws a filled rectangle (UI elements).
  *        It creates a row buffer for the given width and sends it for each row of the rectangle.
  */
 void ILI9488_DrawBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
     ILI9488_SetAddressWindow(x, y, x + width - 1, y + height - 1);
     ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);
     
     uint16_t bytesPerRow = width * 2;
     uint8_t rowBuffer[bytesPerRow];
     uint8_t high = color >> 8;
     uint8_t low = color & 0xFF;
     for (uint16_t i = 0; i < width; i++) {
         rowBuffer[i * 2] = high;
         rowBuffer[i * 2 + 1] = low;
     }
     
     gpio_put(LCD_DC, 1);
     gpio_put(LCD_CS, 0);
     for (uint16_t j = 0; j < height; j++) {
         spi_write_blocking(ILI9488_SPI_INSTANCE, rowBuffer, bytesPerRow);
     }
     gpio_put(LCD_CS, 1);
 }
 
 void ILI9488_SetBacklight(uint8_t brightness) {
     if (brightness > 100) brightness = 100;
     uint16_t level = (brightness * 65535) / 100;
     pwm_set_gpio_level(LCD_BL, level);
 }