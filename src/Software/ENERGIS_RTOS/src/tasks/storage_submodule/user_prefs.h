/**
 * @file src/tasks/storage_submodule/user_prefs.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage11 11. User Preferences
 * @ingroup tasks10
 * @brief Device name, location, and display settings with CRC-8 validation
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details
 * This module manages user-configurable preferences stored in EEPROM with CRC-8
 * data integrity protection. Includes device identification strings and display
 * settings with automatic fallback to defaults on corruption.
 *
 * Key Features:
 * - Device name customization (up to 31 characters)
 * - Physical location string (up to 31 characters)
 * - Temperature unit selection (Celsius/Fahrenheit)
 * - CRC-8 validation for data integrity
 * - Automatic fallback to built-in defaults
 * - Split-write protection for EEPROM page boundaries
 *
 * EEPROM Memory Layout (EEPROM_USER_PREF_START, 66 bytes total):
 * - Offset 0-31:  Device name string (null-terminated)
 * - Offset 32-63: Location string (null-terminated)
 * - Offset 64:    Temperature unit (0=Celsius, 1=Fahrenheit)
 * - Offset 65:    CRC-8 checksum
 *
 * Data Structure:
 * - Device Name: 32-byte field for user-defined device name
 *   * Default: "ENERGIS PDU"
 *   * Used in SNMP sysName and web interface
 *   * Automatically null-terminated
 *
 * - Location: 32-byte field for physical location
 *   * Default: "Unknown"
 *   * Used in SNMP sysLocation and web interface
 *   * Automatically null-terminated
 *
 * - Temperature Unit: 1-byte enum
 *   * 0 = Celsius (default)
 *   * 1 = Fahrenheit
 *   * Applied to web interface and SNMP OID output
 *
 * CRC-8 Validation:
 * - Polynomial: 0x07 (via calculate_crc8())
 * - Calculated over first 65 bytes (name + location + temp_unit)
 * - Stored in byte 66
 * - Read operations fail on CRC mismatch
 * - Failed reads trigger fallback to defaults
 *
 * Split-Write Protection:
 * - EEPROM_WriteUserPrefsWithChecksum() uses split writes
 * - Prevents corruption from writes spanning page boundaries
 * - First 64 bytes (strings): single 64-byte write
 * - Last 2 bytes (temp_unit + CRC): separate 2-byte write
 * - CAT24C256 has 64-byte page size requiring alignment
 *
 * Load Operation Flow:
 * 1. LoadUserPreferences() reads from EEPROM with CRC validation
 * 2. If CRC valid:
 *    a. Return validated configuration
 *    b. Ensure strings are null-terminated
 * 3. If CRC invalid or EEPROM empty:
 *    a. Load DEFAULT_USER_PREFS structure
 *    b. Persist defaults to EEPROM for future boots
 *    c. Return default configuration
 *
 * Default Configuration:
 * - Device name: "ENERGIS PDU"
 * - Location: "Unknown"
 * - Temperature unit: Celsius (0)
 * - Used on first boot or EEPROM corruption
 *
 * Thread Safety:
 * - All read/write functions require eepromMtx held by caller
 * - LoadUserPreferences() manages mutex internally
 * - Prevents concurrent access corruption
 *
 * Integration Points:
 * - storage_common: CRC-8 calculation
 * - StorageTask: Provides DEFAULT_USER_PREFS and eepromMtx
 * - NetTask: Reads device name for SNMP sysName
 * - Web interface: Displays device name and location
 * - MeterTask: Applies temperature unit to sensor readings
 *
 * API Layers:
 * - Raw APIs: Direct EEPROM access without CRC (for legacy/debug)
 * - CRC APIs: Recommended for production use with integrity checking
 * - Load API: High-level with defaults and automatic fallback
 *
 * @note Always prefer CRC-verified APIs for production code.
 * @note LoadUserPreferences() is the recommended high-level interface.
 * @note Strings are always null-terminated on read operations.
 * @note Split writes required to respect CAT24C256 page boundaries.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef USER_PREFS_H
#define USER_PREFS_H

#include "../../CONFIG.h"

/** @name Raw APIs
 * @{ */
/**
 * @brief Write raw user preferences block without CRC validation.
 *
 * Low-level write operation that directly writes buffer to EEPROM user preferences
 * section without CRC generation. Provided for legacy compatibility and debug access.
 * Production code should use EEPROM_WriteUserPrefsWithChecksum() instead.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_USER_PREF_START
 * - Maximum size: EEPROM_USER_PREF_SIZE bytes
 * - No CRC appended or validated
 *
 * @param[in] data Pointer to source buffer containing user preferences.
 * @param[in] len  Number of bytes to write. Must be <= EEPROM_USER_PREF_SIZE.
 *
 * @return 0 if write successful.
 * @return -1 if length exceeds EEPROM_USER_PREF_SIZE or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning No CRC generation or validation performed.
 * @warning Prefer EEPROM_WriteUserPrefsWithChecksum() for production code.
 * @note Bounds checking performed before write operation.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len);

/**
 * @brief Read raw user preferences block without CRC validation.
 *
 * Low-level read operation that directly reads buffer from EEPROM user preferences
 * section without CRC verification. Provided for legacy compatibility and debug access.
 * Production code should use EEPROM_ReadUserPrefsWithChecksum() instead.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_USER_PREF_SIZE
 * - Maximum size: EEPROM_USER_PREF_SIZE bytes
 * - No CRC verified
 *
 * @param[out] data Destination buffer for user preferences.
 * @param[in]  len  Number of bytes to read. Must be <= EEPROM_USER_PREF_SIZE.
 *
 * @return 0 if read successful.
 * @return -1 if length exceeds EEPROM_USER_PREF_SIZE.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning No CRC verification performed on read data.
 * @warning Prefer EEPROM_ReadUserPrefsWithChecksum() for production code.
 * @note Bounds checking performed before read operation.
 * @note Read failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Caller responsible for validating data integrity.
 */
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len);
/** @} */

