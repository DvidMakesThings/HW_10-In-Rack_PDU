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
 * @brief Calculate CRC-8 checksum for data integrity verification.
 * @details See storage_common.h for full API documentation.
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
 * @brief Generate MAC address from device serial number.
 * @details See storage_common.h for full API documentation.
 */
void Energis_FillMacFromSerial(uint8_t mac[6]) { DeviceIdentity_FillMac(mac); }

/**
 * @brief Validate and repair corrupted MAC address.
 * @details See storage_common.h for full API documentation.
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