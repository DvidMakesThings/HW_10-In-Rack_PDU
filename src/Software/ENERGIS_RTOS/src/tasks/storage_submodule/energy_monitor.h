/**
 * @file src/tasks/storage_submodule/energy_monitor.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage06 6. Energy Monitoring
 * @ingroup storage
 * @brief Energy consumption logging with ring buffer
 * @{
 * 
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Manages energy monitoring data in EEPROM as a ring buffer.
 * Stores periodic energy consumption records for historical tracking.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ENERGY_MONITOR_H
#define ENERGY_MONITOR_H

#include "../../CONFIG.h"

/**
 * @brief Write the energy monitoring region.
 * @param data Pointer to source buffer
 * @param len Must be <= EEPROM_ENERGY_MON_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len);

/**
 * @brief Read the energy monitoring region.
 * @param data Destination buffer
 * @param len Must be <= EEPROM_ENERGY_MON_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len);

/**
 * @brief Append a fixed-size energy record to the ring buffer and update pointer.
 *
 * Implements circular buffer behavior:
 * 1. Reads current write pointer
 * 2. Writes new record at pointer location
 * 3. Increments pointer (wraps to 0 if at end)
 * 4. Updates pointer in EEPROM
 *
 * @param data Pointer to one energy record of size ENERGY_RECORD_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on write error
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data);

#endif /* ENERGY_MONITOR_H */

/** @} */