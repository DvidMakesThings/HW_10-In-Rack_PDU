/**
 * @file src/tasks/storage_submodule/network.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage07 7. Network Configuration
 * @ingroup tasks10
 * @brief Network settings with CRC validation and MAC repair
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details
 * This module manages network configuration parameters stored in EEPROM with CRC-8
 * data integrity protection and automatic MAC address repair. It handles both
 * system information (firmware version) and network settings (IP, MAC, gateway,
 * DNS, DHCP) with separate EEPROM address spaces.
 *
 * Key Features:
 * - Network configuration persistence with CRC-8 validation
 * - Automatic MAC address repair for corrupted values
 * - System information storage with integrity checking
 * - Fallback to defaults on read failures
 * - Thread-safe access with mutex protection
 *
 * EEPROM Memory Layout:
 * 1. System Info Section (EEPROM_SYS_INFO_START):
 *    - Size: EEPROM_SYS_INFO_SIZE bytes
 *    - Contents: Firmware version string (SWVERSION)
 *    - Optional CRC-8 appended for validation
 *
 * 2. Network Config Section (EEPROM_USER_NETWORK_START):
 *    - Size: 24 bytes (EEPROM_USER_NETWORK_SIZE)
 *    - Layout: MAC(6) + IP(4) + Subnet(4) + Gateway(4) + DNS(4) + DHCP(1) + CRC(1)
 *    - CRC-8 calculated over first 23 bytes, stored in byte 24
 *
 * Network Configuration Structure:
 * - MAC Address: 6-byte hardware address (derived from serial number)
 * - IP Address: 4-byte IPv4 address (default or DHCP-assigned)
 * - Subnet Mask: 4-byte network mask
 * - Gateway: 4-byte default gateway address
 * - DNS Server: 4-byte domain name server address
 * - DHCP Enable: 1-byte flag (0=static, 1=DHCP)
 * - CRC-8: 1-byte checksum for integrity verification
 *
 * CRC-8 Validation:
 * - Polynomial: 0x07 (via calculate_crc8())
 * - Calculated over all data bytes before CRC field
 * - Read operations fail on CRC mismatch
 * - Failed reads trigger fallback to defaults
 * - Successful reads trigger MAC repair check
 *
 * MAC Address Repair:
 * - Automatic detection of corrupted MAC values
 * - Checks prefix matches ENERGIS_MAC_PREFIX
 * - Detects invalid suffix patterns (all zeros, all 0xFF)
 * - Enforces serial-derived MAC on provisioned devices
 * - Repaired MAC persisted immediately to EEPROM
 * - Repair events logged as ERR_SEV_WARNING
 *
 * Load Operation Flow:
 * 1. LoadUserNetworkConfig() reads from EEPROM with CRC validation
 * 2. If CRC valid:
 *    a. Check MAC address with Energis_RepairMac()
 *    b. If MAC corrupted, regenerate from serial and persist
 *    c. Return validated/repaired configuration
 * 3. If CRC invalid or EEPROM empty:
 *    a. Load DEFAULT_NETWORK structure
 *    b. Generate MAC from DeviceIdentity serial
 *    c. Persist defaults to EEPROM
 *    d. Return default configuration
 *
 * Thread Safety:
 * - All read/write functions require eepromMtx held by caller
 * - LoadUserNetworkConfig() manages mutex internally
 * - Prevents concurrent access corruption
 * - Ensures atomic read-modify-write for MAC repair
 *
 * Integration Points:
 * - DeviceIdentity: Provides serial number for MAC generation
 * - storage_common: CRC-8 calculation and MAC repair functions
 * - StorageTask: Provides DEFAULT_NETWORK and eepromMtx
 * - NetTask: Consumes loaded network configuration
 * - W5500 Ethernet: MAC address programming
 *
 * Error Handling:
 * - Bounds checking on all buffer operations
 * - CRC mismatch logged as ERR_SEV_ERROR
 * - MAC repair logged as ERR_SEV_WARNING
 * - Failed reads fall back to safe defaults
 * - Error codes enqueued to event log
 *
 * API Layers:
 * - Raw APIs: Direct EEPROM access without CRC (for legacy/debug)
 * - CRC APIs: Recommended for production use with integrity checking
 * - Load API: High-level with defaults and automatic repair
 *
 * @note Always prefer CRC-verified APIs for production code.
 * @note LoadUserNetworkConfig() is the recommended high-level interface.
 * @note Raw APIs provided for legacy compatibility and debug access.
 * @note MAC address uniqueness depends on unique device serial numbers.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "../../CONFIG.h"

/** @name Raw Network Config APIs
 * @{ */
