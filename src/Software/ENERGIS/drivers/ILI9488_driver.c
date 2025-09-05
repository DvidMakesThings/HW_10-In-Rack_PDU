/**
 * @file ILI9488_driver.c
 * @author DvidMakesThings - David Sipos
 * @defgroup driver4 4. ILI9488 TFT LCD Driver
 * @ingroup drivers
 * @brief Optimized ILI9488 TFT LCD Driver for 16-bit color (RGB565)
 * @{
 * @version 1.4
 * @date 2025-03-03
 *
 * This file implements the driver for the ILI9488 TFT LCD display, supporting
 * hardware initialization, pixel and rectangle drawing, text rendering, DMA-based
 * fast graphics operations, backlight control, and BMP image display. The driver
 * is optimized for 16-bit color (RGB565) and uses DMA for high-speed data transfer
 * where possible. All functions are designed for use on the Raspberry Pi Pico platform.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "ILI9488_driver.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "utils/font_8x12.h"
#include <stdio.h>  // For printf()
#include <stdlib.h> // For abs()
#include <string.h>

#if HAS_SCREEN
/**
 * @brief Writes a single byte to the ILI9488 display.
 *
 * Writes either a command or data byte to the display, depending on the isCommand flag.
 *
 * @param data The byte to send.
 * @param isCommand True if sending a command, false for data.
 */
static inline void ILI9488_Write(uint8_t data, bool isCommand);

/**
 * @brief Initializes the ILI9488 display.
 *
 * Performs hardware and software initialization of the ILI9488 TFT LCD display,
 * including reset sequence and configuration of pixel format and memory access control.
 */
void ILI9488_Init(void) {
    gpio_put(LCD_BL, 1); // Release LCD reset
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
    uint8_t memAccessCtrl = 0xE0; // Fixes mirrored text issue
    ILI9488_SendData(&memAccessCtrl, 1);
}

/**
 * @brief Sends a command byte to the ILI9488 display.
 *
 * Sets the display to command mode and transmits a single command byte over SPI.
 *
 * @param cmd Command byte to send.
 */
void ILI9488_SendCommand(uint8_t cmd) {
    gpio_put(LCD_DC, 0);
    spi_write_blocking(ILI9488_SPI_INSTANCE, &cmd, 1);
}

/**
 * @brief Sends a block of data to the ILI9488 display.
 *
 * Sets the display to data mode and transmits a buffer of bytes over SPI.
 *
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to send.
 */
void ILI9488_SendData(const uint8_t *data, uint32_t len) {
    gpio_put(LCD_DC, 1);
    spi_write_blocking(ILI9488_SPI_INSTANCE, data, len);
}

/**
 * @brief Writes a single byte (command or data) to the ILI9488 display.
 *
 * Internal helper for SPI byte transmission, toggling command/data mode as needed.
 *
 * @param data The byte to send.
 * @param isCommand True if sending a command, false for data.
 */
static inline void ILI9488_Write(uint8_t data, bool isCommand) {
    gpio_put(LCD_CS, 0);
    gpio_put(LCD_DC, isCommand ? 0 : 1);
    spi_write_blocking(ILI9488_SPI_INSTANCE, &data, 1);
    gpio_put(LCD_CS, 1);
}

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
void ILI9488_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    ILI9488_SendCommand(ILI9488_CMD_COLUMN_ADDR);
    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;
    ILI9488_SendData(data, 4);

    ILI9488_SendCommand(ILI9488_CMD_PAGE_ADDR);
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    ILI9488_SendData(data, 4);
}

/**
 * @brief Draws a single pixel at the specified position.
 *
 * Draws a pixel at (x, y) with the specified RGB565 color value.
 *
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param color 16-bit RGB565 color value.
 */
void ILI9488_DrawPixel(uint16_t x, uint16_t y, uint32_t color) {
    uint8_t data[3] = {
        (color >> 16) & 0xFC, // Extract stored Red
        (color >> 8) & 0xFC,  // Extract stored Green
        (color) & 0xFC        // Extract stored Blue
    };

    ILI9488_SetAddressWindow(x, y, x, y);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);
    ILI9488_SendData(data, 3);
}

