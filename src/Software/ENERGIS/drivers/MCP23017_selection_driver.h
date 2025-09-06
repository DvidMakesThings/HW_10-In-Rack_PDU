/**
 * @file MCP23017_selection_driver.h
 * @author
 * @brief Driver interface for MCP23017 I/O Expander driving the "selection"
 *        LED row on the ENERGIS display board.
 * @version 1.1
 * @date 2025-09-06
 *
 * @details
 * This header defines the API for controlling the MCP23017 I/O expander
 * used for the "selection" indicators (second row of orange LEDs) on the
 * ENERGIS display board. Functions include initialization, register access,
 * pin direction configuration, and digital I/O operations.
 */

#ifndef MCP23017_SELECTION_DRIVER_H
#define MCP23017_SELECTION_DRIVER_H

#include "../CONFIG.h"
#include "hardware/i2c.h"
#include <stdint.h>

/**
 * @brief Initialize the MCP23017 I/O expander for the selection LED row.
 *
 * Performs hardware reset, configures IOCON/IODIR, seeds shadow latches from OLAT,
 * and drives a known-safe initial state. All I²C access is mutex-guarded.
 */
void mcp_selection_init(void);

/**
 * @brief Write a value to a specific MCP23017 register (mutex-guarded).
 *
 * Keeps shadow latches in sync if OLATA/OLATB is written.
 * @param reg Register address to write to.
 * @param value Value to write.
 */
void mcp_selection_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Read a value from a specific MCP23017 register (mutex-guarded).
 *
 * @param reg Register address to read from.
 * @return Value read from the register.
 */
uint8_t mcp_selection_read_reg(uint8_t reg);

/**
 * @brief Set the direction of a specific MCP23017 pin.
 *
 * @param pin Pin number (0-15).
 * @param direction 0 for output, 1 for input.
 */
void mcp_selection_set_direction(uint8_t pin, uint8_t direction);

/**
 * @brief Write a digital value to a specific MCP23017 pin.
 *
 * Uses shadowed whole-port writes under a mutex to avoid race conditions.
 * @param pin Pin number (0-15).
 * @param value 0 for low, 1 for high.
 */
void mcp_selection_write_pin(uint8_t pin, uint8_t value);

/**
 * @brief Read the digital value from a specific MCP23017 pin.
 *
 * @param pin Pin number (0-15).
 * @return 0 if low, 1 if high.
 */
uint8_t mcp_selection_read_pin(uint8_t pin);

/**
 * @brief Atomically set or clear a group of bits on a given port using a mask.
 *
 * @param port_ab 0 for Port A, 1 for Port B.
 * @param mask Bit mask for affected bits.
 * @param value_bits Values for masked bits.
 */
void mcp_selection_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits);

/**
 * @brief Re-sync software shadow latches from hardware OLAT registers.
 */
void mcp_selection_resync_from_hw(void);

#endif // MCP23017_SELECTION_DRIVER_H
