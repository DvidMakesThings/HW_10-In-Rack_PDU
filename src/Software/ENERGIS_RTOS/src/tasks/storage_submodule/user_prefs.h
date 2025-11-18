/**
 * @file src/tasks/storage_submodule/user_prefs.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage08 8. User Preferences
 * @ingroup storage
 * @brief Device name, location, and display settings
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Manages user-configurable preferences including device name, location string,
 * and temperature unit. Uses CRC-8 validation for data integrity.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef USER_PREFS_H
#define USER_PREFS_H

#include "../../CONFIG.h"

/**
 * @brief Write raw user preferences block.
 * @param data Pointer to source buffer
 * @param len Must be <= EEPROM_USER_PREF_SIZE
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len);

/**
 * @brief Read raw user preferences block.
 * @param data Destination buffer
 * @param len Must be <= EEPROM_USER_PREF_SIZE
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len);

/**
 * @brief Write user preferences with CRC-8 verification.
 *
 * Layout: device_name(32) + location(32) + temp_unit(1) + CRC(1) = 66 bytes
 * Uses split writes for page boundary safety.
 *
 * @param prefs Pointer to structured preferences
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on null input or write error
 */
int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs);

/**
 * @brief Read and verify user preferences with CRC-8.
 *
 * Validates CRC and ensures strings are null-terminated.
 *
 * @param prefs Destination structure for preferences
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 if CRC OK, -1 on CRC mismatch or null pointer
 */
int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs);

/**
 * @brief Load user preferences from EEPROM or return built-in defaults on failure.
 *
 * Attempts to read preferences with CRC validation. On failure, returns
 * default device name, location, and temperature unit.
 *
 * @return userPrefInfo structure with valid data
 */
userPrefInfo LoadUserPreferences(void);

/**
 * @brief Write default device name and location to preferences (with CRC).
 *
 * Used during factory defaults initialization to write default:
 * - Device name: "ENERGIS PDU"
 * - Location: "Unknown"
 * - Temp unit: Celsius (0)
 *
 * @return 0 on success, -1 on write error
 */
int EEPROM_WriteDefaultNameLocation(void);

#endif /* USER_PREFS_H */

/** @} */