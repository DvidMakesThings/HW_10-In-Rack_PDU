/**
 * @file MCP23017_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers03 3. MCP23017 Driver
 * @ingroup drivers
 * @brief Header file for MCP23017 I2C GPIO expander driver.
 * @{
 *
 * @version 2.0.0
 * @date 2025-12-16
 *
 * @details
 * Simplified MCP23017 driver with direct I2C operations.
 * All functions are thread-safe via per-device mutex.
 *
 * Design Principles (v2.0):
 * - Simple, direct I2C operations with minimal abstraction
 * - Per-device mutex for thread safety
 * - Shadow OLAT registers to prevent torn read-modify-write
 * - Timeout-based I2C operations (no infinite blocking)
 * - Clear error reporting
 *
 * Usage:
 * 1. Call MCP2017_Init() once at startup
 * 2. Use mcp_relay(), mcp_display(), mcp_selection() to get device handles
 * 3. Use mcp_write_pin(), mcp_read_pin() for pin operations
 * 4. Use mcp_write_mask() for atomic multi-bit operations
 *
 * IMPORTANT: For relay control, use SwitchTask APIs instead of direct MCP calls.
 * This ensures proper serialization and display LED mirroring.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef MCP23017_DRIVER_H
#define MCP23017_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "hardware/i2c.h"
#include "semphr.h"

/* ==================== MCP23017 Register Map ==================== */

/**
 * @brief MCP23017 register addresses (BANK=0, sequential).
 */
typedef enum {
    MCP23017_IODIRA = 0x00,   /**< I/O direction register A */
    MCP23017_IODIRB = 0x01,   /**< I/O direction register B */
    MCP23017_IPOLA = 0x02,    /**< Input polarity register A */
    MCP23017_IPOLB = 0x03,    /**< Input polarity register B */
    MCP23017_GPINTENA = 0x04, /**< Interrupt-on-change control A */
    MCP23017_GPINTENB = 0x05, /**< Interrupt-on-change control B */
    MCP23017_DEFVALA = 0x06,  /**< Default compare value A */
    MCP23017_DEFVALB = 0x07,  /**< Default compare value B */
    MCP23017_INTCONA = 0x08,  /**< Interrupt control A */
    MCP23017_INTCONB = 0x09,  /**< Interrupt control B */
    MCP23017_IOCON = 0x0A,    /**< Configuration register */
    MCP23017_IOCONB = 0x0B,   /**< Configuration register (mirror) */
    MCP23017_GPPUA = 0x0C,    /**< Pull-up resistor config A */
    MCP23017_GPPUB = 0x0D,    /**< Pull-up resistor config B */
    MCP23017_INTFA = 0x0E,    /**< Interrupt flag A */
    MCP23017_INTFB = 0x0F,    /**< Interrupt flag B */
    MCP23017_INTCAPA = 0x10,  /**< Interrupt capture A */
    MCP23017_INTCAPB = 0x11,  /**< Interrupt capture B */
    MCP23017_GPIOA = 0x12,    /**< GPIO port A */
    MCP23017_GPIOB = 0x13,    /**< GPIO port B */
    MCP23017_OLATA = 0x14,    /**< Output latch A */
    MCP23017_OLATB = 0x15     /**< Output latch B */
} mcp23017_reg_t;

/* ==================== Configuration ==================== */

/**
 * @brief Maximum I2C retries before failure.
 */
#ifndef MCP_I2C_MAX_RETRIES
#define MCP_I2C_MAX_RETRIES 3
#endif

/**
 * @brief I2C operation timeout in microseconds.
 */
#ifndef MCP_I2C_TIMEOUT_US
#define MCP_I2C_TIMEOUT_US 5000
#endif

/**
 * @brief Delay between I2C retries in microseconds.
 */
#ifndef MCP_I2C_RETRY_DELAY_US
#define MCP_I2C_RETRY_DELAY_US 200
#endif

/**
 * @brief Reset pulse duration in milliseconds.
 */
#ifndef MCP_RESET_PULSE_MS
#define MCP_RESET_PULSE_MS 5
#endif

/**
 * @brief Post-reset stabilization delay in milliseconds.
 */
#ifndef MCP_POST_RESET_MS
#define MCP_POST_RESET_MS 10
#endif

/**
 * @brief Maximum number of MCP23017 devices.
 */
#ifndef MCP_MAX_DEVICES
#define MCP_MAX_DEVICES 8
#endif

/* ==================== Device Context ==================== */

/**
 * @brief MCP23017 device context structure.
 *
 * @details
 * Holds all state for a single MCP23017 device including I2C bus,
 * address, shadow registers, and synchronization mutex.
 */
typedef struct {
    i2c_inst_t *i2c;         /**< I2C bus instance (i2c0/i2c1) */
    uint8_t addr;            /**< 7-bit I2C address (0x20-0x27) */
    int8_t rst_gpio;         /**< Reset GPIO pin (-1 if not used) */
    volatile uint8_t olat_a; /**< Shadow of OLATA register */
    volatile uint8_t olat_b; /**< Shadow of OLATB register */
    SemaphoreHandle_t mutex; /**< Per-device mutex for thread safety */
    bool inited;             /**< True after successful initialization */
} mcp23017_t;

