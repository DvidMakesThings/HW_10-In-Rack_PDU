/**
 * @file MCP23017_display_driver.h
 * @author David Sipos (DvidMakesThings)
 * @brief Driver interface for MCP23017 I/O Expander on ENERGIS display board.
 * @version 1.1
 * @date 2025-03-03
 * @copyright Copyright (c) 2025
 *
 * @details
 * This header defines the API for controlling the MCP23017 I/O expander
 * used on the ENERGIS display board. Functions include initialization,
 * register access, pin direction configuration, and digital I/O operations.
 */

#ifndef MCP23017_DISPLAY_DRIVER_H
#define MCP23017_DISPLAY_DRIVER_H

#include "../CONFIG.h"
#include "hardware/i2c.h"
#include <stdint.h>

/**
 * @brief Initializes the MCP23017 I/O expander for the display board.
 *
 * Performs a hardware reset using the MCP_LCD_RST pin and configures all pins as outputs.
 */
void mcp_display_init(void);

/**
 * @brief Writes a value to a MCP23017 register.
 *
 * @param reg The register address to write to.
 * @param value The value to write to the register.
 */
void mcp_display_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Reads a value from a MCP23017 register.
 *
 * @param reg The register address to read from.
 * @return The value read from the register.
 */
uint8_t mcp_display_read_reg(uint8_t reg);

/**
 * @brief Sets the direction of a MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @param direction Pin direction: 0 for output, 1 for input.
 */
void mcp_display_set_direction(uint8_t pin, uint8_t direction);

/**
 * @brief Writes a digital value to a MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @param value Digital value: 0 for low, 1 for high.
 */
void mcp_display_write_pin(uint8_t pin, uint8_t value);

/**
 * @brief Reads the digital value from a MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @return Digital value: 0 if low, 1 if high.
 */
uint8_t mcp_display_read_pin(uint8_t pin);

#endif // MCP23017_DISPLAY_DRIVER_H