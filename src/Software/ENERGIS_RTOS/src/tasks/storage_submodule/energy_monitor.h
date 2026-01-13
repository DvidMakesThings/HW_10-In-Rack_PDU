/**
 * @file src/tasks/storage_submodule/energy_monitor.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage04 4. Energy Monitoring
 * @ingroup tasks10
 * @brief Energy consumption logging with ring buffer persistence
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details
 * This module manages historical energy consumption data stored in EEPROM using a
 * ring buffer structure. Provides periodic logging of energy measurements for
 * trend analysis and capacity planning.
 *
 * Key Features:
 * - Ring buffer for energy consumption records
 * - Automatic pointer wrapping for circular storage
 * - Fixed-size records for predictable memory usage
 * - Write pointer persistence across power cycles
 * - Support for bulk read/write operations
 *
 * EEPROM Memory Layout (EEPROM_ENERGY_MON_START, total size varies):
 * - Offset 0-1: Write pointer (uint16_t, entry index)
 * - Offset 2+:  Energy records (ENERGY_RECORD_SIZE bytes each)
 *
 * Ring Buffer Structure:
 * - First 2 bytes (ENERGY_MON_POINTER_SIZE): Write pointer
 *   * Stores index of next record to write
 *   * Automatically wraps to 0 when buffer fills
 *   * Persisted to EEPROM after each append
 *
 * - Remaining bytes: Energy record array
 *   * Each record: ENERGY_RECORD_SIZE bytes
 *   * Number of records: (EEPROM_ENERGY_MON_SIZE - 2) / ENERGY_RECORD_SIZE
 *   * Older records overwritten when buffer wraps
 *
 * Energy Record Format:
 * - Size: ENERGY_RECORD_SIZE bytes (defined in config.h)
 * - Contents: Application-specific energy data structure
 * - Typically includes: timestamp, channel ID, energy counter, power metrics
 * - Record structure defined by MeterTask
 *
 * Append Operation:
 * 1. Read current write pointer from EEPROM
 * 2. Calculate address for new record: BASE + 2 + (ptr × ENERGY_RECORD_SIZE)
 * 3. Write new energy record to calculated address
 * 4. Increment pointer (wrap to 0 if at end of buffer)
 * 5. Write updated pointer back to EEPROM
 * 6. Oldest record automatically overwritten on wrap
 *
 * Bulk Operations:
 * - EEPROM_WriteEnergyMonitoring(): Bulk write entire region (for restoration)
 * - EEPROM_ReadEnergyMonitoring(): Bulk read entire region (for backup/analysis)
 * - Used for firmware updates, factory reset, or data extraction
 *
 * Thread Safety:
 * - All read/write functions require eepromMtx held by caller
 * - Prevents concurrent access corruption
 * - Ensures atomic read-modify-write for pointer updates
 *
 * Integration Points:
 * - MeterTask: Periodically appends energy records (e.g., hourly)
 * - Web interface: Retrieves historical data for charts
 * - SNMP: Provides energy statistics via custom OIDs
 * - Storage Task: Provides eepromMtx for thread-safe access
 *
 * Usage Pattern:
 * 1. MeterTask accumulates energy measurements
 * 2. On logging interval (e.g., every hour):
 *    a. Pack measurements into energy record structure
 *    b. Call EEPROM_AppendEnergyRecord() to persist
 * 3. Web interface or SNMP reads historical records:
 *    a. Call EEPROM_ReadEnergyMonitoring() to bulk read
 *    b. Parse records using write pointer to determine newest/oldest
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
 * - Write failures logged as ERR_SEV_ERROR
 * - Failed writes leave pointer unchanged
 * - Error codes enqueued to event log
 *
 * @note Energy record format is application-defined via ENERGY_RECORD_SIZE.
 * @note Ring buffer overwrites oldest data when full (no overflow protection).
 * @note Caller must interpret pointer to distinguish new vs. old records.
 * @note All operations require eepromMtx held by caller.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ENERGY_MONITOR_H
#define ENERGY_MONITOR_H

#include "../../CONFIG.h"

/** @name Bulk Operations
 * @{ */