/** @name CRC APIs
 * @{ */
/**
 * @brief Write user preferences with CRC-8 validation appended.
 *
 * Recommended write operation for production code. Packs userPrefInfo structure into
 * 66-byte EEPROM layout with CRC-8 checksum appended for integrity verification.
 * Uses split writes to respect CAT24C256 page boundaries and prevent corruption.
 *
 * EEPROM Layout (66 bytes total):
 * - Offset 0-31:  Device name string (32 bytes, null-terminated)
 * - Offset 32-63: Location string (32 bytes, null-terminated)
 * - Offset 64:    Temperature unit (1 byte, 0=Celsius 1=Fahrenheit)
 * - Offset 65:    CRC-8 checksum (1 byte)
 *
 * Split-Write Strategy:
 * - First write: Bytes 0-63 (device name + location)
 * - Second write: Bytes 64-65 (temp_unit + CRC)
 * - Prevents corruption from writes spanning 64-byte page boundaries
 *
 * CRC Calculation:
 * - Algorithm: CRC-8 polynomial 0x07 via calculate_crc8()
 * - Input: Bytes 0-64 (device name + location + temp_unit)
 * - Output: Stored in byte 65
 *
 * @param[in] prefs Pointer to userPrefInfo structure containing preferences.
 *
 * @return 0 if write successful.
 * @return -1 if prefs is NULL or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Null pointer check performed before write operation.
 * @note CRC automatically calculated and appended.
 * @note Split writes prevent page boundary corruption.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note This is the recommended API for production user prefs writes.
 */
int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs);

/**
 * @brief Read and verify user preferences with CRC-8 validation.
 *
 * Recommended read operation for production code. Reads 66-byte EEPROM user preferences
 * and verifies CRC-8 checksum. Unpacks buffer into userPrefInfo structure only if CRC
 * validation passes. Ensures strings are null-terminated.
 *
 * EEPROM Layout (66 bytes total):
 * - Offset 0-31:  Device name string (32 bytes, null-terminated)
 * - Offset 32-63: Location string (32 bytes, null-terminated)
 * - Offset 64:    Temperature unit (1 byte, 0=Celsius 1=Fahrenheit)
 * - Offset 65:    CRC-8 checksum (1 byte)
 *
 * CRC Validation:
 * - Calculate CRC-8 over bytes 0-64
 * - Compare with stored CRC in byte 65
 * - Fail if mismatch detected
 *
 * String Safety:
 * - Device name and location forcibly null-terminated
 * - Prevents buffer overruns from corrupted EEPROM data
 *
 * @param[out] prefs Pointer to userPrefInfo structure to receive preferences.
 *
 * @return 0 if CRC validation passed and structure populated.
 * @return -1 if prefs is NULL or CRC mismatch detected.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Null pointer check performed before read operation.
 * @note CRC mismatch logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Strings forcibly null-terminated for safety.
 * @note Caller should use LoadUserPreferences() for automatic fallback to defaults.
 * @note This is the recommended API for production user prefs reads.
 */
int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs);
/** @} */

/** @name Load API
 * @{ */
/**
 * @brief Load user preferences from EEPROM with automatic fallback to defaults.
 *
 * High-level interface for loading user preferences. Attempts EEPROM read with CRC
 * validation. On success, returns validated configuration. On failure, returns default
 * configuration and persists defaults to EEPROM. Manages eepromMtx internally for
 * thread-safe operation.
 *
 * Operation Flow:
 * 1. Acquire eepromMtx for thread-safe access
 * 2. Call EEPROM_ReadUserPrefsWithChecksum() to read with CRC validation
 * 3. If CRC valid:
 *    a. Return validated preferences
 * 4. If CRC invalid or EEPROM empty:
 *    a. Load DEFAULT_USER_PREFS structure from StorageTask
 *    b. Persist defaults to EEPROM for future boots
 *    c. Log fallback event as ERR_SEV_WARNING
 *    d. Return default preferences
 * 5. Release eepromMtx
 *
 * Default Configuration:
 * - Device name: "ENERGIS PDU"
 * - Location: "Unknown"
 * - Temperature unit: Celsius (0)
 * - Safe fallback for first boot or EEPROM corruption
 *
 * @return userPrefInfo structure containing valid user preferences.
 *         Always returns valid data, never fails.
 *
 * @note This is the recommended high-level API for loading user preferences.
 * @note Manages eepromMtx internally; caller should NOT hold mutex.
 * @note Always returns valid configuration via fallback to defaults.
 * @note Default configs automatically persisted to EEPROM.
 * @note Called by web interface and SNMP handlers during initialization.
 */
userPrefInfo LoadUserPreferences(void);
/** @} */

/** @name Defaults API
 * @{ */
/**
 * @brief Write default device name and location to preferences with CRC-8.
 *
 * Writes factory default user preferences to EEPROM with CRC-8 validation.
 * Used during first-boot initialization to establish baseline configuration.
 *
 * Default Values Written:
 * - Device name: "ENERGIS PDU"
 * - Location: "Unknown"
 * - Temperature unit: Celsius (0)
 *
 * CRC Calculation:
 * - CRC-8 calculated over default values
 * - Stored in byte 65 of preferences block
 *
 * @return 0 if write successful.
 * @return -1 if write operation fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Used by EEPROM_WriteFactoryDefaults() during first boot.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Establishes known-good configuration for fresh devices.
 */
int EEPROM_WriteDefaultNameLocation(void);
/** @} */

#endif /* USER_PREFS_H */

/** @} */