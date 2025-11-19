/**
 * @file src/tasks/storage_submodule/user_output.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of user output (relay) state persistence. Manages default
 * power-on states for all 8 relay channels in EEPROM.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define ST_USER_OUTPUT_TAG "[ST-USRO]"

/**
 * @brief Write relay states to EEPROM user output section.
 *
 * Stores power-on states for relay channels. Each byte represents one channel:
 * - 0x00 = OFF on power-up
 * - 0x01 = ON on power-up
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Pointer to relay state array
 * @param len Number of bytes to write (typically 8 for 8 channels)
 * @return 0 on success, -1 on bounds check failure or I2C error
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len) {
    /* Bounds check */
    if (len > EEPROM_USER_OUTPUT_SIZE) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x1);
        ERROR_PRINT_CODE(err_code, "%s Write length exceeds size\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    /* Write to EEPROM */
    return CAT24C256_WriteBuffer(EEPROM_USER_OUTPUT_START, data, (uint16_t)len);
}

/**
 * @brief Read relay states from EEPROM user output section.
 *
 * Retrieves saved power-on states for relay channels.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer for relay states
 * @param len Number of bytes to read (typically 8 for 8 channels)
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len) {
    /* Bounds check */
    if (len > EEPROM_USER_OUTPUT_SIZE) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x2);
        ERROR_PRINT_CODE(err_code, "%s Read length exceeds size\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    /* Read from EEPROM */
    CAT24C256_ReadBuffer(EEPROM_USER_OUTPUT_START, data, (uint32_t)len);
    return 0;
}