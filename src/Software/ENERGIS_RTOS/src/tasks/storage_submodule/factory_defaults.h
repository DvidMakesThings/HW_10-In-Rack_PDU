/**
 * @file src/tasks/storage_submodule/factory_defaults.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage06 6. Factory Defaults
 * @ingroup tasks10
 * @brief Factory defaults write/verify and first-boot detection
 * @{
 *
 * @version 2.0.0
 * @date 2025-12-14
 *
 * @details
 * This module handles first-boot detection and initialization of all EEPROM configuration
 * sections with factory default values. It ensures that a newly flashed device or one with
 * erased EEPROM boots successfully with valid configuration data.
 *
 * Key Features:
 * - First-boot detection via magic value verification
 * - Complete EEPROM initialization with default configuration
 * - Post-write validation to verify data integrity
 * - Deferred provisioning for device-specific identity parameters
 *
 * First-Boot Detection:
 * - Reads magic value from EEPROM_MAGIC_ADDR
 * - Magic value EEPROM_MAGIC_VAL indicates initialized EEPROM
 * - Missing or mismatched magic triggers factory default write
 * - Magic written only after successful initialization
 * - Prevents repeated initialization on subsequent boots
 *
 * Configuration Sections Written:
 * 1. System Info: Firmware version string (SWVERSION)
 * 2. Relay Status: All channels OFF (safe default state)
 * 3. Network Config: Default IP/gateway/DNS with derived MAC address
 * 4. Sensor Calibration: Default HLW8032 voltage/current factors per channel
 * 5. Temperature Calibration: Default 1-point calibration (0.706V@27°C, 0.001721V/°C)
 * 6. Energy Monitoring: Placeholder zeros (no historical data)
 * 7. Event Logs: Placeholder zeros (no historical events)
 * 8. User Preferences: Default device name and location with CRC
 *
 * Device Identity Management:
 * - Serial number and region NOT written by factory defaults
 * - DeviceIdentity_Init() called to load existing provisioning if present
 * - Unprovisioned devices use placeholder "UNPROVISIONED" serial
 * - MAC address derived from current serial (placeholder or provisioned)
 * - After UART provisioning, MAC automatically updated on next boot
 *
 * Calibration Initialization:
 * - HLW8032 per-channel: Default voltage/current factors, zero offsets
 * - Calibration flags set to 0xFF (not calibrated)
 * - Temperature: Default 1-point linear model (RP2040 internal sensor)
 * - Actual calibration performed later via UART commands
 *
 * Thread Safety:
 * - EEPROM_WriteFactoryDefaults() requires eepromMtx held by caller
 * - check_factory_defaults() manages mutex internally for read/write
 * - Single-threaded initialization during InitTask prevents races
 *
 * Boot Sequence Integration:
 * - Called early in InitTask before other subsystems initialize
 * - Ensures valid configuration exists before NetTask/MeterTask start
 * - DeviceIdentity loaded before network MAC generation
 * - Validation performed before transitioning to normal operation
 *
 * Version History:
 * - v1.x: Wrote compile-time SERIAL_NUMBER macro to device identity
 * - v2.0: Serial number deferred to UART provisioning commands
 *
 * @note Device identity (serial/region) requires UART provisioning after first boot.
 * @note Factory defaults safe for immediate power-on but not fully functional.
 * @note Magic value write is atomic checkpoint - failure leaves EEPROM incomplete.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef FACTORY_DEFAULTS_H
#define FACTORY_DEFAULTS_H

#include "../../CONFIG.h"

/** @name Public API
 * @{ */
/**
 * @brief Check if factory defaults need to be written (first boot detection).
 *
 * Performs first-boot detection by reading the magic value from EEPROM_MAGIC_ADDR.
 * If the magic value does not match EEPROM_MAGIC_VAL, this is the first boot and
 * complete factory default configuration is written to all EEPROM sections. Also
 * initializes the DeviceIdentity module to load any existing provisioning data.
 *
 * Initialization Sequence:
 * 1. Call DeviceIdentity_Init() to load serial/region if present
 * 2. Read magic value with mutex protection
 * 3. If magic mismatch (first boot):
 *    a. Write all factory default sections with mutex held
 *    b. Write temperature calibration defaults
 *    c. Write per-channel HLW8032 calibration defaults
 *    d. Write magic value to mark initialization complete
 * 4. If magic matches, EEPROM already initialized
 *
 * Factory Default Sections:
 * - System info (firmware version)
 * - Relay status (all OFF)
 * - Network config (default IP with MAC derived from serial)
 * - Sensor calibration (default factors for all 8 channels)
 * - Temperature calibration (1-point linear model)
 * - Energy monitoring (placeholder zeros)
 * - Event logs (placeholder zeros)
 * - User preferences (default name/location)
 *
 * MAC Address Handling:
 * - MAC derived from DeviceIdentity (placeholder or provisioned serial)
 * - Unprovisioned devices get MAC from "UNPROVISIONED" placeholder
 * - After UART provisioning, MAC regenerated from new serial on next boot
 *
 * @return true if defaults were successfully written OR already present.
 * @return false if factory default write operation failed.
 *
 * @note Manages eepromMtx internally for thread-safe EEPROM access.
 * @note Called during InitTask before other subsystems start.
 * @note Magic value written last as atomic checkpoint of successful init.
 * @note Failure leaves EEPROM in incomplete state requiring reflash.
 */
