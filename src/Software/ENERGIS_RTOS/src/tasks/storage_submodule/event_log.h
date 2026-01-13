/**
 * @file src/tasks/storage_submodule/event_log.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage05 5. Event Logging
 * @ingroup tasks10
 * @brief System event logging with separate error and warning ring buffers
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details
 * This module manages system event logs stored in EEPROM using dual ring buffer
 * structures. Provides persistent logging of errors and warnings for diagnostics,
 * troubleshooting, and compliance auditing.
 *
 * Key Features:
 * - Separate ring buffers for errors and warnings
 * - 16-bit event codes for compact storage
 * - Automatic pointer wrapping for circular storage
 * - Write pointer persistence across power cycles
 * - Support for bulk read/write operations
 * - Append-only design prevents event tampering
 *
 * EEPROM Memory Layout:
 * Two independent ring buffer regions:
 *
 * 1. Error Log (EEPROM_EVENT_ERR_START, EEPROM_EVENT_ERR_SIZE bytes):
 *    - Offset 0-1: Write pointer (uint16_t, entry index)
 *    - Offset 2+:  Error codes (EVENT_LOG_ENTRY_SIZE bytes each)
 *
 * 2. Warning Log (EEPROM_EVENT_WARN_START, EEPROM_EVENT_WARN_SIZE bytes):
 *    - Offset 0-1: Write pointer (uint16_t, entry index)
 *    - Offset 2+:  Warning codes (EVENT_LOG_ENTRY_SIZE bytes each)
 *
 * Event Code Format:
 * - Size: EVENT_LOG_ENTRY_SIZE bytes (typically 2 bytes for uint16_t)
 * - Structure: 16-bit packed event code via ERR_MAKE_CODE macro
 *   * Bits 15-12: Module ID (identifies source subsystem)
 *   * Bits 11-8:  Severity level (ERR_SEV_ERROR / ERR_SEV_WARNING)
 *   * Bits 7-4:   Function ID (identifies specific function)
 *   * Bits 3-0:   Error instance (distinguishes multiple errors in same function)
 *
 * Ring Buffer Structure (per buffer):
 * - First 2 bytes (EVENT_LOG_POINTER_SIZE): Write pointer
 *   * Stores index of next entry to write
 *   * Automatically wraps to 0 when buffer fills
 *   * Persisted to EEPROM after each append
 *
 * - Remaining bytes: Event code array
 *   * Each entry: EVENT_LOG_ENTRY_SIZE bytes (2 bytes for uint16_t)
 *   * Number of entries: (BUFFER_SIZE - 2) / 2
 *   * Older entries overwritten when buffer wraps
 *
 * Append Operation:
 * 1. Read current write pointer from buffer start
 * 2. Validate pointer (clamp to 0 if out of range)
 * 3. Calculate address for new entry: BASE + 2 + (ptr × ENTRY_SIZE)
 * 4. Write new event code to calculated address
 * 5. Increment pointer (wrap to 0 if at end of buffer)
 * 6. Write updated pointer back to buffer start
 * 7. Oldest entry automatically overwritten on wrap
 *
 * Error vs. Warning Classification:
 * - Errors: Critical faults requiring attention
 *   * Overcurrent trips
 *   * I2C communication failures
 *   * EEPROM write errors
 *   * Watchdog resets
 *   * Ethernet link failures
 *
 * - Warnings: Non-critical events for awareness
 *   * Temperature threshold crossings
 *   * MAC address repairs
 *   * CRC mismatches with successful recovery
 *   * Button long-press events
 *   * DHCP fallback to static IP
 *
 * Bulk Operations:
 * - EEPROM_WriteEventLogs(): Bulk write entire error region (for restoration)
 * - EEPROM_ReadEventLogs(): Bulk read entire error region (for backup/analysis)
 * - Used for firmware updates, factory reset, or diagnostics extraction
 *
 * Thread Safety:
 * - All read/write functions require eepromMtx held by caller
 * - Prevents concurrent access corruption
 * - Ensures atomic read-modify-write for pointer updates
 *
 * Integration Points:
 * - All modules: Append error/warning codes via Storage_EnqueueErrorCode()
 * - StorageTask: Processes queued codes and calls EEPROM append functions
 * - Web interface: Retrieves historical logs for diagnostics page
 * - SNMP: Provides event statistics via custom OIDs
 * - Console commands: Reads and displays event logs
 *
 * Usage Pattern:
 * 1. Module detects error or warning condition
 * 2. Module calls Storage_EnqueueErrorCode() with ERR_MAKE_CODE() value
 * 3. StorageTask processes queue and calls:
 *    a. EEPROM_AppendErrorCode() for errors (ERR_SEV_ERROR)
 *    b. EEPROM_AppendWarningCode() for warnings (ERR_SEV_WARNING)
 * 4. Web interface or SNMP reads historical logs:
 *    a. Call EEPROM_ReadEventLogs() to bulk read error buffer
 *    b. Parse codes using write pointer to determine newest/oldest
 *    c. Decode codes using error_code.h lookup tables
 *
 * Pointer Management:
 * - Pointer tracks next write position (not last written position)
 * - Valid pointer range: 0 to max_entries-1
 * - Corrupted pointer (out of range) clamped to 0
 * - Empty buffer indicated by uninitialized EEPROM (all 0xFF)
 *
 * Error Handling:
 * - Bounds checking on all buffer operations
 * - Pointer validation before use
 * - Write failures logged to debug console (not re-enqueued to avoid loops)
 * - Failed writes leave pointer unchanged
 *
 * @note Event codes are 16-bit packed values via ERR_MAKE_CODE macro.
 * @note Ring buffers overwrite oldest data when full (no overflow protection).
 * @note Caller must interpret pointer to distinguish new vs. old entries.
 * @note All operations require eepromMtx held by caller.
 * @note Error logging failures are printed but not re-logged to prevent infinite loops.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include "../../CONFIG.h"

/** @name Bulk Operations
 * @{ */