/**
 * @brief Write raw network configuration block without CRC validation.
 *
 * Low-level write operation that directly writes buffer to EEPROM network configuration
 * section without CRC generation. Provided for legacy compatibility and debug access.
 * Production code should use EEPROM_WriteUserNetworkWithChecksum() instead.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_USER_NETWORK_START
 * - Maximum size: EEPROM_USER_NETWORK_SIZE bytes
 * - No CRC appended or validated
 *
 * @param[in] data Pointer to source buffer containing network configuration.
 * @param[in] len  Number of bytes to write. Must be <= EEPROM_USER_NETWORK_SIZE.
 *
 * @return 0 if write successful.
 * @return -1 if length exceeds EEPROM_USER_NETWORK_SIZE or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning No CRC generation or validation performed.
 * @warning Prefer EEPROM_WriteUserNetworkWithChecksum() for production code.
 * @note Bounds checking performed before write operation.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len);

/**
 * @brief Read raw network configuration block without CRC validation.
 *
 * Low-level read operation that directly reads buffer from EEPROM network configuration
 * section without CRC verification. Provided for legacy compatibility and debug access.
 * Production code should use EEPROM_ReadUserNetworkWithChecksum() instead.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_USER_NETWORK_START
 * - Maximum size: EEPROM_USER_NETWORK_SIZE bytes
 * - No CRC verified
 *
 * @param[out] data Destination buffer for network configuration.
 * @param[in]  len  Number of bytes to read. Must be <= EEPROM_USER_NETWORK_SIZE.
 *
 * @return 0 if read successful.
 * @return -1 if length exceeds EEPROM_USER_NETWORK_SIZE.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @warning No CRC verification performed on read data.
 * @warning Prefer EEPROM_ReadUserNetworkWithChecksum() for production code.
 * @note Bounds checking performed before read operation.
 * @note Read failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Caller responsible for validating data integrity.
 */
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len);
/** @} */

/** @name CRC Network Config APIs
 * @{ */
/**
 * @brief Write network configuration with CRC-8 validation appended.
 *
 * Recommended write operation for production code. Packs networkInfo structure into
 * 24-byte EEPROM layout with CRC-8 checksum appended for integrity verification.
 * CRC calculated over first 23 bytes and stored in byte 24.
 *
 * EEPROM Layout (24 bytes total):
 * - Offset 0-5:   MAC address (6 bytes)
 * - Offset 6-9:   IP address (4 bytes)
 * - Offset 10-13: Subnet mask (4 bytes)
 * - Offset 14-17: Gateway address (4 bytes)
 * - Offset 18-21: DNS server address (4 bytes)
 * - Offset 22:    DHCP enable flag (1 byte, 0=static 1=DHCP)
 * - Offset 23:    CRC-8 checksum (1 byte)
 *
 * CRC Calculation:
 * - Algorithm: CRC-8 polynomial 0x07 via calculate_crc8()
 * - Input: Bytes 0-22 (MAC through DHCP flag)
 * - Output: Stored in byte 23
 *
 * @param[in] net_info Pointer to networkInfo structure containing configuration.
 *
 * @return 0 if write successful.
 * @return -1 if net_info is NULL or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Null pointer check performed before write operation.
 * @note CRC automatically calculated and appended.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note This is the recommended API for production network config writes.
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info);

/**
 * @brief Read and verify network configuration with CRC-8 validation.
 *
 * Recommended read operation for production code. Reads 24-byte EEPROM network
 * configuration and verifies CRC-8 checksum. Unpacks buffer into networkInfo
 * structure only if CRC validation passes.
 *
 * EEPROM Layout (24 bytes total):
 * - Offset 0-5:   MAC address (6 bytes)
 * - Offset 6-9:   IP address (4 bytes)
 * - Offset 10-13: Subnet mask (4 bytes)
 * - Offset 14-17: Gateway address (4 bytes)
 * - Offset 18-21: DNS server address (4 bytes)
 * - Offset 22:    DHCP enable flag (1 byte, 0=static 1=DHCP)
 * - Offset 23:    CRC-8 checksum (1 byte)
 *
 * CRC Validation:
 * - Calculate CRC-8 over bytes 0-22
 * - Compare with stored CRC in byte 23
 * - Fail if mismatch detected
 *
 * @param[out] net_info Pointer to networkInfo structure to receive configuration.
 *
 * @return 0 if CRC validation passed and structure populated.
 * @return -1 if net_info is NULL or CRC mismatch detected.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Null pointer check performed before read operation.
 * @note CRC mismatch logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Caller should use LoadUserNetworkConfig() for automatic fallback to defaults.
 * @note This is the recommended API for production network config reads.
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info);
/** @} */

/** @name Load API
 * @{ */
