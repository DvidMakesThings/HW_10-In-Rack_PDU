/**
 * @file src/tasks/storage_submodule/event_log.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of event log management. Provides ring buffer functionality
 * for storing system events, faults, and configuration changes in EEPROM.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

/**
 * @brief Write event log region to EEPROM.
 *
 * Used for bulk writes of event log data. First 2 bytes are ring buffer pointer.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Source buffer containing event log data
 * @param len Number of bytes to write
 * @return 0 on success, -1 on bounds check failure or I2C error
 */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    return CAT24C256_WriteBuffer(EEPROM_EVENT_LOG_START, data, (uint16_t)len);
}

/**
 * @brief Read event log region from EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len Number of bytes to read
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    CAT24C256_ReadBuffer(EEPROM_EVENT_LOG_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Append one event log entry to the ring buffer.
 *
 * Ring buffer structure:
 * - Address 0x1000-0x1001: Write pointer (uint16_t)
 * - Address 0x1002+: Event log entries (EVENT_LOG_ENTRY_SIZE bytes each)
 *
 * Process:
 * 1. Read current write pointer
 * 2. Calculate entry address
 * 3. Write new entry
 * 4. Increment and wrap pointer if needed
 * 5. Update pointer in EEPROM
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param entry Pointer to event log entry (EVENT_LOG_ENTRY_SIZE bytes)
 * @return 0 on success, -1 on I2C write error
 */
int EEPROM_AppendEventLog(const uint8_t *entry) {
    /* Read current write pointer from start of event log section */
    uint16_t ptr = 0;
    CAT24C256_ReadBuffer(EEPROM_EVENT_LOG_START, (uint8_t *)&ptr, 2);

    /* Calculate address for new entry (skip pointer bytes) */
    uint16_t addr = EEPROM_EVENT_LOG_START + EVENT_LOG_POINTER_SIZE + (ptr * EVENT_LOG_ENTRY_SIZE);

    /* Write the event log entry */
    if (CAT24C256_WriteBuffer(addr, entry, EVENT_LOG_ENTRY_SIZE) != 0)
        return -1;

    /* Increment pointer and wrap if at buffer end */
    ptr++;
    if ((ptr * EVENT_LOG_ENTRY_SIZE) >= (EEPROM_EVENT_LOG_SIZE - EVENT_LOG_POINTER_SIZE))
        ptr = 0;

    /* Update pointer in EEPROM */
    return CAT24C256_WriteBuffer(EEPROM_EVENT_LOG_START, (uint8_t *)&ptr, 2);
}