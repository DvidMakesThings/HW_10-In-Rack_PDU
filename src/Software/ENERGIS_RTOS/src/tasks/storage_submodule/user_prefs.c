/**
 * @file src/tasks/storage_submodule/user_prefs.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of user preferences management. Handles device name, location,
 * and temperature unit settings with CRC-8 validation for data integrity.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

/* External declarations from StorageTask.c */
extern const userPrefInfo DEFAULT_USER_PREFS;

#define STORAGE_TASK_TAG "[Storage]"

/**
 * @brief Write raw user preferences block to EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Source buffer containing preferences data
 * @param len Number of bytes to write
 * @return 0 on success, -1 on bounds check failure or I2C error
 */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, data, (uint16_t)len);
}

/**
 * @brief Read raw user preferences block from EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len Number of bytes to read
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Write user preferences with CRC-8 validation.
 *
 * Layout: device_name(32) + location(32) + temp_unit(1) + CRC(1) = 66 bytes
 * Uses split writes to avoid page boundary issues with CAT24C512.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param prefs Pointer to user preferences structure
 * @return 0 on success, -1 on null pointer or I2C error
 */
int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs) {
    if (!prefs)
        return -1;

    uint8_t buffer[66];

    /* Pack preferences into buffer */
    memcpy(&buffer[0], prefs->device_name, 32);
    memcpy(&buffer[32], prefs->location, 32);
    buffer[64] = prefs->temp_unit;

    /* Calculate and append CRC */
    buffer[65] = calculate_crc8(buffer, 65);

    /* Split write to avoid page boundary issues */
    int res = 0;
    res |= CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, &buffer[0], 64);
    res |= CAT24C512_WriteBuffer(EEPROM_USER_PREF_START + 64, &buffer[64], 2);

    return res;
}

/**
 * @brief Read and verify user preferences with CRC-8.
 *
 * Validates CRC and ensures strings are properly null-terminated for safety.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param prefs Pointer to user preferences structure to fill
 * @return 0 if CRC OK, -1 on CRC mismatch or null pointer
 */
int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs) {
    if (!prefs)
        return -1;

    uint8_t record[66];
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, record, 66);

    /* Verify CRC */
    if (calculate_crc8(&record[0], 65) != record[65]) {
        ERROR_PRINT("%s User prefs CRC mismatch\r\n", STORAGE_TASK_TAG);
        return -1;
    }

    /* Unpack preferences from buffer */
    memcpy(prefs->device_name, &record[0], 32);
    prefs->device_name[31] = '\0'; /* Ensure null termination */

    memcpy(prefs->location, &record[32], 32);
    prefs->location[31] = '\0'; /* Ensure null termination */

    prefs->temp_unit = record[64];

    return 0;
}

/**
 * @brief Load user preferences from EEPROM or return defaults.
 *
 * Attempts to read preferences with CRC validation. If successful, returns
 * stored values. On failure, returns built-in defaults.
 *
 * @return Valid userPrefInfo structure (either from EEPROM or defaults)
 */
userPrefInfo LoadUserPreferences(void) {
    userPrefInfo prefs;

    /* Attempt to read from EEPROM with CRC validation */
    if (EEPROM_ReadUserPrefsWithChecksum(&prefs) == 0) {
        INFO_PRINT("%s Loaded user prefs from EEPROM\r\n", STORAGE_TASK_TAG);
        return prefs;
    }

    /* CRC failed or empty EEPROM - use defaults */
    WARNING_PRINT("%s No saved prefs, using defaults\r\n", STORAGE_TASK_TAG);
    return DEFAULT_USER_PREFS;
}

/**
 * @brief Write default device name and location with CRC.
 *
 * Used during factory defaults initialization to set:
 * - Device name from DEFAULT_NAME
 * - Location from DEFAULT_LOCATION
 * - Temperature unit: Celsius (0)
 *
 * @return 0 on success, -1 on write error
 */
int EEPROM_WriteDefaultNameLocation(void) {
    return EEPROM_WriteUserPrefsWithChecksum(&DEFAULT_USER_PREFS);
}