/**
 * @brief Load network configuration from EEPROM with automatic fallback and MAC repair.
 *
 * High-level interface for loading network configuration. Attempts EEPROM read with CRC
 * validation. On success, performs automatic MAC address repair if needed. On failure,
 * returns default configuration with MAC derived from device serial number. Manages
 * eepromMtx internally for thread-safe operation.
 *
 * Operation Flow:
 * 1. Acquire eepromMtx for thread-safe access
 * 2. Call EEPROM_ReadUserNetworkWithChecksum() to read with CRC validation
 * 3. If CRC valid:
 *    a. Call Energis_RepairMac() to check MAC integrity
 *    b. If MAC repaired, persist corrected config to EEPROM
 *    c. Log repair event as ERR_SEV_WARNING
 *    d. Return validated/repaired configuration
 * 4. If CRC invalid or EEPROM empty:
 *    a. Load DEFAULT_NETWORK structure from StorageTask
 *    b. Call Energis_FillMacFromSerial() to generate MAC from serial
 *    c. Persist defaults to EEPROM for future boots
 *    d. Log fallback event as ERR_SEV_WARNING
 *    e. Return default configuration
 * 5. Release eepromMtx
 *
 * MAC Repair Conditions:
 * - Wrong vendor prefix (not ENERGIS_MAC_PREFIX)
 * - Invalid suffix patterns (all zeros or all 0xFF)
 * - MAC mismatch on provisioned devices (enforces serial-derived MAC)
 *
 * Default Configuration:
 * - Network settings from DEFAULT_NETWORK constant
 * - MAC address generated from DeviceIdentity serial number
 * - Safe fallback for first boot or EEPROM corruption
 *
 * @return networkInfo structure containing valid network configuration.
 *         Always returns valid data, never fails.
 *
 * @note This is the recommended high-level API for loading network config.
 * @note Manages eepromMtx internally; caller should NOT hold mutex.
 * @note Always returns valid configuration via fallback to defaults.
 * @note Repaired/default configs automatically persisted to EEPROM.
 * @note Called by NetTask during initialization.
 */
networkInfo LoadUserNetworkConfig(void);
/** @} */

/** @name System Info APIs (Raw/CRC)
 * @{ */
/**
 * @brief Write system information block without CRC validation.
 *
 * Low-level write operation for system information section. Writes raw buffer
 * containing firmware version string without CRC generation. Used primarily
 * during factory defaults initialization.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_SYS_INFO_START
 * - Maximum size: EEPROM_SYS_INFO_SIZE bytes
 * - Contents: Firmware version string (SWVERSION)
 * - No CRC appended
 *
 * @param[in] data Pointer to source buffer containing system information.
 * @param[in] len  Number of bytes to write. Must be <= EEPROM_SYS_INFO_SIZE.
 *
 * @return 0 if write successful.
 * @return -1 if length exceeds EEPROM_SYS_INFO_SIZE or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Bounds checking performed before write operation.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Primarily used by factory defaults initialization.
 */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len);

/**
 * @brief Read system information block without CRC validation.
 *
 * Low-level read operation for system information section. Reads raw buffer
 * containing firmware version string without CRC verification.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_SYS_INFO_START
 * - Maximum size: EEPROM_SYS_INFO_SIZE bytes
 * - Contents: Firmware version string (SWVERSION)
 * - No CRC verified
 *
 * @param[out] data Destination buffer for system information.
 * @param[in]  len  Number of bytes to read. Must be <= EEPROM_SYS_INFO_SIZE.
 *
 * @return 0 if read successful.
 * @return -1 if length exceeds EEPROM_SYS_INFO_SIZE.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Bounds checking performed before read operation.
 * @note Read failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Caller responsible for validating data integrity.
 */
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len);

/**
 * @brief Write system information with CRC-8 validation appended.
 *
 * Writes system information buffer with CRC-8 checksum appended for integrity
 * verification. Reserves 1 byte for CRC, limiting data to EEPROM_SYS_INFO_SIZE-1.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_SYS_INFO_START
 * - Data length: Up to EEPROM_SYS_INFO_SIZE-1 bytes
 * - CRC byte: Appended at offset len
 * - Total size: len + 1 bytes
 *
 * CRC Calculation:
 * - Algorithm: CRC-8 polynomial 0x07 via calculate_crc8()
 * - Input: Data buffer bytes 0 to len-1
 * - Output: Stored at offset len
 *
 * @param[in] data Pointer to source buffer containing system information.
 * @param[in] len  Number of data bytes. Must be <= EEPROM_SYS_INFO_SIZE-1.
 *
 * @return 0 if write successful.
 * @return -1 if length exceeds EEPROM_SYS_INFO_SIZE-1 or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Reserves 1 byte for CRC checksum.
 * @note CRC automatically calculated and appended.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len);

/**
 * @brief Read and verify system information with CRC-8 validation.
 *
 * Reads system information buffer and verifies CRC-8 checksum. Copies data to
 * destination buffer only if CRC validation passes.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_SYS_INFO_START
 * - Data length: Up to EEPROM_SYS_INFO_SIZE-1 bytes
 * - CRC byte: Read from offset len
 * - Total read: len + 1 bytes
 *
 * CRC Validation:
 * - Calculate CRC-8 over data bytes 0 to len-1
 * - Compare with stored CRC at offset len
 * - Fail if mismatch detected
 *
 * @param[out] data Destination buffer for system information.
 * @param[in]  len  Number of data bytes to read. Must be <= EEPROM_SYS_INFO_SIZE-1.
 *
 * @return 0 if CRC validation passed and buffer populated.
 * @return -1 if length exceeds EEPROM_SYS_INFO_SIZE-1 or CRC mismatch detected.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note CRC mismatch logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Destination buffer not modified on CRC failure.
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len);
/** @} */

#endif /* NETWORK_H */

/** @} */