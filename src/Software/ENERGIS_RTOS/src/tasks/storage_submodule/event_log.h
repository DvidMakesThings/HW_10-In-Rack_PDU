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
 * @param len Must be <= EEPROM_EVENT_ERR_SIZE
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
int EEPROM_AppendErrorCode(const uint8_t *entry);

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
int EEPROM_AppendWarningCode(const uint8_t *entry);

#endif /* EVENT_LOG_H */

/** @} */