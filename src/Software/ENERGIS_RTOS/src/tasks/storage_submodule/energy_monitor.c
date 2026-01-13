/**
 * @file src/tasks/storage_submodule/energy_monitor.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of energy monitoring data management. Provides ring buffer
 * functionality for storing periodic energy consumption records in EEPROM.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define ST_ENERGY_MON_TAG "[ST-EMON]"

/** @brief Write energy monitoring region. See energy_monitor.h. */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_ENERGY_MON, 0x1);
        ERROR_PRINT_CODE(err_code, "%s EEPROM_WriteEnergyMonitoring: Write length exceeds size\r\n",
                         ST_ENERGY_MON_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    return CAT24C256_WriteBuffer(EEPROM_ENERGY_MON_START, data, (uint16_t)len);
}

/** @brief Read energy monitoring region. See energy_monitor.h. */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_ENERGY_MON, 0x2);
        ERROR_PRINT_CODE(err_code, "%s EEPROM_ReadEnergyMonitoring: Read length exceeds size\r\n",
                         ST_ENERGY_MON_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    CAT24C256_ReadBuffer(EEPROM_ENERGY_MON_START, data, (uint32_t)len);
    return 0;
}

/** @brief Append energy record to ring buffer. See energy_monitor.h. */
int EEPROM_AppendEnergyRecord(const uint8_t *data) {
    /* Read current write pointer from start of energy section */
    uint16_t ptr = 0;

    CAT24C256_ReadBuffer(EEPROM_ENERGY_MON_START, (uint8_t *)&ptr, 2);

    /* Calculate address for new record (skip pointer bytes) */
    uint16_t addr = EEPROM_ENERGY_MON_START + ENERGY_MON_POINTER_SIZE + (ptr * ENERGY_RECORD_SIZE);

    /* Write the energy record */

    if (CAT24C256_WriteBuffer(addr, data, ENERGY_RECORD_SIZE) != 0) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_ENERGY_MON, 0x3);
        ERROR_PRINT_CODE(
            err_code,
            "%s EEPROM_AppendEnergyRecord: Failed to write energy record at address 0x%04X\r\n",
            ST_ENERGY_MON_TAG, addr);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    /* Increment pointer and wrap if at buffer end */
    ptr++;
    if ((ptr * ENERGY_RECORD_SIZE) >= (EEPROM_ENERGY_MON_SIZE - ENERGY_MON_POINTER_SIZE))
        ptr = 0;

    /* Update pointer in EEPROM */

    return CAT24C256_WriteBuffer(EEPROM_ENERGY_MON_START, (uint8_t *)&ptr, 2);
}