bool check_factory_defaults(void);

/**
 * @brief Write factory defaults to all EEPROM sections.
 *
 * Writes default configuration values to all EEPROM sections in sequence. Does NOT
 * write device identity (serial number, region) which must be provisioned separately
 * via UART commands. Uses mutex protection provided by caller to ensure atomic writes.
 *
 * Configuration Sections Written:
 * 1. System Info (EEPROM_SYS_INFO_START):
 *    - Firmware version string (SWVERSION)
 *    - Serial number NOT written (provisioned separately)
 *
 * 2. Relay Status (via EEPROM_WriteUserOutput):
 *    - All 8 channels set to OFF (safe default)
 *    - Prevents unexpected power-on of connected equipment
 *
 * 3. Network Configuration (via EEPROM_WriteUserNetworkWithChecksum):
 *    - Default IP address, subnet, gateway, DNS from DEFAULT_NETWORK
 *    - MAC address derived from DeviceIdentity (placeholder or provisioned)
 *    - CRC-8 checksum calculated and stored for validation
 *
 * 4. Sensor Calibration (via EEPROM_WriteSensorCalibration):
 *    - Default voltage factor (HLW8032_VF) for all 8 channels
 *    - Default current factor (HLW8032_CF) for all 8 channels
 *    - Default resistor divider values (R1=1880kΩ, R2=1kΩ)
 *    - Default shunt resistance (0.002Ω)
 *    - Zero offsets (0.0V, 0.0A)
 *    - Calibration flags set to 0xFF (not calibrated)
 *
 * 5. Energy Monitoring (via EEPROM_WriteEnergyMonitoring):
 *    - Placeholder zeros (no historical energy data)
 *
 * 6. Event Logs (via EEPROM_WriteEventLogs):
 *    - Placeholder zeros (no historical events)
 *
 * 7. User Preferences (via EEPROM_WriteDefaultNameLocation):
 *    - Default device name and location strings
 *    - CRC-8 checksum for validation
 *
 * Error Handling:
 * - Each write operation returns status code
 * - Status codes ORed together to detect any failures
 * - Failed writes logged via ERROR_PRINT_CODE
 * - Error codes enqueued to event log if enabled
 *
 * @return 0 if all sections written successfully.
 * @return -1 if any section write failed (bitwise OR of all failures).
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning Does NOT write magic value; caller responsible for checkpoint.
 * @note Device identity (serial/region) requires UART provisioning after init.
 * @note All channels default to OFF for safety.
 */
int EEPROM_WriteFactoryDefaults(void);

/**
 * @brief Perform basic read-back validation after factory defaulting.
 *
 * Validates that factory defaults were written correctly to EEPROM by reading back
 * and checking critical configuration sections. Detects write failures, corruption,
 * or incomplete initialization. Does not modify EEPROM; read-only verification.
 *
 * Validation Checks:
 * 1. Firmware Version:
 *    - Read from EEPROM_SYS_INFO_START
 *    - Compare with compile-time SWVERSION string
 *    - Mismatch logged as ERR_SEV_WARNING
 *
 * 2. Device Identity:
 *    - Call DeviceIdentity_IsValid() to check provisioning state
 *    - Informational message if not provisioned (expected after factory init)
 *    - No error raised since provisioning is deferred to UART commands
 *
 * 3. Network Configuration:
 *    - Read via EEPROM_ReadUserNetworkWithChecksum()
 *    - Verify CRC-8 checksum matches stored value
 *    - CRC mismatch logged as ERR_SEV_WARNING
 *
 * 4. Sensor Calibration:
 *    - Read calibration data structure from EEPROM
 *    - Verify read operation succeeds
 *    - Read failure logged as ERR_SEV_WARNING
 *
 * Error Reporting:
 * - All validation failures logged with unique error codes
 * - Error codes enqueued to event log if ERRORLOGGER enabled
 * - Function completes all checks even if some fail
 * - Always returns 0 (informational validation)
 *
 * @return 0 always (validation results logged, not returned).
 *
 * @note Does not write to EEPROM; read-only verification.
 * @note Called immediately after EEPROM_WriteFactoryDefaults() by check_factory_defaults().
 * @note Device identity not provisioned is expected and not treated as error.
 * @note Validation warnings indicate potential EEPROM write issues.
 */
int EEPROM_ReadFactoryDefaults(void);
/** @} */

#endif /* FACTORY_DEFAULTS_H */

/** @} */