/**
 * @file src/drivers/CAT24C256_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup driver02 2. EEPROM Driver
 * @ingroup drivers
 * @brief I2C driver for CAT24C256 32KB EEPROM with page-aware write operations.
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-06
 *
 * @details
 * Provides thread-safe access to the CAT24C256 serial EEPROM used for non-volatile
 * parameter storage in the ENERGIS PDU. The driver handles:
 * - 64-byte page boundary management for writes
 * - Mandatory write cycle delays per datasheet timing
 * - 16-bit address space (0x0000 - 0x7FFF, 32KB total)
 * - I2C bus arbitration via centralized bus manager
 *
 * Key Features:
 * - Page-aware buffered writes prevent page boundary corruption
 * - Automatic write cycle delays (5ms per page)
 * - Sequential multi-byte reads without page restrictions
 * - Self-test function for hardware validation
 *
 * Thread Safety:
 * - All operations must be called from StorageTask with eepromMtx protection
 * - Uses i2c_bus_* wrappers for serialized I2C access
 *
 * Hardware Configuration:
 * - I2C address: 0x50 (7-bit)
 * - Bus: I2C1 (configurable via CONFIG.h)
 * - Speed: Typically 100-400 kHz
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CAT24C256_DRIVER_H
#define CAT24C256_DRIVER_H

#include "../CONFIG.h"

/** @name EEPROM Constants
 *  @ingroup driver02
 *  @{ */

/**
 * @brief I2C device address for CAT24C256 (7-bit).
 * @note Hardware address pins A0-A2 can modify this if available on device
 */
#define CAT24C256_I2C_ADDR 0x50

/**
 * @brief EEPROM page size in bytes for write operations.
 * @note Writes must not cross 64-byte page boundaries per datasheet
 */
#define CAT24C256_PAGE_SIZE 64

/**
 * @brief Total EEPROM capacity in bytes (32KB).
 */
#define CAT24C256_TOTAL_SIZE EEPROM_SIZE

/**
 * @brief Mandatory write cycle delay in milliseconds.
 * @note Per datasheet: 5ms typical, 10ms maximum
 */
#define CAT24C256_WRITE_CYCLE_MS 5
/** @} */

/**
 * @brief Initialize CAT24C256 EEPROM driver.
 *
 * @details
 * Configures I2C GPIO pins with proper function assignment and pull-ups.
 * The I2C peripheral must already be initialized by system_startup_init()
 * before calling this function.
 *
 * Pin Configuration:
 * - SDA: GPIO function + internal pull-up
 * - SCL: GPIO function + internal pull-up
 *
 * @param None
 * @return None
 *
 * @note Call once during system initialization after I2C peripheral init
 * @note Thread-safe, can be called from any task context
 */
void CAT24C256_Init(void);

/** @name Public API
 *  @ingroup driver02
 *  @{ */

/**
 * @brief Write single byte to EEPROM.
 *
 * @details
 * Performs atomic single-byte write with automatic write cycle delay.
 * Operation blocks for write cycle time after I2C transaction completes.
 *
 * @param addr Memory address [0x0000 - 0x7FFF]
 * @param data Byte value to write
 *
 * @return 0 on success
 * @return -1 on I2C communication failure
 *
 * @note Thread-safety: Must be called from StorageTask with eepromMtx held
 * @note Blocks for CAT24C256_WRITE_CYCLE_MS after write
 */
int CAT24C256_WriteByte(uint16_t addr, uint8_t data);

/**
 * @brief Read single byte from EEPROM.
 *
 * @details
 * Performs two-phase I2C transaction:
 * 1. Write 16-bit address with repeated start
 * 2. Read one data byte
 *
 * @param addr Memory address [0x0000 - 0x7FFF]
 *
 * @return Byte value read from EEPROM
 * @return 0xFF on I2C communication failure
 *
 * @note Thread-safety: Must be called from StorageTask with eepromMtx held
 */
uint8_t CAT24C256_ReadByte(uint16_t addr);

/**
 * @brief Write buffer to EEPROM with automatic page boundary handling.
 *
 * @details
 * Writes data in page-aligned chunks to prevent page boundary corruption.
 * Automatically splits writes that cross 64-byte page boundaries into
 * multiple operations with proper write cycle delays between each.
 *
 * Page Boundary Example:
 * - Write 100 bytes starting at 0x0020:
 *   - Chunk 1: 0x0020-0x003F (32 bytes to page boundary)
 *   - Delay 5ms
 *   - Chunk 2: 0x0040-0x007F (64 bytes, full page)
 *   - Delay 5ms
 *   - Chunk 3: 0x0080-0x0083 (4 bytes remaining)
 *   - Delay 5ms
 *
 * @param addr Starting memory address [0x0000 - 0x7FFF]
 * @param data Source buffer pointer
 * @param len Number of bytes to write
 *
 * @return 0 on complete success
 * @return -1 on NULL pointer or any I2C error
 *
 * @note Thread-safety: Must be called from StorageTask with eepromMtx held
 * @note Total operation time: (number_of_pages × 5ms) + I2C transfer time
 */
int CAT24C256_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Read buffer from EEPROM.
 *
 * @details
 * Performs sequential read using EEPROM auto-increment feature.
 * Can read across page boundaries without restriction. On error,
 * fills destination buffer with 0xFF to indicate invalid data.
 *
 * @param addr Starting memory address [0x0000 - 0x7FFF]
 * @param buffer Destination buffer pointer
 * @param len Number of bytes to read
 *
 * @return None
 *
 * @note Thread-safety: Must be called from StorageTask with eepromMtx held
 * @note Buffer filled with 0xFF on I2C communication failure
 */
void CAT24C256_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len);

/**
 * @brief Execute comprehensive EEPROM self-test.
 *
 * @details
 * Validates EEPROM functionality by writing and reading back a test
 * pattern designed to detect common failure modes:
 *
 * Test Pattern (8 bytes):
 * - 0xAA, 0x55: Alternating bit patterns (detects stuck bits)
 * - 0xCC, 0x33: Adjacent bit pairs (detects crosstalk)
 * - 0xF0, 0x0F: Nibble patterns (detects partial byte errors)
 * - 0x00, 0xFF: Extreme values (detects threshold issues)
 *
 * Procedure:
 * 1. Write test pattern to specified address
 * 2. Wait 10ms for write completion
 * 3. Read back pattern
 * 4. Compare byte-by-byte with expected values
 *
 * @param test_addr Starting address for test (modifies 8 bytes)
 *
 * @return true if all bytes match expected pattern
 * @return false on write failure, read failure, or data mismatch
 *
 * @warning Overwrites 8 bytes at test_addr
 * @note Thread-safety: Must be called from StorageTask with eepromMtx held
 */
bool CAT24C256_SelfTest(uint16_t test_addr);
/** @} */

#endif /* CAT24C256_DRIVER_H */

/** @} */
