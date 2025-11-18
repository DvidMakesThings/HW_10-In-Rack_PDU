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

#define GUARDIAN_TAG "<Guardian>"

/**
 * @brief Initialize the MCP23017 I/O expander for the relay board.
 *
 * Performs hardware reset, configures IOCON/IODIR, seeds shadow latches from OLAT,
 * and drives a known-safe initial state. All I²C access is mutex-guarded.
 */
void mcp_display_init(void);

/**
 * @brief Write a value to a specific MCP23017 register (mutex-guarded).
 *
 * Keeps shadow latches in sync if OLATA/OLATB is written.
 * @param reg Register address to write to.
 * @param value Value to write.
 */
void mcp_display_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Read a value from a specific MCP23017 register (mutex-guarded).
 *
 * @param reg Register address to read from.
 * @return Value read from the register.
 */
uint8_t mcp_display_read_reg(uint8_t reg);

/**
 * @brief Set the direction of a specific MCP23017 pin.
 *
 * @param pin Pin number (0-15).
 * @param direction 0 for output, 1 for input.
 */
void mcp_display_set_direction(uint8_t pin, uint8_t direction);

/**
 * @brief Write a digital value to a specific MCP23017 pin.
 *
 * Uses shadowed whole-port writes under a mutex to avoid race conditions.
 * @param pin Pin number (0-15).
 * @param value 0 for low, 1 for high.
 */
void mcp_display_write_pin(uint8_t pin, uint8_t value);

/**
 * @brief Read the digital value from a specific MCP23017 pin.
 *
 * @param pin Pin number (0-15).
 * @return 0 if low, 1 if high.
 */
uint8_t mcp_display_read_pin(uint8_t pin);

/**
 * @brief Atomically set or clear a group of bits on a given port using a mask.
 *
 * @param port_ab 0 for Port A, 1 for Port B.
 * @param mask Bit mask for affected bits.
 * @param value_bits Values for masked bits.
 */
void mcp_display_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits);

/**
 * @brief Re-sync software shadow latches from hardware OLAT registers.
 */
void mcp_display_resync_from_hw(void);

/**
 * @brief Write both MCPs for one pin only if needed, and report asymmetry, with logging.
 * @param caller_tag       Text label of the caller (e.g. "uart", "fsm", __func__).
 * @param pin              Pin number (0–15).
 * @param value            Desired state: 0 = low, 1 = high.
 * @param asym_before_out  Optional pointer; set to 1 if relay!=display BEFORE, else 0.
 * @param asym_after_out   Optional pointer; set to 1 if relay!=display AFTER, else 0.
 * @return uint8_t         1 if a write was performed; 0 if no change was needed.
 */
uint8_t set_relay_state_with_tag(const char *caller_tag, uint8_t pin, uint8_t value,
                                 uint8_t *asym_before_out, uint8_t *asym_after_out);

/**
 * @brief Periodic asymmetry guardian for the two MCP23017s.
 *
 * Reads @ref mcp_dual_asymmetry_mask() on a short interval.
 * If any monitored bit differs (relay != display), it logs once per poll.
 * When @ref DUAL_GUARD_AUTOHEAL is 1, it sets BOTH chips to the relay’s current
 * value for each mismatched pin via set_relay_state_tag(@ref GUARDIAN_TAG,...).
 *
 * Only pins filtered by @ref DUAL_GUARD_MASK are considered (default: IO0..7).
 *
 * @return void
 */
void mcp_dual_guardian_poll(void);

#endif // MCP23017_RELAY_DRIVER_H
