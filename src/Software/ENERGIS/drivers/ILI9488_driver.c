/**
* @file ILI9488_driver.c
* @author David Sipos
* @brief Optimized ILI9488 TFT LCD Driver for 16-bit color (RGB565)
* @version 1.4
* @date 2025-03-03
* 
* @project ENERGIS - The Managed PDU Project for 10-Inch Rack
*/

#include "ILI9488_driver.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h" 
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include <string.h>
#include <stdio.h>          // For printf()
 
// ==== Internal Function Prototypes ====
static inline void ILI9488_Write(uint8_t data, bool isCommand);
 
/**
 * @brief Initializes the ILI9488 display.
*/
void ILI9488_Init(void) {
    gpio_put(LCD_BL, 1);  // Release LCD reset
    // Reset the display
    gpio_put(LCD_RESET, 1);
    sleep_ms(10);
    gpio_put(LCD_RESET, 0);
    sleep_ms(100);
    gpio_put(LCD_RESET, 1);
    sleep_ms(10);
 
    // Initialize the display
    ILI9488_SendCommand(0x01);  
    sleep_ms(50);
    ILI9488_SendCommand(0x11);
    sleep_ms(50);
    ILI9488_SendCommand(0x29);  
    sleep_ms(10);
    ILI9488_SendCommand(0x3A);  
    uint8_t pixelFormat = 0x66;
    ILI9488_SendData(&pixelFormat, 1);
    ILI9488_SendCommand(0x36);  
    uint8_t memAccessCtrl = 0x60;
    ILI9488_SendData(&memAccessCtrl, 1);
}
  
 
/**
 * @brief Sends a command to the display.
 * @param cmd Command byte to be sent.
 */
void ILI9488_SendCommand(uint8_t cmd) {
    gpio_put(LCD_DC, 0);
    spi_write_blocking(ILI9488_SPI_INSTANCE, &cmd, 1);
}
 
/**
 * @brief Sends a block of data to the display.
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to send.
 */
void ILI9488_SendData(const uint8_t *data, uint32_t len) {
    gpio_put(LCD_DC, 1);
    spi_write_blocking(ILI9488_SPI_INSTANCE, data, len);
}
 
/**
 * @brief Writes a single byte (Command or Data).
 * @param data The byte to send.
 * @param isCommand True if sending a command; false for data.
*/
static inline void ILI9488_Write(uint8_t data, bool isCommand) {
   gpio_put(LCD_CS, 0); 
   gpio_put(LCD_DC, isCommand ? 0 : 1);
   spi_write_blocking(ILI9488_SPI_INSTANCE, &data, 1);
   gpio_put(LCD_CS, 1);
}
 
/**
 * @brief Sets the drawing window for rendering.
 * @param x0 Start X-coordinate.
 * @param y0 Start Y-coordinate.
 * @param x1 End X-coordinate.
 * @param y1 End Y-coordinate.
 */
void ILI9488_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    ILI9488_SendCommand(ILI9488_CMD_COLUMN_ADDR);
    data[0] = (x0 >> 8) & 0xFF; data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF; data[3] = x1 & 0xFF;
    ILI9488_SendData(data, 4);

    ILI9488_SendCommand(ILI9488_CMD_PAGE_ADDR);
    data[0] = (y0 >> 8) & 0xFF; data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF; data[3] = y1 & 0xFF;
    ILI9488_SendData(data, 4);
}

 
/**
 * @brief Draws a single pixel at a specified position.
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param color 16-bit RGB565 color.
 */
void ILI9488_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    uint8_t data[2] = { color >> 8, color & 0xFF };
    ILI9488_SetAddressWindow(x, y, x, y);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);
    ILI9488_SendData(data, 2);
}
 
/**
 * @brief Fills the entire screen with a single color.
 * @param color 16-bit RGB565 color value.
 */
void ILI9488_FillScreen(uint16_t color) {
    uint8_t b = (color >> 11) & 0x3F;  // Extract 6-bit Red
    uint8_t g = (color >> 5) & 0x3F;   // Extract 6-bit Green
    uint8_t r = (color & 0x1F) << 1;   // Extract 5-bit Blue (Shift Left)

    uint8_t buffer[1440];  // ✅ 480 pixels * 3 bytes (RGB666)

    for (int i = 0; i < 480; i++) {
        buffer[i * 3] = r << 2;   // ✅ Convert R5 to R6
        buffer[i * 3 + 1] = g << 2;  // ✅ Convert G6
        buffer[i * 3 + 2] = b << 2;  // ✅ Convert B5 to B6
    }

    ILI9488_SetAddressWindow(0, 0, 479, 319);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);

    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);

    for (int i = 0; i < 320; i++) {
        spi_write_blocking(ILI9488_SPI_INSTANCE, buffer, sizeof(buffer));
    }

    gpio_put(LCD_CS, 1);
}