/**
 * @brief Write entire error event log region to EEPROM (bulk operation).
 *
 * Writes complete error event log buffer including write pointer and all event codes.
 * Used for restoring backed-up event data or factory reset operations. Overwrites
 * entire EEPROM error region in single operation.
 *
 * Buffer Layout Expected:
 * - Offset 0-1: Write pointer (uint16_t, entry index)
 * - Offset 2+:  Error event codes (EVENT_LOG_ENTRY_SIZE bytes each)
 *
 * @param[in] data Pointer to source buffer containing complete error event log data.
 * @param[in] len  Number of bytes to write. Must be <= EEPROM_EVENT_ERR_SIZE.
 *
 * @return 0 if write successful.
 * @return -1 if length exceeds EEPROM_EVENT_ERR_SIZE or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning Overwrites all existing error event data in EEPROM.
 * @note Bounds checking performed before write operation.
 * @note Write failures logged to debug console (not re-enqueued to event log).
 * @note Used primarily for backup restoration or factory reset, not typical operation.
 */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len);

/**
 * @brief Read entire error event log region from EEPROM (bulk operation).
 *
 * Reads complete error event log buffer including write pointer and all event codes.
 * Used for backing up event data, web interface diagnostics page, or SNMP statistics.
 *
 * Buffer Layout Returned:
 * - Offset 0-1: Write pointer (uint16_t, indicates next write position)
 * - Offset 2+:  Error event codes (EVENT_LOG_ENTRY_SIZE bytes each)
 *
 * Interpreting Results:
 * - Write pointer indicates next entry to be written (not last written)
 * - Entries wrap circularly, oldest data overwritten when buffer full
 * - Empty/uninitialized buffer has pointer = 0x0000 or 0xFFFF
 * - Caller must parse entries using pointer to determine order
 * - Event codes decoded using ERR_DECODE_* macros from error_code.h
 *
 * @param[out] data Destination buffer for error event log data.
 * @param[in]  len  Number of bytes to read. Must be <= EEPROM_EVENT_ERR_SIZE.
 *
 * @return 0 if read successful.
 * @return -1 if length exceeds EEPROM_EVENT_ERR_SIZE.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Bounds checking performed before read operation.
 * @note Read failures logged to debug console (not re-enqueued to event log).
 * @note Caller must allocate buffer of at least len bytes.
 * @note Used for web interface, SNMP, console commands, or backup operations.
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len);
/** @} */

/** @name Append APIs
 * @{ */
