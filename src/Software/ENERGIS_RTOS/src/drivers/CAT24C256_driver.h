/**
 * @file src/drivers/CAT24C256_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers02 2. CAT24C256 EEPROM Driver Implementation
 * @ingroup drivers
 * @brief Header file for CAT24C256 I2C EEPROM driver
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-06
 *
 * @details
 * RTOS-compatible implementation for CAT24C256 EEPROM.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CAT24C256_DRIVER_H
#define CAT24C256_DRIVER_H

#include "../CONFIG.h"

/* ==================== CAT24C256 Constants ==================== */

/**
 * @brief Default I2C device address for CAT24C256 (7-bit address)
 * @note Address can be modified by hardware address pins A0-A2 if available on the device
 */
#define CAT24C256_I2C_ADDR 0x50

/**
 * @brief Page size for write operations (128 bytes)
 * @note Write operations must respect page boundaries for proper operation
 */
#define CAT24C256_PAGE_SIZE 128

/**
 * @brief Total memory size of the CAT24C256 EEPROM (32KB)
 */
#define CAT24C256_TOTAL_SIZE EEPROM_SIZE

/**
 * @brief Write cycle time in milliseconds (typical: 5ms max)
 * @note This delay is required after each write operation to ensure data integrity
 */
#define CAT24C256_WRITE_CYCLE_MS 5

/* ==================== Function Prototypes ==================== */

/**
 * @brief Initialize the CAT24C256 EEPROM driver.
 *
 * Uses CONFIG.h definitions: I2C1_SDA, I2C1_SCL, EEPROM_I2C, I2C_SPEED.
 * I2C peripheral already initialized by system_startup_init().
 *
 * RTOS: Safe to call from any task context.
 */
/**
 * @brief Initializes the CAT24C256 EEPROM hardware interface
 *
 * Sets up the I2C interface pins (SDA and SCL) with proper function assignments and pull-up
 * resistors. The I2C peripheral must be previously initialized by system_startup_init().
 *
 * @param None
 * @return None
 * @note Thread-safe: Can be called from any task context
 */
void CAT24C256_Init(void);

/**
 * @brief Writes a single byte to a specific EEPROM address
 *
 * Performs an atomic write operation of one byte to the specified memory address.
 * Automatically handles the required write cycle delay after the operation.
 *
 * @param addr Memory address between 0x0000 and 0xFFFF
 * @param data 8-bit value to write to the specified address
 * @return 0 on successful write, -1 if I2C communication fails
 * @note Thread-safety: Must only be called from StorageTask with eepromMtx protection
 */
int CAT24C256_WriteByte(uint16_t addr, uint8_t data);

/**
 * @brief Reads a single byte from the specified EEPROM address
 *
 * Performs a two-part I2C transaction:
 * 1. Write address bytes with repeated start
 * 2. Read one byte of data
 *
 * @param addr Source memory address (0x0000 - 0xFFFF)
 * @return Read byte value, 0xFF if read operation fails
 */
uint8_t CAT24C256_ReadByte(uint16_t addr);

/**
 * @brief Writes a contiguous block of data to EEPROM with page boundary handling
 *
 * Implements automatic page-aware write operations that respect the 128-byte
 * page boundaries of the CAT24C256. Write operations that cross page boundaries
 * are automatically split into multiple page-aligned chunks.
 *
 * @param addr Starting memory address (0x0000 - 0xFFFF)
 * @param data Pointer to source data buffer
 * @param len Number of bytes to write
 * @return 0 if all writes successful, -1 on NULL pointer or any I2C error
 * @note Includes write cycle delays between each page write operation
 */
int CAT24C256_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Reads a contiguous block of data from EEPROM
 *
 * Performs a sequential read operation starting at the specified address.
 * Uses the EEPROM's auto-increment feature for efficient multi-byte reads.
 * On error, fills the destination buffer with 0xFF.
 *
 * @param addr Starting memory address (0x0000 - 0xFFFF)
 * @param buffer Pointer to destination buffer
 * @param len Number of bytes to read
 * @return None
 * @note Supports reads across page boundaries without special handling
 */
void CAT24C256_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len);

/**
 * @brief Performs a comprehensive self-test of EEPROM functionality
 *
 * Validates EEPROM operation by:
 * 1. Writing a test pattern with alternating bits and edge cases
 * 2. Reading back the pattern to verify data integrity
 * 3. Comparing written and read data byte-by-byte
 *
 * Test pattern: 0xAA, 0x55, 0xCC, 0x33, 0xF0, 0x0F, 0x00, 0xFF
 * - Tests alternating bits (0xAA, 0x55)
 * - Tests paired bits (0xCC, 0x33)
 * - Tests half-byte patterns (0xF0, 0x0F)
 * - Tests extreme values (0x00, 0xFF)
 *
 * @param test_addr Starting address for test pattern
 * @return true if test passes, false on any error
 * @note Modifies 8 bytes of EEPROM content starting at test_addr
 */
bool CAT24C256_SelfTest(uint16_t test_addr);

#endif /* CAT24C256_DRIVER_H */

/** @} */