/**
 * @brief Fills the entire screen with a single color using DMA.
 * @param color 16-bit RGB565 color value.
 */
#include "hardware/dma.h"

void ILI9488_FillScreenDMA(uint16_t color) {
    uint8_t b = (color >> 11) & 0x3F;  // Extract 6-bit Blue
    uint8_t g = (color >> 5) & 0x3F;   // Extract 6-bit Green
    uint8_t r = (color & 0x1F) << 1;   // Extract 5-bit Red

    static uint8_t buffer[14400];  // ✅ Buffer for 10 rows (4800 pixels * 3 bytes)

    for (int i = 0; i < 4800; i++) {
        buffer[i * 3] = r << 2;
        buffer[i * 3 + 1] = g << 2;
        buffer[i * 3 + 2] = b << 2;
    }

    ILI9488_SetAddressWindow(0, 0, 479, 319);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);

    gpio_put(LCD_DC, 1);  // ✅ Data mode
    gpio_put(LCD_CS, 0);  // ✅ Keep CS low for full transfer

    // ✅ Setup DMA
    int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(ILI9488_SPI_INSTANCE, true));

    dma_channel_configure(
        dma_channel, &c,
        &spi_get_hw(ILI9488_SPI_INSTANCE)->dr,  // SPI TX FIFO
        buffer,
        sizeof(buffer),  // Send 10 rows per DMA transaction
        false  // Do not start immediately
    );

    for (int i = 0; i < 32; i++) {  // ✅ Now only 32 transactions instead of 320
        dma_channel_set_read_addr(dma_channel, buffer, true);
        dma_channel_wait_for_finish_blocking(dma_channel);
    }

    dma_channel_unclaim(dma_channel);

    gpio_put(LCD_CS, 1);  // ✅ End SPI transfer
}

 
/**
* @brief Draws a filled rectangle (for UI elements).
* @param x X-coordinate.
* @param y Y-coordinate.
* @param width Rectangle width.
* @param height Rectangle height.
* @param color Fill color (RGB565).
*/
void ILI9488_DrawBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color) {
    uint16_t bytesPerRow = width * 2;
    uint8_t rowBuffer[bytesPerRow];
    uint8_t high = color >> 8;
    uint8_t low = color & 0xFF;

    for (uint16_t i = 0; i < width; i++) {
        rowBuffer[i * 2] = high;
        rowBuffer[i * 2 + 1] = low;
    }

    ILI9488_SetAddressWindow(x, y, x + width - 1, y + height - 1);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);

    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);

    for (uint16_t j = 0; j < height; j++) {
        spi_write_blocking(ILI9488_SPI_INSTANCE, rowBuffer, bytesPerRow);
    }

    gpio_put(LCD_CS, 1);
}

/**
 * @brief Draws a text string at a given position.
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param text Pointer to the string to be displayed.
 * @param color Text color in RGB565 format.
 */
void ILI9488_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color) {
    while (*text) {
        ILI9488_DrawChar(x, y, *text, color);
        x += 8;  // Advance by character width (8 pixels)
        text++;
    }
}

/**
 * @brief Draws a single character using an 8x12 font.
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param c Character to draw.
 * @param color 16-bit RGB565 color.
 */
static void ILI9488_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color) {
    if (c < 32 || c > 126) return;
    extern const unsigned char font_8x12[][12];  // Ensure this font table is included
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
 * @brief Sets the LCD backlight brightness using PWM.
 * @param brightness Brightness level (0-100%).
 */
void ILI9488_SetBacklight(uint8_t brightness) {
    if (brightness > 100) brightness = 100; // Limit max value

    // Calculate PWM duty cycle (65535 = 100%)
    uint16_t level = (brightness * 65535) / 100;

    // Configure PWM on LCD_BL if not already done
    static bool pwm_initialized = false;
    if (!pwm_initialized) {
        gpio_set_function(LCD_BL, GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(LCD_BL);
        pwm_set_wrap(slice_num, 65535);  // 16-bit resolution
        pwm_set_enabled(slice_num, true);
        pwm_initialized = true;
    }

    // Set backlight brightness
    pwm_set_gpio_level(LCD_BL, level);
}