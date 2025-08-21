/**
 * @file MCP23017_display_driver.c
 * @author David Sipos
 * @brief Implementation for MCP23017 I/O Expander driver on ENERGIS display board.
 * @version 1.1
 * @date 2025-03-03
 * @copyright Copyright (c) 2025
 *
 * @details
 * This file implements functions for initializing and controlling the MCP23017
 * I/O expander on the ENERGIS display board, including register access and pin manipulation.
 */

#include "MCP23017_display_driver.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

/**
 * @brief Reads a register from the MCP23017 device.
 *
 * Writes the register address, then reads a single byte from the device.
 *
 * @param reg Register address to read.
 * @return Value read from the register.
 */
static uint8_t mcp_display_read_register(uint8_t reg) {
    uint8_t value;
    // Write register address, then read one byte.
    i2c_write_blocking(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, &reg, 1, true);
    i2c_read_blocking(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, &value, 1, false);
    return value;
}

/**
 * @brief Writes a value to a register on the MCP23017 device.
 *
 * Sends the register address and value as a two-byte sequence.
 *
 * @param reg Register address to write.
 * @param value Value to write to the register.
 */
static void mcp_display_write_register(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, data, 2, false);
}

/**
 * @brief Writes a value to a specific MCP23017 register.
 *
 * @param reg Register address to write.
 * @param value Value to write.
 */
void mcp_display_write_reg(uint8_t reg, uint8_t value) { mcp_display_write_register(reg, value); }

/**
 * @brief Reads a value from a specific MCP23017 register.
 *
 * @param reg Register address to read.
 * @return Value read from the register.
 */
uint8_t mcp_display_read_reg(uint8_t reg) { return mcp_display_read_register(reg); }

/**
 * @brief Initializes the MCP23017 I/O expander for the display board.
 *
 * Performs a hardware reset using MCP_LCD_RST and sets all pins as outputs.
 */
void mcp_display_init(void) {
    // Perform a hardware reset on the MCP23017 using MCP_LCD_RST.
    // Assume that startup_init() has already been called so that MCP_LCD_RST is configured.
    gpio_put(MCP_LCD_RST, 0); // Assert reset.
    sleep_ms(100);            // Hold reset for 10ms.
    gpio_put(MCP_LCD_RST, 1); // Release reset.
    sleep_ms(100);            // Wait for the device to initialize.

    // Configure all pins as outputs by writing 0x00 to both IODIRA and IODIRB.
    mcp_display_write_register(MCP23017_IODIRA, 0x00);
    mcp_display_write_register(MCP23017_IODIRB, 0x00);
}

/**
 * @brief Sets the direction of a specific MCP23017 pin.
 *
 * Configures the pin as input or output by updating the IODIR register.
 *
 * @param pin Pin number (0-15).
 * @param direction Pin direction: 0 for output, 1 for input.
 */
void mcp_display_set_direction(uint8_t pin, uint8_t direction) {
    // Determine the register (IODIRA for pins 0-7, IODIRB for pins 8-15).
    uint8_t reg = (pin < 8) ? MCP23017_IODIRA : MCP23017_IODIRB;
    uint8_t bit = pin % 8;
    uint8_t current = mcp_display_read_register(reg);

    if (direction) {
        current |= (1 << bit); // Set as input.
    } else {
        current &= ~(1 << bit); // Set as output.
    }
    mcp_display_write_register(reg, current);
}

/**
 * @brief Writes a digital value to a specific MCP23017 pin.
 *
 * Updates the output latch register to set the pin high or low.
 *
 * @param pin Pin number (0-15).
 * @param value Digital value: 0 for low, 1 for high.
 */
void mcp_display_write_pin(uint8_t pin, uint8_t value) {
    // Write to output latch registers (OLATA for pins 0-7, OLATB for pins 8-15).
    uint8_t reg = (pin < 8) ? MCP23017_OLATA : MCP23017_OLATB;
    uint8_t bit = pin % 8;
    uint8_t current = mcp_display_read_register(reg);

    if (value) {
        current |= (1 << bit); // Set pin high.
    } else {
        current &= ~(1 << bit); // Set pin low.
    }
    mcp_display_write_register(reg, current);
}

/**
 * @brief Reads the digital value from a specific MCP23017 pin.
 *
 * Reads the GPIO register and returns the value of the specified pin.
 *
 * @param pin Pin number (0-15).
 * @return Digital value: 0 if low, 1 if high.
 */
uint8_t mcp_display_read_pin(uint8_t pin) {
    // Read from the GPIO registers (GPIOA for pins 0-7, GPIOB for pins 8-15).
    uint8_t reg = (pin < 8) ? MCP23017_GPIOA : MCP23017_GPIOB;
    uint8_t bit = pin % 8;
    uint8_t value = mcp_display_read_register(reg);
    return (value >> bit) & 0x01;
}