/* ==================== Device Registration ==================== */

/**
 * @brief Register or retrieve an MCP23017 device context.
 *
 * @param i2c I2C bus instance (i2c0 or i2c1)
 * @param addr 7-bit I2C address (0x20-0x27)
 * @param rst_gpio Reset GPIO pin number (-1 if not connected)
 * @return Pointer to device context, or NULL on error.
 *
 * @details
 * If a device with the same I2C bus and address is already registered,
 * returns the existing context. Otherwise creates a new one.
 */
mcp23017_t *mcp_register(i2c_inst_t *i2c, uint8_t addr, int8_t rst_gpio);

/**
 * @brief Initialize an MCP23017 device to a known state.
 *
 * @param dev Device context from mcp_register()
 *
 * @details
 * Configures the device with:
 * - BANK=0, SEQOP=1 (sequential operation)
 * - All pins as outputs
 * - No pull-ups
 * - Syncs shadow registers from hardware
 */
void mcp_init(mcp23017_t *dev);

/* ==================== Register Operations ==================== */

/**
 * @brief Write a value to an MCP23017 register.
 *
 * @param dev Device context
 * @param reg Register address
 * @param value Value to write
 * @return true on success, false on I2C failure.
 *
 * @note Thread-safe (acquires device mutex).
 * @note Updates shadow registers if writing to OLATx.
 */
bool mcp_write_reg(mcp23017_t *dev, uint8_t reg, uint8_t value);

/**
 * @brief Read a value from an MCP23017 register.
 *
 * @param dev Device context
 * @param reg Register address
 * @param out Pointer to receive read value
 * @return true on success, false on I2C failure.
 *
 * @note Thread-safe (acquires device mutex).
 */
bool mcp_read_reg(mcp23017_t *dev, uint8_t reg, uint8_t *out);

/* ==================== Pin Operations ==================== */

/**
 * @brief Set pin direction (input/output).
 *
 * @param dev Device context
 * @param pin Pin number (0-7 = Port A, 8-15 = Port B)
 * @param direction 0 = output, 1 = input
 *
 * @note Thread-safe (acquires device mutex).
 */
void mcp_set_direction(mcp23017_t *dev, uint8_t pin, uint8_t direction);

/**
 * @brief Write a logic level to a pin.
 *
 * @param dev Device context
 * @param pin Pin number (0-7 = Port A, 8-15 = Port B)
 * @param value 0 = low, 1 = high
 * @return true on success, false on I2C failure.
 *
 * @note Thread-safe (acquires device mutex).
 * @note Uses shadow register to prevent torn read-modify-write.
 */
bool mcp_write_pin(mcp23017_t *dev, uint8_t pin, uint8_t value);

/**
 * @brief Read a logic level from a pin.
 *
 * @param dev Device context
 * @param pin Pin number (0-7 = Port A, 8-15 = Port B)
 * @return 0 = low, 1 = high
 *
 * @note Thread-safe (acquires device mutex).
 */
uint8_t mcp_read_pin(mcp23017_t *dev, uint8_t pin);

/**
 * @brief Atomic masked write to a port.
 *
 * @param dev Device context
 * @param port_ab 0 = Port A, 1 = Port B
 * @param mask Bits to modify (1 = modify, 0 = preserve)
 * @param value_bits New values for masked bits
 * @return true on success, false on I2C failure.
 *
 * @note Thread-safe (acquires device mutex).
 * @note Uses shadow register for atomicity.
 */
bool mcp_write_mask(mcp23017_t *dev, uint8_t port_ab, uint8_t mask, uint8_t value_bits);

/**
 * @brief Re-sync shadow registers from hardware.
 *
 * @param dev Device context
 *
 * @note Thread-safe (acquires device mutex).
 * @note Call if hardware state may have changed externally.
 */
void mcp_resync_from_hw(mcp23017_t *dev);

/* ==================== Board-Specific Functions ==================== */

/**
 * @brief Initialize all MCP23017 devices for the ENERGIS PDU.
 *
 * @details
 * Registers and initializes:
 * - Relay MCP (I2C1 @ MCP_RELAY_ADDR)
 * - Display MCP (I2C0 @ MCP_DISPLAY_ADDR)
 * - Selection MCP (I2C0 @ MCP_SELECTION_ADDR)
 *
 * @note Call once during system startup, before SwitchTask_Init().
 */
void MCP2017_Init(void);

/**
 * @brief Get relay MCP23017 device context.
 * @return Pointer to relay MCP context, or NULL if not initialized.
 */
mcp23017_t *mcp_relay(void);

/**
 * @brief Get display MCP23017 device context.
 * @return Pointer to display MCP context, or NULL if not initialized.
 */
mcp23017_t *mcp_display(void);

/**
 * @brief Get selection MCP23017 device context.
 * @return Pointer to selection MCP context, or NULL if not initialized.
 */
mcp23017_t *mcp_selection(void);

/* Attempt soft recovery by reprogramming key registers without hardware reset. */
bool mcp_recover(mcp23017_t *dev);

#endif /* MCP23017_DRIVER_H */

/** @} */
/** @} */