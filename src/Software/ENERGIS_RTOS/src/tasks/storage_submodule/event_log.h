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
 * @brief Read event log region from EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len  Number of bytes to read
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len);

/**
 * @brief Append one event log entry (16-bit code) to the ring buffer.
 *
 * Ring buffer structure:
 * - [EEPROM_EVENT_LOG_START .. +1]        : uint16_t write pointer (entry index)
 * - [START + EVENT_LOG_POINTER_SIZE .. ]  : EVENT_LOG_ENTRY_SIZE-byte entries
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param entry Pointer to EVENT_LOG_ENTRY_SIZE bytes (here: uint16_t error code).
 * @return 0 on success, -1 on I2C write error
 */
int EEPROM_AppendEventLog(const uint8_t *entry);

#endif /* EVENT_LOG_H */

/** @} */