/**
 * @brief Write entire energy monitoring region to EEPROM (bulk operation).
 *
 * Writes complete energy monitoring buffer including write pointer and all records.
 * Used for restoring backed-up energy data or factory reset operations. Overwrites
 * entire EEPROM region in single operation.
 *
 * Buffer Layout Expected:
 * - Offset 0-1: Write pointer (uint16_t)
 * - Offset 2+:  Energy records (ENERGY_RECORD_SIZE bytes each)
 *
 * @param[in] data Pointer to source buffer containing complete energy monitoring data.
 * @param[in] len  Number of bytes to write. Must be <= EEPROM_ENERGY_MON_SIZE.
 *
 * @return 0 if write successful.
 * @return -1 if length exceeds EEPROM_ENERGY_MON_SIZE or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning Overwrites all existing energy data in EEPROM.
 * @note Bounds checking performed before write operation.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Used primarily for backup restoration, not typical operation.
 */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len);

/**
 * @brief Read entire energy monitoring region from EEPROM (bulk operation).
 *
 * Reads complete energy monitoring buffer including write pointer and all records.
 * Used for backing up energy data, web interface display, or SNMP statistics.
 *
 * Buffer Layout Returned:
 * - Offset 0-1: Write pointer (uint16_t, indicates next write position)
 * - Offset 2+:  Energy records (ENERGY_RECORD_SIZE bytes each)
 *
 * Interpreting Results:
 * - Write pointer indicates next record to be written (not last written)
 * - Records wrap circularly, oldest data overwritten when buffer full
 * - Empty/uninitialized buffer has pointer = 0x0000 or 0xFFFF
 * - Caller must parse records using pointer to determine order
 *
 * @param[out] data Destination buffer for energy monitoring data.
 * @param[in]  len  Number of bytes to read. Must be <= EEPROM_ENERGY_MON_SIZE.
 *
 * @return 0 if read successful.
 * @return -1 if length exceeds EEPROM_ENERGY_MON_SIZE.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Bounds checking performed before read operation.
 * @note Read failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Caller must allocate buffer of at least len bytes.
 * @note Used for web interface, SNMP, or backup operations.
 */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len);
/** @} */

/** @name Append API
 * @{ */
/**
 * @brief Append one energy record to ring buffer with automatic wrapping.
 *
 * Primary function for logging periodic energy measurements. Implements circular
 * buffer behavior with automatic pointer management and wrap-around. Reads current
 * write pointer, writes new record, increments pointer (with wrapping), and persists
 * updated pointer to EEPROM.
 *
 * Operation Sequence:
 * 1. Read current write pointer from EEPROM (offset 0-1)
 * 2. Calculate record address: BASE + 2 + (pointer × ENERGY_RECORD_SIZE)
 * 3. Write new energy record to calculated address
 * 4. Increment pointer: ptr++
 * 5. If pointer at end of buffer: ptr = 0 (wrap to start)
 * 6. Write updated pointer back to EEPROM (offset 0-1)
 *
 * Ring Buffer Behavior:
 * - Buffer size: (EEPROM_ENERGY_MON_SIZE - 2) bytes
 * - Number of records: (EEPROM_ENERGY_MON_SIZE - 2) / ENERGY_RECORD_SIZE
 * - When buffer fills, wraps to start and overwrites oldest record
 * - Pointer always indicates next write position, not last written
 *
 * Pointer Validation:
 * - Out-of-range pointers clamped to 0 (guards against corruption)
 * - Max valid pointer: (buffer_size / ENERGY_RECORD_SIZE) - 1
 * - Corrupted pointer triggers error log but operation continues
 *
 * @param[in] data Pointer to energy record buffer (ENERGY_RECORD_SIZE bytes).
 *                 Must contain complete energy record structure.
 *
 * @return 0 if append successful and pointer updated.
 * @return -1 if I2C write fails (record or pointer write).
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning Buffer must contain exactly ENERGY_RECORD_SIZE bytes.
 * @note Oldest records automatically overwritten when buffer wraps.
 * @note Pointer persisted to EEPROM after each append for power-loss safety.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Called periodically by MeterTask (e.g., hourly energy snapshots).
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data);
/** @} */

#endif /* ENERGY_MONITOR_H */

/** @} */