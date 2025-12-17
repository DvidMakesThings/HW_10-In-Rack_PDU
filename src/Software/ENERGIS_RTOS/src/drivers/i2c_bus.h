/**
 * @file i2c_bus.h
 * @author DvidMakesThings - David Sipos
 * 
 * @defgroup drivers Drivers
 * @brief HAL drivers for the Energis PDU firmware.
 * @{
 * 
 * @defgroup drivers01 1. I2C Bus Manager
 * @brief Centralized, serialized I2C access with synchronous wrappers.
 * @{
 *
 * @version 2.0.0
 * @date 2025-12-17
 *
 * @details
 * Exposes synchronous wrappers around Pico SDK I2C functions that are
 * serialized through a single FIFO/Task, ensuring no interleaved accesses
 * across controllers. This improves determinism under high load (e.g. SNMP
 * stress) and avoids re-entrancy concerns.
 *
 * Usage:
 * - Call I2C_BusInit() once during system startup
 * - Use i2c_bus_* helpers instead of direct Pico SDK calls
 * - Optional legacy lock/unlock are still available for compatibility
 *
 * Thread-safety:
 * - All functions are safe to call from multiple tasks; requests are queued.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#pragma once
#include "FreeRTOS.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "semphr.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void I2C_BusLock(i2c_inst_t *i2c);
void I2C_BusUnlock(i2c_inst_t *i2c);

/* ---------------- I2C Bus Manager (FIFO, single owner) --------------- */
/**
 * @brief Initialize the global I2C bus manager task and queue.
 * Must be called once after hardware i2c_init() and before clients use wrappers.
 */
void I2C_BusInit(void);

/**
 * @brief Queue a write on the specified I2C controller and wait synchronously.
 * Mirrors Pico SDK `i2c_write_timeout_us` signature/semantics, but serialized via a FIFO.
 */
int i2c_bus_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len,
                             bool nostop, uint32_t timeout_us);

/**
 * @brief Queue a read on the specified I2C controller and wait synchronously.
 * Mirrors Pico SDK `i2c_read_timeout_us` signature/semantics, but serialized via a FIFO.
 */
int i2c_bus_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                            uint32_t timeout_us);

/* ---------------- High-level register/memory helpers (centralized) --------- */
/**
 * @brief Write 8-bit register with 8-bit value: [reg, value]
 * @return true on success
 */
bool i2c_bus_write_reg8(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t value,
                        uint32_t timeout_us);

/**
 * @brief Read 8-bit register: write [reg] (no stop), then read 1 byte
 * @return true on success, and writes into out
 */
bool i2c_bus_read_reg8(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t *out,
                       uint32_t timeout_us);

/**
 * @brief Write to 16-bit memory address: [mem_hi, mem_lo, data...]
 * @return true on success
 */
bool i2c_bus_write_mem16(i2c_inst_t *i2c, uint8_t addr, uint16_t mem, const uint8_t *src,
                         size_t len, uint32_t timeout_us);

/**
 * @brief Read from 16-bit memory address: write [mem_hi, mem_lo] (no stop), then read len bytes
 * @return true on success, and fills dst
 */
bool i2c_bus_read_mem16(i2c_inst_t *i2c, uint8_t addr, uint16_t mem, uint8_t *dst, size_t len,
                        uint32_t timeout_us);

#ifdef __cplusplus
}
#endif

/** @} */
