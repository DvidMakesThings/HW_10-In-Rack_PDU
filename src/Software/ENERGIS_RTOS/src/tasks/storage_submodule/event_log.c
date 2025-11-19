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

/* Make sure each event entry is exactly one uint16_t (16-bit code). */
_Static_assert(EVENT_LOG_ENTRY_SIZE == sizeof(uint16_t),
               "EVENT_LOG_ENTRY_SIZE must match uint16_t");

/**
 * @brief Append one 16-bit event code to a ring buffer region in EEPROM.
 *
 * Ring buffer structure for a given region:
 * - [base .. base + 1]              : uint16_t write pointer (entry index)
 * - [base + EVENT_LOG_POINTER_SIZE] : first EVENT_LOG_ENTRY_SIZE-byte entry
 *
 * The buffer is append-only with wrap-around. If the stored pointer is out of
 * range, it is clamped to 0 and overwritten.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param base       Start address of the EEPROM block (error or warning region).
 * @param block_size Size in bytes of the EEPROM block.
 * @param entry      Pointer to EVENT_LOG_ENTRY_SIZE bytes (here: uint16_t code).
 *
 * @return 0 on success, -1 on I2C write error or invalid configuration.
 */
static int EEPROM_AppendCodeGeneric(uint16_t base, uint16_t block_size, const uint8_t *entry) {
    uint16_t ptr = 0;

    /* Sanity guard: block must be big enough for pointer + at least one entry */
    if (block_size <= (EVENT_LOG_POINTER_SIZE + EVENT_LOG_ENTRY_SIZE))
        return -1;

    /* Read current write pointer from start of event log section */
    CAT24C256_ReadBuffer(base, (uint8_t *)&ptr, EVENT_LOG_POINTER_SIZE);

    /* Calculate how many entries fit in the remaining space */
    const uint16_t max_entries =
        (uint16_t)((block_size - EVENT_LOG_POINTER_SIZE) / EVENT_LOG_ENTRY_SIZE);

    if (max_entries == 0)
        return -1;

    if (ptr >= max_entries) {
        /* Guard against corrupted pointer */
        ptr = 0;
    }

    /* Calculate address for new entry (skip pointer bytes) */
    uint16_t addr = (uint16_t)(base + EVENT_LOG_POINTER_SIZE + (ptr * EVENT_LOG_ENTRY_SIZE));

    /* Write the event log entry (16-bit code) */
    if (CAT24C256_WriteBuffer(addr, entry, EVENT_LOG_ENTRY_SIZE) != 0)
        return -1;

    /* Increment pointer and wrap if at buffer end */
    ptr++;
    if (ptr >= max_entries)
        ptr = 0;

    /* Update pointer in EEPROM */
    if (CAT24C256_WriteBuffer(base, (uint8_t *)&ptr, EVENT_LOG_POINTER_SIZE) != 0)
        return -1;

    return 0;
}

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
    if (len > EEPROM_EVENT_ERR_SIZE)
        return -1;
    return CAT24C256_WriteBuffer(EEPROM_EVENT_ERR_START, data, (uint16_t)len);
}

/**
 * @brief Read event log region from EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len  Number of bytes to read
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_ERR_SIZE)
        return -1;

    CAT24C256_ReadBuffer(EEPROM_EVENT_ERR_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Append one error event code (16-bit) to the error log ring buffer.
 *
 * Uses the error log region defined by EEPROM_EVENT_ERR_START / EEPROM_EVENT_ERR_SIZE
 * and stores one 16-bit code per entry.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param entry Pointer to EVENT_LOG_ENTRY_SIZE bytes (uint16_t error code).
 *
 * @return 0 on success, -1 on I2C write error.
 */
int EEPROM_AppendErrorCode(const uint8_t *entry) {
    return EEPROM_AppendCodeGeneric(EEPROM_EVENT_ERR_START, EEPROM_EVENT_ERR_SIZE, entry);
}

/**
 * @brief Append one warning event code (16-bit) to the warning log ring buffer.
 *
 * Uses the warning log region defined by EEPROM_EVENT_WARN_START / EEPROM_EVENT_WARN_SIZE
 * and stores one 16-bit code per entry.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param entry Pointer to EVENT_LOG_ENTRY_SIZE bytes (uint16_t warning code).
 *
 * @return 0 on success, -1 on I2C write error.
 */
int EEPROM_AppendWarningCode(const uint8_t *entry) {
    return EEPROM_AppendCodeGeneric(EEPROM_EVENT_WARN_START, EEPROM_EVENT_WARN_SIZE, entry);
}