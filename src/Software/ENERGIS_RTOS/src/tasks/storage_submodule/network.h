/**
 * @file src/tasks/storage_submodule/network.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage04 4. Network Configuration
 * @ingroup storage
 * @brief Network settings with CRC validation and MAC repair
 * @{
 * 
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Manages network configuration (IP, MAC, gateway, DNS, DHCP) in EEPROM
 * with CRC-8 validation. Includes automatic MAC address repair for corrupted values.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef NETWORK_H
#define NETWORK_H

#include "../../CONFIG.h"

/**
 * @brief Write raw network configuration block.
 * @param data Pointer to source buffer
 * @param len Must be <= EEPROM_USER_NETWORK_SIZE
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len);

/**
 * @brief Read raw network configuration block.
 * @param data Destination buffer
 * @param len Must be <= EEPROM_USER_NETWORK_SIZE
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len);

/**
 * @brief Write network configuration with CRC-8 appended.
 *
 * Layout in EEPROM: MAC(6), IP(4), SN(4), GW(4), DNS(4), DHCP(1), CRC(1) = 24 bytes
 *
 * @param net_info Pointer to structured network config
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error or null input
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info);

/**
 * @brief Read and verify network configuration with CRC-8.
 * @param net_info Destination structure for network config
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 if CRC OK, -1 on CRC mismatch or null pointer
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info);

/**
 * @brief Load network configuration from EEPROM or return built-in defaults on failure.
 *
 * Attempts to read network config with CRC validation. On success, repairs MAC if needed.
 * On failure (CRC mismatch or empty), returns defaults with derived MAC.
 *
 * @return networkInfo structure with valid data
 */
networkInfo LoadUserNetworkConfig(void);

/**
 * @brief Write system info block (serial number, firmware version).
 * @param data Pointer to source buffer
 * @param len Must be <= EEPROM_SYS_INFO_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds or write error
 */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len);

/**
 * @brief Read system info block.
 * @param data Destination buffer
 * @param len Must be <= EEPROM_SYS_INFO_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds error
 */
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len);

/**
 * @brief Write system info with CRC-8 appended.
 * @param data Pointer to source buffer
 * @param len Must be <= EEPROM_SYS_INFO_SIZE-1 (reserves 1 byte for CRC)
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds or write error
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len);

/**
 * @brief Read and verify system info with CRC-8.
 * @param data Destination buffer
 * @param len Must be <= EEPROM_SYS_INFO_SIZE-1
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 if CRC OK, -1 on CRC mismatch or bounds error
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len);

#endif /* NETWORK_H */

/** @} */