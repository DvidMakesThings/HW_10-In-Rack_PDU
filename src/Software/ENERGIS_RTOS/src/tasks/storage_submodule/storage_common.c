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

/**
 * @brief Dumps entire EEPROM contents in formatted hex to log.
 *
 * Reads all EEPROM data in 16-byte chunks and logs in hex format.
 *
 * @details
 * This helper walks the whole CAT24C512 address space and prints a
 * human-readable hex dump header plus one line per 16-byte row.
 * During the dump the Storage task temporarily runs at idle priority
 * and periodically delays so that other tasks (Net, Button, Health, etc.)
 * can continue to run and feed the watchdog. Intended for debug use only
 * because it generates a large amount of log output.
 *
 * @param None
 * @return None
 */
void CAT24C512_DumpFormatted(void) {
    char line[256], field[16];
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    UBaseType_t old_prio = uxTaskPriorityGet(h);

    vTaskPrioritySet(h, tskIDLE_PRIORITY);

    log_printf("EE_DUMP_START\r\n");

    snprintf(line, sizeof(line), "%-7s", "Addr");
    for (uint8_t col = 0; col < 16; col++) {
        snprintf(field, sizeof(field), "%-7X", col);
        strncat(line, field, sizeof(line) - strlen(line) - 1);
    }
    log_printf("%s\r\n", line);

    for (uint32_t addr = 0; addr < CAT24C512_TOTAL_SIZE; addr += 16) {
        uint8_t buffer[16];

        CAT24C512_ReadBuffer((uint16_t)addr, buffer, 16);

        snprintf(line, sizeof(line), "0x%04X ", (uint16_t)addr);
        for (uint8_t i = 0; i < 16; i++) {
            snprintf(field, sizeof(field), "%02X ", buffer[i]);
            strncat(line, field, sizeof(line) - strlen(line) - 1);
        }

        log_printf("%s\r\n", line);

        Health_Heartbeat(HEALTH_ID_STORAGE);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    Health_Heartbeat(HEALTH_ID_STORAGE);
    log_printf("EE_DUMP_END\r\n");

    vTaskPrioritySet(h, old_prio);
}

/**
 * @brief Reads console password hash from EEPROM
 * @param hash Output buffer (32 bytes)
 * @return true on success, false on failure
 */
bool EEPROM_ReadConsoleHash(uint8_t hash[32]) {
    if (!hash)
        return false;

    /* Read from EEPROM */
    CAT24C512_ReadBuffer(EEPROM_CONSOLE_HASH_ADDR, hash, 32);

    /* Check if hash is all 0xFF (unprogrammed) or all 0x00 (invalid) */
    bool all_ff = true;
    bool all_00 = true;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0xFF)
            all_ff = false;
        if (hash[i] != 0x00)
            all_00 = false;
    }

    /* If unprogrammed, return default hash (SHA-256 of "admin") */
    if (all_ff || all_00) {
        const uint8_t default_hash[32] = CONSOLE_DEFAULT_PASSWORD_HASH;
        memcpy(hash, default_hash, 32);
        WARNING_PRINT("[EEPROM] Console hash unprogrammed, using default password 'admin'\r\n");
        WARNING_PRINT("[EEPROM] SECURITY WARNING: Change console password immediately!\r\n");
        return true;
    }

    INFO_PRINT("[EEPROM] Console hash loaded from EEPROM\r\n");
    return true;
}

/**
 * @brief Writes console password hash to EEPROM
 * @param hash Input buffer (32 bytes)
 * @return true on success, false on failure (returns int to match factory_defaults.c)
 */
int EEPROM_WriteConsoleHash(const uint8_t hash[32]) {
    if (!hash)
        return -1;

    int result = CAT24C512_WriteBuffer(EEPROM_CONSOLE_HASH_ADDR, hash, 32);

    if (result == 0) {
        INFO_PRINT("[EEPROM] Console password hash written\r\n");

        /* Verify write */
        uint8_t verify[32];
        CAT24C512_ReadBuffer(EEPROM_CONSOLE_HASH_ADDR, verify, 32);
        if (memcmp(hash, verify, 32) == 0) {
            INFO_PRINT("[EEPROM] Console hash verified OK\r\n");
            return 0;
        } else {
            ERROR_PRINT("[EEPROM] Console hash verification FAILED!\r\n");
            return -1;
        }
    } else {
        ERROR_PRINT("[EEPROM] Failed to write console hash\r\n");
        return -1;
    }
}