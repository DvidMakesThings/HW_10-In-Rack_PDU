/**
 * @file src/tasks/storage_submodule/storage_common.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-12-14
 *
 * @details
 * Implementation of shared utilities for EEPROM storage submodules.
 * Provides CRC-8 checksum calculation and MAC address management.
 *
 * Version History:
 * - v1.x: MAC derivation from compile-time SERIAL_NUMBER macro
 * - v2.0: MAC derivation delegated to DeviceIdentity module
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define ST_COMMON_TAG "[ST-COM]"

/**
 * @brief Calculate CRC-8 checksum using polynomial 0x07.
 *
 * @details
 * Implements CRC-8 algorithm for data integrity verification.
 * Used for network config, user preferences, and device identity validation.
 *
 * @param data Input buffer to calculate CRC over.
 * @param len Length of input buffer in bytes.
 * @return Calculated CRC-8 value.
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
 * @brief Derive ENERGIS MAC address from the cached serial number.
 *
 * @details
 * Delegates to DeviceIdentity_FillMac() which uses FNV-1a hash of the
 * serial number stored in EEPROM (via RAM cache) to generate the last
 * 3 bytes of the MAC address. First 3 bytes use ENERGIS_MAC_PREFIX.
 *
 * @param mac 6-byte output buffer for MAC address.
 *
 * @note This function requires DeviceIdentity_Init() to have been called.
 */
void Energis_FillMacFromSerial(uint8_t mac[6]) { DeviceIdentity_FillMac(mac); }

/**
 * @brief Check and repair MAC address if invalid.
 *
 * @details
 * Validates MAC prefix and suffix. Repairs if:
 * - Prefix doesn't match ENERGIS_MAC_PREFIX
 * - Suffix is all zeros
 * - Suffix is all 0xFF
 *
 * In addition, if the device identity is provisioned (valid serial number and region
 * loaded from EEPROM), the function enforces that the MAC address matches the
 * serial-derived value. This ensures that a MAC derived from the placeholder
 * "UNPROVISIONED" value will be replaced after provisioning.
 *
 * Uses DeviceIdentity_FillMac() for repair which derives the MAC from
 * the cached serial number.
 *
 * @param n Network info structure containing MAC to validate/repair.
 * @return true if MAC was repaired, false if already valid.
 */
bool Energis_RepairMac(networkInfo *n) {
    /* Check for wrong prefix */
    const bool wrong_prefix = n->mac[0] != ENERGIS_MAC_PREFIX0 ||
                              n->mac[1] != ENERGIS_MAC_PREFIX1 || n->mac[2] != ENERGIS_MAC_PREFIX2;

    /* Check for invalid suffix patterns */
    const bool zero_suffix = (n->mac[3] | n->mac[4] | n->mac[5]) == 0x00;
    const bool ff_suffix = (n->mac[3] & n->mac[4] & n->mac[5]) == 0xFF;

    /*
     * If the device is provisioned, enforce that the MAC matches the current
     * serial-derived MAC. This updates MAC after SN is written to EEPROM.
     */
    if (DeviceIdentity_IsValid()) {
        uint8_t expected_mac[6];
        DeviceIdentity_FillMac(expected_mac);

        if (memcmp(n->mac, expected_mac, sizeof(expected_mac)) != 0) {
            memcpy(n->mac, expected_mac, sizeof(expected_mac));
            return true;
        }
    }

    /* Repair if any validation failed */
    if (wrong_prefix || zero_suffix || ff_suffix) {
        DeviceIdentity_FillMac(n->mac);
        return true;
    }

    return false;
}