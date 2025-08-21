/**
 * @file MCP23017_relay_driver.h
 * @author DvidMakesThings - David Sipos (DvidMakesThings)
 * @brief Driver for the Relay Board's MCP23017 I/O Expander.
 * @version 1.1
 * @date 2025-08-21
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef MCP23017_RELAY_DRIVER_H
#define MCP23017_RELAY_DRIVER_H

#include "../CONFIG.h"
#include "hardware/i2c.h"
#include <stdint.h>

/**
 * @brief Initializes the relay board's MCP23017.
 *
 * Performs a hardware reset using MCP_REL_RST, configures IOCON/IODIR,
 * seeds software shadows from OLAT, and drives a known state.
 * All I²C access is mutex-guarded to prevent torn RMW updates.
 */
void mcp_relay_init(void);

/**
 * @brief Writes a value to a specified MCP23017 register (guarded).
 *
 * If OLATA/OLATB is written, the shadow is kept in sync.
 */
void mcp_relay_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Reads a value from a specified MCP23017 register (guarded).
 */
uint8_t mcp_relay_read_reg(uint8_t reg);

/**
 * @brief Sets the direction of a specific MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @param direction 0 = output, 1 = input.
 */
void mcp_relay_set_direction(uint8_t pin, uint8_t direction);

/**
 * @brief Writes a digital value to a specific MCP23017 pin.
 *        Uses shadowed whole-port writes under a mutex to avoid races.
 *
 * @param pin The pin number (0-15).
 * @param value 0 = low, 1 = high.
 */
void mcp_relay_write_pin(uint8_t pin, uint8_t value);

/**
 * @brief Reads the digital value from a specific MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @return 0 = low, 1 = high.
 */
uint8_t mcp_relay_read_pin(uint8_t pin);

/**
 * @brief Atomically set/clear a group of bits on a given port from a mask.
 *
 * @param port_ab 0 for Port A, 1 for Port B
 * @param mask    Bits to affect
 * @param value_bits Values for those bits (only masked bits are applied)
 */
void mcp_relay_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits);

/**
 * @brief Re-sync software shadows from hardware OLAT (optional utility).
 */
void mcp_relay_resync_from_hw(void);

#endif // MCP23017_RELAY_DRIVER_H
