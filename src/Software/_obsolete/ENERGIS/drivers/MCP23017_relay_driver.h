/**
 * @file MCP23017_relay_driver.h
 * @author David Sipos (DvidMakesThings)
 * @brief MCP23017 I/O Expander driver for ENERGIS relay board with race-free shadow-latched writes.
 * @version 1.1
 * @date 2025-08-21
 * @copyright Copyright (c) 2025
 *
 * Provides function prototypes for controlling the MCP23017 I/O expander on the relay board,
 * including initialization, register access, pin direction, atomic digital I/O, and shadow sync.
 */

#ifndef MCP23017_RELAY_DRIVER_H
#define MCP23017_RELAY_DRIVER_H

#include "../CONFIG.h"
#include "hardware/i2c.h"
#include <stdint.h>

/**
 * @brief Initialize the MCP23017 I/O expander for the relay board.
 *
 * Performs hardware reset, configures IOCON/IODIR, seeds shadow latches from OLAT,
 * and drives a known-safe initial state. All I²C access is mutex-guarded.
 */
void mcp_relay_init(void);

/**
 * @brief Write a value to a specific MCP23017 register (mutex-guarded).
 *
 * Keeps shadow latches in sync if OLATA/OLATB is written.
 * @param reg Register address to write to.
 * @param value Value to write.
 */
void mcp_relay_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Read a value from a specific MCP23017 register (mutex-guarded).
 *
 * @param reg Register address to read from.
 * @return Value read from the register.
 */
uint8_t mcp_relay_read_reg(uint8_t reg);

/**
 * @brief Set the direction of a specific MCP23017 pin.
 *
 * @param pin Pin number (0-15).
 * @param direction 0 for output, 1 for input.
 */
void mcp_relay_set_direction(uint8_t pin, uint8_t direction);

/**
 * @brief Write a digital value to a specific MCP23017 pin.
 *
 * Uses shadowed whole-port writes under a mutex to avoid race conditions.
 * @param pin Pin number (0-15).
 * @param value 0 for low, 1 for high.
 */
void mcp_relay_write_pin(uint8_t pin, uint8_t value);

/**
 * @brief Read the digital value from a specific MCP23017 pin.
 *
 * @param pin Pin number (0-15).
 * @return 0 if low, 1 if high.
 */
uint8_t mcp_relay_read_pin(uint8_t pin);

/**
 * @brief Atomically set or clear a group of bits on a given port using a mask.
 *
 * @param port_ab 0 for Port A, 1 for Port B.
 * @param mask Bit mask for affected bits.
 * @param value_bits Values for masked bits.
 */
void mcp_relay_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits);

/**
 * @brief Re-sync software shadow latches from hardware OLAT registers.
 */
void mcp_relay_resync_from_hw(void);

#endif // MCP23017_RELAY_DRIVER_H