/**
 * @brief Fills the entire screen with a single color using DMA.
 *
 * Uses DMA to rapidly fill the entire display with the specified color.
 *
 * @param color 16-bit RGB565 color value.
 */
void ILI9488_FillScreenDMA(uint32_t color) {
    uint8_t b = (color >> 16) & 0xFC;
    uint8_t g = (color >> 8) & 0xFC;
    uint8_t r = (color) & 0xFC;

    static uint8_t __attribute__((aligned(4))) buffer[14400]; // 4800 pixels * 3 bytes

    for (int i = 0; i < 4800; i++) {
        buffer[i * 3] = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }

    ILI9488_SetAddressWindow(0, 0, 479, 319);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);

    gpio_put(LCD_DC, 1); // Enable Data Mode
    gpio_put(LCD_CS, 0); // Begin SPI Transaction

    int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(ILI9488_SPI_INSTANCE, true));

    dma_channel_configure(dma_channel, &c, &spi_get_hw(ILI9488_SPI_INSTANCE)->dr, buffer,
                          sizeof(buffer), false);

    for (int i = 0; i < 32; i++) { // loops 32 times (fills all 320 rows)
        dma_channel_set_read_addr(dma_channel, buffer, true);
        dma_channel_wait_for_finish_blocking(dma_channel);
    }

    dma_channel_unclaim(dma_channel);
    gpio_put(LCD_CS, 1); // End SPI Transaction
}

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
void ILI9488_DrawBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t color) {
    uint8_t b = (color >> 16) & 0xFC; // Extract Red
    uint8_t g = (color >> 8) & 0xFC;  // Extract Green
    uint8_t r = (color) & 0xFC;       // Extract Blue

    static uint8_t rowBuffer[1440]; // 480 pixels * 3 bytes (RGB666)

    for (int i = 0; i < width; i++) {
        rowBuffer[i * 3] = r;
        rowBuffer[i * 3 + 1] = g;
        rowBuffer[i * 3 + 2] = b;
    }

    ILI9488_SetAddressWindow(x, y, x + width - 1, y + height - 1);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);

    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);

    // ✅ Use DMA for sending large data
    int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(ILI9488_SPI_INSTANCE, true));

    dma_channel_configure(dma_channel, &c,
                          &spi_get_hw(ILI9488_SPI_INSTANCE)->dr, // SPI TX FIFO
                          rowBuffer,
                          width * 3, // Transfer 1 row at a time
                          false);

    for (uint16_t j = 0; j < height; j++) {
        dma_channel_set_read_addr(dma_channel, rowBuffer, true);
        dma_channel_wait_for_finish_blocking(dma_channel);
    }

    dma_channel_unclaim(dma_channel);
    gpio_put(LCD_CS, 1);
}

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
void ILI9488_DrawText(uint16_t x, uint16_t y, const char *text, uint32_t color) {
    while (*text) {
        ILI9488_DrawChar(x, y, *text, color);
        x += 8; // Adjust character spacing
        text++;
    }
}

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
void ILI9488_DrawLineDMA(uint16_t x0, uint16_t y0, uint16_t x1, uint32_t color) {
    if (x0 > x1) {
        uint16_t temp = x0;
        x0 = x1;
        x1 = temp;
    } // Ensure x0 < x1
    int length = x1 - x0 + 1;

    // Convert 24-bit color to RGB666
    uint8_t b = (color >> 16) & 0xFC;
    uint8_t g = (color >> 8) & 0xFC;
    uint8_t r = (color) & 0xFC;

    // Fill the buffer with the color
    static uint8_t __attribute__((aligned(4))) buffer[1440]; // Max width row buffer
    for (int i = 0; i < length; i++) {
        buffer[i * 3] = r;
        buffer[i * 3 + 1] = g;
        buffer[i * 3 + 2] = b;
    }

    ILI9488_SetAddressWindow(x0, y0, x1, y0);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);

    gpio_put(LCD_DC, 1);
    gpio_put(LCD_CS, 0);

    // DMA setup
    int dma_channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(ILI9488_SPI_INSTANCE, true));

    dma_channel_configure(dma_channel, &c,
                          &spi_get_hw(ILI9488_SPI_INSTANCE)->dr, // SPI TX FIFO
                          buffer,
                          length * 3, // 3 bytes per pixel
                          true        // Start immediately
    );

    dma_channel_wait_for_finish_blocking(dma_channel);
    dma_channel_unclaim(dma_channel);

    gpio_put(LCD_CS, 1);
}

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
static void ILI9488_DrawChar(uint16_t x, uint16_t y, char c, uint32_t color) {
    if (c < 32 || c > 126)
        return; // ✅ Ignore unsupported characters
    extern const unsigned char font_8x12[][12];
    const unsigned char *charData = font_8x12[c - 32];

    uint8_t b = (color >> 16) & 0xFC;
    uint8_t g = (color >> 8) & 0xFC;
    uint8_t r = (color) & 0xFC;

    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 8; col++) {
            if (charData[row] & (1 << (7 - col))) {
                ILI9488_DrawPixel(x + col, y + row, (r << 16) | (g << 8) | b);
            }
        }
    }
}

