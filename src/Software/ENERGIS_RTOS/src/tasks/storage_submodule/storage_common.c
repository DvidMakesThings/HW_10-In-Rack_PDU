/**
 * @file src/tasks/storage_submodule/storage_common.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of shared utilities for EEPROM storage submodules.
 * Provides CRC-8 checksum calculation and MAC address derivation from serial number.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

/**
 * @brief Calculate CRC-8 checksum using polynomial 0x07.
 *
 * Implements CRC-8 algorithm for data integrity verification.
 * Used for network config and user preferences validation.
 *
 * @param data Input buffer to calculate CRC over
 * @param len Length of input buffer in bytes
 * @return Calculated CRC-8 value
 */
uint8_t calculate_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief Derive ENERGIS MAC address from serial number using FNV-1a 32-bit hash.
 *
 * Uses the FNV-1a hash algorithm to generate unique last 3 bytes of MAC
 * from device serial number. First 3 bytes use ENERGIS_MAC_PREFIX.
 *
 * @param mac 6-byte output buffer for MAC address
 */
void Energis_FillMacFromSerial(uint8_t mac[6]) {
#ifndef SERIAL_NUMBER
#define SERIAL_NUMBER SERIAL_NUMBER_STR SERIAL_NUMBER_NUM
#endif
    /* FNV-1a hash initialization */
    uint32_t h = 0x811C9DC5u;
    const char *s = (const char *)SERIAL_NUMBER;

    /* Hash serial number string */
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 0x01000193u;
    }

    /* Set MAC: prefix (3 bytes) + hash-derived suffix (3 bytes) */
    mac[0] = ENERGIS_MAC_PREFIX0;
    mac[1] = ENERGIS_MAC_PREFIX1;
    mac[2] = ENERGIS_MAC_PREFIX2;
    mac[3] = (uint8_t)(h >> 16);
    mac[4] = (uint8_t)(h >> 8);
    mac[5] = (uint8_t)h;
}

/**
 * @brief Check and repair MAC address if invalid.
 *
 * Validates MAC prefix and suffix. Repairs if:
 * - Prefix doesn't match ENERGIS_MAC_PREFIX
 * - Suffix is all zeros
 * - Suffix is all 0xFF
 *
 * @param n Network info structure containing MAC to validate/repair
 * @return true if MAC was repaired, false if already valid
 */
bool Energis_RepairMac(networkInfo *n) {
    /* Check for wrong prefix */
    const bool wrong_prefix = n->mac[0] != ENERGIS_MAC_PREFIX0 ||
                              n->mac[1] != ENERGIS_MAC_PREFIX1 || n->mac[2] != ENERGIS_MAC_PREFIX2;

    /* Check for invalid suffix patterns */
    const bool zero_suffix = (n->mac[3] | n->mac[4] | n->mac[5]) == 0x00;
    const bool ff_suffix = (n->mac[3] & n->mac[4] & n->mac[5]) == 0xFF;

    /* Repair if any validation failed */
    if (wrong_prefix || zero_suffix || ff_suffix) {
        Energis_FillMacFromSerial(n->mac);
        return true;
    }

    return false;
}