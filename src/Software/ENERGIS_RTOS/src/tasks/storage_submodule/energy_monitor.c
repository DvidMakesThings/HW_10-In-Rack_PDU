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

/**
 * @brief Write energy monitoring region to EEPROM.
 *
 * Used for bulk writes of energy data. First 2 bytes are ring buffer pointer.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Source buffer containing energy data
 * @param len Number of bytes to write
 * @return 0 on success, -1 on bounds check failure or I2C error
 */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    return CAT24C256_WriteBuffer(EEPROM_ENERGY_MON_START, data, (uint16_t)len);
}

/**
 * @brief Read energy monitoring region from EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len Number of bytes to read
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    CAT24C256_ReadBuffer(EEPROM_ENERGY_MON_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Append one energy record to the ring buffer.
 *
 * Ring buffer structure:
 * - Address 0x0800-0x0801: Write pointer (uint16_t)
 * - Address 0x0802+: Energy records (ENERGY_RECORD_SIZE bytes each)
 *
 * Process:
 * 1. Read current write pointer
 * 2. Calculate record address
 * 3. Write new record
 * 4. Increment and wrap pointer if needed
 * 5. Update pointer in EEPROM
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Pointer to energy record (ENERGY_RECORD_SIZE bytes)
 * @return 0 on success, -1 on I2C write error
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data) {
    /* Read current write pointer from start of energy section */
    uint16_t ptr = 0;
    CAT24C256_ReadBuffer(EEPROM_ENERGY_MON_START, (uint8_t *)&ptr, 2);

    /* Calculate address for new record (skip pointer bytes) */
    uint16_t addr = EEPROM_ENERGY_MON_START + ENERGY_MON_POINTER_SIZE + (ptr * ENERGY_RECORD_SIZE);

    /* Write the energy record */
    if (CAT24C256_WriteBuffer(addr, data, ENERGY_RECORD_SIZE) != 0)
        return -1;

    /* Increment pointer and wrap if at buffer end */
    ptr++;
    if ((ptr * ENERGY_RECORD_SIZE) >= (EEPROM_ENERGY_MON_SIZE - ENERGY_MON_POINTER_SIZE))
        ptr = 0;

    /* Update pointer in EEPROM */
    return CAT24C256_WriteBuffer(EEPROM_ENERGY_MON_START, (uint8_t *)&ptr, 2);
}