/**
 * @file src/tasks/storage_submodule/storage_common.h
 * @author DvidMakesThings - David Sipos
 * 
 * @defgroup storage Storage Common
 * @brief Shared utilities for storage submodules
 * @{
 *
 * @defgroup storage01 1. Storage Common
 * @ingroup storage
 * @brief Shared definitions and helper functions for EEPROM storage
 * @{
 * 
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Provides common utilities used across all storage submodules including
 * CRC calculation, MAC address derivation from serial number, and shared constants.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STORAGE_COMMON_H
#define STORAGE_COMMON_H

#include "../../CONFIG.h"

/**
 * @brief Calculate CRC-8 checksum.
 * @param data Input buffer
 * @param len Buffer length
 * @return CRC-8 checksum value
 */
uint8_t calculate_crc8(const uint8_t *data, size_t len);

/**
 * @brief Fill MAC address from device serial number using FNV-1a hash.
 * @param mac 6-byte MAC address buffer to fill
 */
void Energis_FillMacFromSerial(uint8_t mac[6]);

/**
 * @brief Repair MAC address if corrupted (wrong prefix or invalid suffix).
 * @param n Network info structure containing MAC to check/repair
 * @return true if MAC was repaired, false if already valid
 */
bool Energis_RepairMac(networkInfo *n);

#endif /* STORAGE_COMMON_H */

/** @} */
/** @} */