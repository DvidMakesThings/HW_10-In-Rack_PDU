/**
 * @file src/tasks/storage_submodule/factory_defaults.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage02 2. Factory Defaults
 * @ingroup storage
 * @brief Factory defaults write/verify and first-boot detection
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Handles factory default initialization on first boot, writing all default
 * configuration values to EEPROM, and validating written data. Uses magic value to
 * detect virgin/uninitialized EEPROM.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef FACTORY_DEFAULTS_H
#define FACTORY_DEFAULTS_H

#include "../../CONFIG.h"

/**
 * @brief Check if factory defaults need to be written (first boot detection).
 *
 * Reads magic value from EEPROM. If not found, writes complete factory defaults
 * and sets magic value to mark initialization complete.
 *
 * @warning Must be called with eepromMtx held by caller during write operations.
 * @return true if defaults were written or already present, false on write failure
 */
bool check_factory_defaults(void);

/**
 * @brief Write factory defaults to all EEPROM sections.
 *
 * Writes default values for:
 * - System info (serial number, software version)
 * - Relay states (all OFF)
 * - Network configuration (with derived MAC)
 * - Sensor calibration (default factors)
 * - Energy monitoring (placeholder)
 * - Event logs (placeholder)
 * - User preferences (default name/location)
 *
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 if any section write fails
 */
int EEPROM_WriteFactoryDefaults(void);

/**
 * @brief Perform basic read-back checks after factory defaulting.
 *
 * Validates that factory defaults were written correctly by reading back
 * and checking critical sections (serial number, network config, sensor cal).
 *
 * @return 0 on success, -1 on validation failure
 */
int EEPROM_ReadFactoryDefaults(void);

#endif /* FACTORY_DEFAULTS_H */

/** @} */