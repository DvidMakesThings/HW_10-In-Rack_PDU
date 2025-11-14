/**
 * @file src/tasks/storage_submodule/event_log.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage07 7. Event Logging
 * @ingroup storage
 * @brief System event logging with ring buffer
 * @{
 * 
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Manages system event logs in EEPROM as a ring buffer.
 * Stores fault history, power events, and configuration changes.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include "../../CONFIG.h"

/**
 * @brief Write the event log region.
 * @param data Pointer to source buffer
 * @param len Must be <= EEPROM_EVENT_LOG_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len);

/**
 * @brief Read the event log region.
 * @param data Destination buffer
 * @param len Must be <= EEPROM_EVENT_LOG_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len);

/**
 * @brief Append one event log entry to the ring buffer and update pointer.
 *
 * Implements circular buffer behavior:
 * 1. Reads current write pointer
 * 2. Writes new entry at pointer location
 * 3. Increments pointer (wraps to 0 if at end)
 * 4. Updates pointer in EEPROM
 *
 * @param entry Pointer to one entry of size EVENT_LOG_ENTRY_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on write error
 */
int EEPROM_AppendEventLog(const uint8_t *entry);

#endif /* EVENT_LOG_H */

/** @} */