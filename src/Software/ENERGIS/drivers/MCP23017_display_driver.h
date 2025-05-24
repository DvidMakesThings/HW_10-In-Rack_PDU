/**
 * @file MCP23017_driver.h
 * @author DvidMakesThings - David Sipos (DvidMakesThings)
 * @brief Driver for MCP23017 I/O Expander
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef MCP23017_DISPLAY_DRIVER_H
#define MCP23017_DISPLAY_DRIVER_H

#include "../CONFIG.h" // Ensure this includes the MCP23017 register definitions and bus/address info
#include "hardware/i2c.h"
#include <stdint.h>

/**
 * @brief Initializes the display board's MCP23017.
 *
 * Performs a hardware reset using the MCP_LCD_RST pin and configures all ports as outputs.
 */
void mcp_display_init(void);

/**
 * @brief Writes a value to a specified MCP23017 register.
 *
 * @param reg The register address.
 * @param value The value to write.
 */
void mcp_display_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Reads a value from a specified MCP23017 register.
 *
 * @param reg The register address.
 * @return The value read.
 */
uint8_t mcp_display_read_reg(uint8_t reg);

/**
 * @brief Sets the direction of a specific MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @param direction 0 = output, 1 = input.
 */
void mcp_display_set_direction(uint8_t pin, uint8_t direction);

/**
 * @brief Writes a digital value to a specific MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @param value 0 = low, 1 = high.
 */
void mcp_display_write_pin(uint8_t pin, uint8_t value);

/**
 * @brief Reads the digital value from a specific MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @return 0 = low, 1 = high.
 */
uint8_t mcp_display_read_pin(uint8_t pin);

#endif // MCP23017_DISPLAY_DRIVER_H