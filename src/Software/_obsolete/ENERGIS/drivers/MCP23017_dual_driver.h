/**
 * @file MCP23017_dual_driver.h
 * @author DvidMakesThings - David Sipos
 * @defgroup driver6 6. MCP23017 Dual Driver
 * @ingroup drivers
 * @brief Thin wrapper driver that controls both Relay and Display MCP23017 chips together.
 * @version 1.0
 * @date 2025-08-25
 *
 * This driver provides convenience functions to operate on both
 * the Relay MCP23017 and the Display MCP23017 simultaneously.
 * It reuses the robust, shadow-latched single-chip drivers internally.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 * @{
 */

#ifndef MCP23017_DUAL_DRIVER_H
#define MCP23017_DUAL_DRIVER_H

#include "../CONFIG.h"
#include <stdint.h>

/**
 * @brief Write a value to a register on both MCP23017 chips.
 * @param reg Register address
 * @param value Value to write
 */
void mcp_dual_write_reg(uint8_t reg, uint8_t value);

/**
 * @brief Configure pin direction on both MCP23017 chips.
 * @param pin Pin number (0-15)
 * @param direction 0 = output, 1 = input
 */
void mcp_dual_set_direction(uint8_t pin, uint8_t direction);

/**
 * @brief Write a digital value to a pin on both MCP23017 chips.
 * @param pin Pin number (0-15)
 * @param value 0 = low, 1 = high
 */
void mcp_dual_write_pin(uint8_t pin, uint8_t value);

/**
 * @brief Atomically write a masked group of bits on both MCP23017 chips.
 * @param port_ab 0 = Port A, 1 = Port B
 * @param mask Bit mask to apply
 * @param value_bits Bits to set
 */
void mcp_dual_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits);

/**
 * @brief Resynchronize both MCP23017 shadow latches from hardware OLAT registers.
 */
void mcp_dual_resync_from_hw(void);

/**
 * @brief Write both MCPs for one pin only if needed, and report asymmetry.
 *
 * Reads the current state of the selected pin on both Relay and Display chips.
 * If either chip's pin state differs from @p value, writes @p value to BOTH chips.
 * Also reports whether the two chips were asymmetric before and after the operation.
 *
 * @param pin              Pin number (0–15).
 * @param value            Desired state: 0 = low, 1 = high.
 * @param asym_before_out  Optional pointer; set to 1 if relay!=display BEFORE, else 0.
 * @param asym_after_out   Optional pointer; set to 1 if relay!=display AFTER, else 0.
 * @return uint8_t         1 if a write was performed; 0 if no change was needed.
 */
uint8_t set_relay_state(uint8_t pin, uint8_t value, uint8_t *asym_before_out,
                        uint8_t *asym_after_out);

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
 * @brief Compute full-chip asymmetry mask (1 bits where relay!=display).
 *
 * Compares all 16 outputs (A0..A7, B0..B7) between Relay and Display chips.
 *
 * Bit mapping in the returned mask:
 *   bit 0..7  -> Port A pins 0..7
 *   bit 8..15 -> Port B pins 0..7
 *
 * @return uint16_t        16-bit mask of asymmetric pins (0 = identical, 1 = mismatch).
 */
uint16_t mcp_dual_asymmetry_mask(void);

/** @} */

#endif /* MCP23017_DUAL_DRIVER_H */
