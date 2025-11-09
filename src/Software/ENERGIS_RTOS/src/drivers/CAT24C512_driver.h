/**
 * @file CAT24C512_driver.h
 * @author DvidMakesThings - David Sipos
 * @brief CAT24C512 I2C EEPROM driver implementation (RTOS-compatible)
 *
 * @version 1.1.0
 * @date 2025-11-06
 * @details
 * RTOS-compatible driver for CAT24C512 EEPROM (512Kbit / 64KB I2C EEPROM).
 * Uses CONFIG.h definitions for pins, I2C settings, and logging.
 *
 * RTOS Compliance:
 * - Uses vTaskDelay() instead of sleep_ms()
 * - Uses CONFIG.h logging macros (INFO_PRINT, ERROR_PRINT)
 * - Uses CONFIG.h pin definitions (I2C1_SDA, I2C1_SCL, EEPROM_I2C)
 * - Thread-safe when used with eepromMtx (handled by StorageTask)
 *
 * Only StorageTask should call these functions (with mutex protection).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CAT24C512_DRIVER_H
#define CAT24C512_DRIVER_H

#include "../CONFIG.h"

/* ==================== CAT24C512 Constants ==================== */

/**
 * @brief Default I2C device address for CAT24C512 (7-bit address)
 * @note Address can be modified by hardware address pins A0-A2 if available on the device
 */
#define CAT24C512_I2C_ADDR 0x50

/**
 * @brief Page size for write operations (128 bytes)
 * @note Write operations must respect page boundaries for proper operation
 */
#define CAT24C512_PAGE_SIZE 128

/**
 * @brief Total memory size of the CAT24C512 EEPROM (64KB)
 */
#define CAT24C512_TOTAL_SIZE 65536

/**
 * @brief Write cycle time in milliseconds (typical: 5ms max)
 * @note This delay is required after each write operation to ensure data integrity
 */
#define CAT24C512_WRITE_CYCLE_MS 5

/* ==================== Function Prototypes ==================== */

/**
 * @brief Initialize the CAT24C512 EEPROM driver.
 *
 * Uses CONFIG.h definitions: I2C1_SDA, I2C1_SCL, EEPROM_I2C, I2C_SPEED.
 * I2C peripheral already initialized by system_startup_init().
 *
 * RTOS: Safe to call from any task context.
 */
/**
 * @brief Initializes the CAT24C512 EEPROM hardware interface
 *
 * Sets up the I2C interface pins (SDA and SCL) with proper function assignments and pull-up
 * resistors. The I2C peripheral must be previously initialized by system_startup_init().
 *
 * @param None
 * @return None
 * @note Thread-safe: Can be called from any task context
 */
void CAT24C512_Init(void);

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
int CAT24C512_WriteByte(uint16_t addr, uint8_t data);

/**
 * @brief Reads a single byte from a specific EEPROM address
 *
 * Performs an atomic read operation of one byte from the specified memory address.
 * Uses a two-step process: first sending the address, then reading the data.
 *
 * @param addr Memory address between 0x0000 and 0xFFFF to read from
 * @return The byte value read from EEPROM (0xFF if read fails)
 * @note Thread-safety: Must only be called from StorageTask with eepromMtx protection
 */
uint8_t CAT24C512_ReadByte(uint16_t addr);

/**
 * @brief Writes multiple bytes to EEPROM starting at specified address
 *
 * Handles page boundary alignment automatically by splitting writes into appropriate chunks.
 * Includes write cycle delays between pages to ensure data integrity.
 *
 * @param addr Starting memory address between 0x0000 and 0xFFFF
 * @param data Pointer to source buffer containing data to write
 * @param len Number of bytes to write (must not exceed available memory)
 * @return 0 on successful write of all data, -1 if any write operation fails
 * @note Thread-safety: Must only be called from StorageTask with eepromMtx protection
 */
int CAT24C512_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Reads multiple bytes from EEPROM into provided buffer
 *
 * Performs a sequential read operation starting from the specified address.
 * No address boundary checking is performed - caller must ensure valid range.
 *
 * @param addr Starting memory address between 0x0000 and 0xFFFF
 * @param buffer Pointer to destination buffer for read data
 * @param len Number of bytes to read
 * @return None
 * @note Thread-safety: Must only be called from StorageTask with eepromMtx protection
 */
void CAT24C512_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len);

/**
 * @brief Reads entire EEPROM contents into provided buffer
 *
 * Performs a complete memory dump of the CAT24C512 from address 0x0000 to 0xFFFF.
 *
 * @param buffer Pointer to pre-allocated buffer of at least CAT24C512_TOTAL_SIZE bytes (64KB)
 * @return None
 * @note Buffer size requirement: Caller must allocate 65536 bytes
 */
void CAT24C512_Dump(uint8_t *buffer);

/**
 * @brief Outputs formatted hex dump of EEPROM contents for testing
 *
 * Creates a human-readable hex dump with address headers and 16-byte rows.
 * Output is wrapped in EE_DUMP_START and EE_DUMP_END markers for automated testing.
 * Includes periodic task yields to prevent RTOS task starvation.
 *
 * @param None
 * @return None
 * @note Thread-safety: Must only be called from StorageTask with eepromMtx protection
 */
void CAT24C512_DumpFormatted(void);

/**
 * @brief Performs a self-test of EEPROM read/write functionality
 *
 * Writes a predefined test pattern to the specified address and verifies readback.
 * Test pattern includes alternating bits and edge cases (0x00, 0xFF, etc.).
 *
 * @param test_addr Starting address for test pattern (ensure 8 bytes available)
 * @return true if test pattern written and read back successfully, false on any error
 * @note Modifies EEPROM content at test_addr through test_addr+7
 */
bool CAT24C512_SelfTest(uint16_t test_addr);

#endif /* CAT24C512_DRIVER_H */