/**
 * @brief Sets the LCD backlight brightness using PWM.
 *
 * Configures and sets the PWM duty cycle for the LCD backlight pin.
 *
 * @param brightness Brightness level (0-100%).
 */
void ILI9488_SetBacklight(uint8_t brightness) {
    if (brightness > 100)
        brightness = 100; // Limit max value

    // Calculate PWM duty cycle (65535 = 100%)
    uint16_t level = (brightness * 65535) / 100;

    // Configure PWM on LCD_BL if not already done
    static bool pwm_initialized = false;
    if (!pwm_initialized) {
        gpio_set_function(LCD_BL, GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(LCD_BL);
        pwm_set_wrap(slice_num, 65535); // 16-bit resolution
        pwm_set_enabled(slice_num, true);
        pwm_initialized = true;
    }

    // Set backlight brightness
    pwm_set_gpio_level(LCD_BL, level);
}

/**
 * @brief Draws a BMP image from a file at the specified position.
 *
 * Reads a 24-bit uncompressed BMP file and renders it at (x, y) on the display.
 *
 * @param x Start X-coordinate.
 * @param y Start Y-coordinate.
 * @param filename BMP file path (must be 24-bit uncompressed).
 */
void ILI9488_DrawBitmap(uint16_t x, uint16_t y, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Failed to open BMP file.\n");
        return;
    }

    // Read BMP header
    uint8_t header[54];
    fread(header, 1, 54, file);

    // Extract width & height from BMP header
    uint32_t width = *(uint32_t *)&header[18];
    uint32_t height = *(uint32_t *)&header[22];

    printf("BMP Info: Width=%d, Height=%d\n", width, height);

    // Set drawing area
    ILI9488_SetAddressWindow(x, y, x + width - 1, y + height - 1);
    ILI9488_SendCommand(ILI9488_CMD_MEMORY_WRITE);

    gpio_put(LCD_DC, 1); // Data mode
    gpio_put(LCD_CS, 0);

    // BMP uses bottom-to-top scan order, so we must read it backwards
    uint8_t pixelData[3]; // BMP stores in BGR order
    for (int row = height - 1; row >= 0; row--) {
        for (uint32_t col = 0; col < width; col++) {
            fread(pixelData, 1, 3, file); // Read BGR888

            // Convert BGR888 → RGB666 for ILI9488
            uint8_t r = pixelData[2] & 0xFC;
            uint8_t g = pixelData[1] & 0xFC;
            uint8_t b = pixelData[0] & 0xFC;

            uint8_t rgb666[3] = {r, g, b};
            spi_write_blocking(ILI9488_SPI_INSTANCE, rgb666, 3);
        }
    }

    gpio_put(LCD_CS, 1);
    fclose(file);
}

#endif