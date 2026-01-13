/**
 * @file src/tasks/storage_submodule/storage_common.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage08 8. Storage Common
 * @ingroup tasks10
 * @brief Shared utility functions for EEPROM data integrity and MAC address management.
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details
 * This module provides common utility functions used across all storage submodules
 * for data integrity verification and network hardware addressing. It serves as the
 * foundation layer for EEPROM configuration management.
 *
 * Key Features:
 * - CRC-8 checksum calculation for data integrity verification
 * - MAC address derivation from device serial number via FNV-1a hash
 * - MAC address validation and automatic repair for corrupted values
 * - Integration with DeviceIdentity module for provisioned device support
 *
 * CRC-8 Algorithm:
 * - Polynomial: 0x07
 * - Initial value: 0x00
 * - Used by: network config, user preferences, device identity
 * - Detects single-bit errors and most multi-bit errors
 * - Fast computation suitable for embedded systems
 *
 * MAC Address Management:
 * - Format: ENERGIS_MAC_PREFIX (3 bytes) + FNV-1a hash (3 bytes)
 * - Prefix: Fixed vendor-specific OUI for ENERGIS devices
 * - Suffix: Derived from device serial number for uniqueness
 * - Hash algorithm: FNV-1a provides good distribution
 * - Validation: Detects corrupted prefix or invalid suffix patterns
 * - Auto-repair: Regenerates MAC from serial when corruption detected
 *
 * Provisioning Integration:
 * - Unprovisioned devices use placeholder serial number
 * - After provisioning, MAC automatically updated to match new serial
 * - Energis_RepairMac() enforces serial-MAC consistency
 * - Ensures unique MAC addresses across production devices
 *
 * Usage Pattern:
 * 1. calculate_crc8() called by submodules before EEPROM writes
 * 2. CRC verified on reads to detect corruption
 * 3. Energis_FillMacFromSerial() populates MAC in network config
 * 4. Energis_RepairMac() called on boot to fix any corruption
 *
 * Integration Points:
 * - DeviceIdentity: Provides serial number and provisioning status
 * - Network submodule: Uses MAC functions for Ethernet configuration
 * - All storage submodules: Use CRC-8 for data integrity
 *
 * @note CRC-8 provides error detection but not correction; corrupted data must be reloaded.
 * @note MAC address uniqueness depends on unique device serial numbers.
 * @note Energis_RepairMac() modifies networkInfo structure in place.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STORAGE_COMMON_H
#define STORAGE_COMMON_H

#include "../../CONFIG.h"

/** @name Checksum Utilities
 * @{ */
/**
 * @brief Calculate CRC-8 checksum for data integrity verification.
 *
 * Computes CRC-8 checksum using polynomial 0x07 with initial value 0x00.
 * Used by storage submodules to detect data corruption in EEPROM. The CRC
 * is typically stored alongside the data and verified on read operations.
 *
 * Algorithm Properties:
 * - Detects all single-bit errors
 * - Detects most multi-bit errors
 * - Fast computation suitable for embedded systems
 * - 8-bit output suitable for constrained EEPROM space
 *
 * @param[in] data Pointer to input buffer to calculate CRC over.
 * @param[in] len  Length of input buffer in bytes.
 *
 * @return Calculated 8-bit CRC checksum value.
 *
 * @note Returns 0x00 for zero-length input.
 * @note Caller must ensure data pointer is valid for len bytes.
 * @note Not cryptographically secure; used only for error detection.
 */
uint8_t calculate_crc8(const uint8_t *data, size_t len);
/** @} */

/** @name MAC Address Utilities
 * @{ */
/**
 * @brief Generate MAC address from device serial number.
 *
 * Populates MAC address buffer with ENERGIS vendor prefix and serial-derived
 * suffix. Delegates to DeviceIdentity_FillMac() which uses FNV-1a hash of the
 * device serial number to generate the last 3 bytes.
 *
 * MAC Address Format:
 * - Bytes 0-2: ENERGIS_MAC_PREFIX (vendor OUI)
 * - Bytes 3-5: FNV-1a hash of serial number (uniqueness)
 *
 * @param[out] mac 6-byte buffer to receive generated MAC address.
 *
 * @note Requires DeviceIdentity module to be initialized.
 * @note Unprovisioned devices use placeholder serial for hash.
 * @note MAC changes after device provisioning with new serial number.
 * @note Caller must provide buffer with at least 6 bytes capacity.
 */
void Energis_FillMacFromSerial(uint8_t mac[6]);

/**
 * @brief Validate and repair corrupted MAC address.
 *
 * Checks MAC address for common corruption patterns and repairs if invalid.
 * Detects wrong vendor prefix, all-zero suffix, all-0xFF suffix, and
 * serial-MAC mismatch on provisioned devices. Repairs by regenerating MAC
 * from current device serial number.
 *
 * Validation Checks:
 * 1. Prefix matches ENERGIS_MAC_PREFIX (vendor OUI)
 * 2. Suffix not all zeros (invalid EEPROM initialization)
 * 3. Suffix not all 0xFF (erased EEPROM pattern)
 * 4. MAC matches serial-derived value (post-provisioning enforcement)
 *
 * Repair Action:
 * - Calls DeviceIdentity_FillMac() to regenerate from current serial
 * - Overwrites corrupted MAC in networkInfo structure
 * - Ensures consistency between serial number and MAC address
 *
 * @param[in,out] n Pointer to networkInfo structure containing MAC to validate/repair.
 *
 * @return true if MAC was repaired (corruption detected).
 * @return false if MAC was already valid (no repair needed).
 *
 * @note Modifies networkInfo structure in place if repair needed.
 * @note Caller should commit repaired config to EEPROM after return.
 * @note Post-provisioning, enforces MAC matches new serial number.
 */
bool Energis_RepairMac(networkInfo *n);
/** @} */

#endif /* STORAGE_COMMON_H */

/** @} */