/**
 * @brief Append one error event code to error ring buffer with automatic wrapping.
 *
 * Primary function for logging critical errors. Implements circular buffer behavior
 * with automatic pointer management and wrap-around. Reads current write pointer,
 * writes new error code, increments pointer (with wrapping), and persists updated
 * pointer to EEPROM.
 *
 * Event Code Format:
 * - Size: EVENT_LOG_ENTRY_SIZE bytes (typically 2 bytes for uint16_t)
 * - Created via ERR_MAKE_CODE(module, severity, function, instance)
 * - Example: ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK, 0x1)
 *
 * Operation Sequence:
 * 1. Read current write pointer from EEPROM (offset 0-1)
 * 2. Validate pointer (clamp to 0 if out of range)
 * 3. Calculate entry address: BASE + 2 + (pointer × EVENT_LOG_ENTRY_SIZE)
 * 4. Write new error code to calculated address
 * 5. Increment pointer: ptr++
 * 6. If pointer at end of buffer: ptr = 0 (wrap to start)
 * 7. Write updated pointer back to EEPROM (offset 0-1)
 *
 * Ring Buffer Behavior:
 * - Buffer size: (EEPROM_EVENT_ERR_SIZE - 2) bytes
 * - Number of entries: (EEPROM_EVENT_ERR_SIZE - 2) / EVENT_LOG_ENTRY_SIZE
 * - When buffer fills, wraps to start and overwrites oldest entry
 * - Pointer always indicates next write position, not last written
 *
 * Typical Error Categories:
 * - I2C communication failures (EEPROM, MCP23017, W5500)
 * - Overcurrent protection trips
 * - EEPROM write failures
 * - Watchdog timeout resets
 * - Ethernet link failures
 * - Fatal configuration errors
 *
 * @param[in] entry Pointer to EVENT_LOG_ENTRY_SIZE bytes containing error code.
 *                  Must point to uint16_t value from ERR_MAKE_CODE().
 *
 * @return 0 if append successful and pointer updated.
 * @return -1 if I2C write fails (entry or pointer write).
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning Buffer must contain exactly EVENT_LOG_ENTRY_SIZE bytes.
 * @note Oldest entries automatically overwritten when buffer wraps.
 * @note Pointer persisted to EEPROM after each append for power-loss safety.
 * @note Write failures logged to debug console (not re-enqueued to prevent loops).
 * @note Called by StorageTask after processing error queue.
 */
int EEPROM_AppendErrorCode(const uint8_t *entry);

/**
 * @brief Append one warning event code to warning ring buffer with automatic wrapping.
 *
 * Primary function for logging non-critical warnings. Implements circular buffer
 * behavior with automatic pointer management and wrap-around. Identical operation to
 * EEPROM_AppendErrorCode() but writes to separate warning buffer region.
 *
 * Event Code Format:
 * - Size: EVENT_LOG_ENTRY_SIZE bytes (typically 2 bytes for uint16_t)
 * - Created via ERR_MAKE_CODE(module, severity, function, instance)
 * - Example: ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_NETWORK, 0x2)
 *
 * Operation Sequence:
 * 1. Read current write pointer from warning buffer start (offset 0-1)
 * 2. Validate pointer (clamp to 0 if out of range)
 * 3. Calculate entry address: BASE + 2 + (pointer × EVENT_LOG_ENTRY_SIZE)
 * 4. Write new warning code to calculated address
 * 5. Increment pointer: ptr++
 * 6. If pointer at end of buffer: ptr = 0 (wrap to start)
 * 7. Write updated pointer back to warning buffer start
 *
 * Ring Buffer Behavior:
 * - Buffer size: (EEPROM_EVENT_WARN_SIZE - 2) bytes
 * - Number of entries: (EEPROM_EVENT_WARN_SIZE - 2) / EVENT_LOG_ENTRY_SIZE
 * - When buffer fills, wraps to start and overwrites oldest entry
 * - Pointer always indicates next write position, not last written
 *
 * Typical Warning Categories:
 * - Temperature threshold crossings (approaching limits)
 * - MAC address repairs (automatic correction applied)
 * - CRC mismatches with successful recovery to defaults
 * - Button long-press events
 * - DHCP timeout with fallback to static IP
 * - Non-fatal configuration inconsistencies
 *
 * @param[in] entry Pointer to EVENT_LOG_ENTRY_SIZE bytes containing warning code.
 *                  Must point to uint16_t value from ERR_MAKE_CODE().
 *
 * @return 0 if append successful and pointer updated.
 * @return -1 if I2C write fails (entry or pointer write).
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning Buffer must contain exactly EVENT_LOG_ENTRY_SIZE bytes.
 * @note Oldest entries automatically overwritten when buffer wraps.
 * @note Pointer persisted to EEPROM after each append for power-loss safety.
 * @note Write failures logged to debug console (not re-enqueued to prevent loops).
 * @note Called by StorageTask after processing warning queue.
 */
int EEPROM_AppendWarningCode(const uint8_t *entry);
/** @} */

#endif /* EVENT_LOG_H */

/** @} */