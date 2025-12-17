/**
 * @file src/tasks/storage_submodule/device_identity.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage10 Device Identity Module
 * @ingroup storage
 * @brief Serial Number and Region Management with EEPROM Persistence
 * @{
 *
 * @version 1.1.0
 * @date 2025-12-14
 *
 * @details
 * This module provides centralized management of device identity parameters:
 * - Serial number (up to 15 characters + null terminator)
 * - Region setting (EU / US)
 *
 * Architecture:
 * - RAM cache provides fast, lock-free read access for all modules
 * - EEPROM provides non-volatile persistence with CRC-8 validation
 * - EEPROM layout is defined **exclusively** in EEPROM_MemoryMap.h
 * - No module is allowed to redefine EEPROM address or size locally
 *
 * Provisioning:
 * - Requires explicit unlock with a fixed token
 * - Unlock opens a time-limited write window
 * - Writes are rejected outside the window
 *
 * IMPORTANT:
 * - All consumers must read identity **only from the RAM cache**
 * - EEPROM is accessed only by this module
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

/* ==================== Configuration Constants ==================== */

/**
 * @brief Maximum length of serial number string (excluding null terminator).
 */
#define DEVICE_SN_MAX_LEN 15

/**
 * @brief Provisioning unlock token (hex string).
 * @note This is the expected token for `PROV UNLOCK <token>` command.
 */
#define PROV_UNLOCK_TOKEN "6D61676963"

/**
 * @brief Provisioning unlock timeout in milliseconds.
 * @details Write window closes automatically after this duration.
 */
#define PROV_UNLOCK_TIMEOUT_MS 60000

/* ==================== Type Definitions ==================== */

/**
 * @brief Device region enumeration.
 * @details Determines current limit for overcurrent protection:
 *          - EU: 10A (IEC/ENEC compliant)
 *          - US: 15A (UL/CSA compliant)
 */
typedef enum {
    DEVICE_REGION_UNKNOWN = 0x00, /**< Unprovisioned or invalid region. */
    DEVICE_REGION_EU = 0x45,      /**< EU region (10A limit). ASCII 'E'. */
    DEVICE_REGION_US = 0x55       /**< US region (15A limit). ASCII 'U'. */
} device_region_t;

/**
 * @brief Device identity RAM cache structure.
 * @details Provides fast read access to identity parameters.
 *          All fields are populated from EEPROM at boot.
 */
typedef struct {
    char serial_number[DEVICE_SN_MAX_LEN + 1]; /**< Null-terminated serial number. */
    device_region_t region;                    /**< Device region setting. */
    bool valid;                                /**< True if cache contains valid data. */
} device_identity_t;

/**
 * @brief Device identity EEPROM payload fields (without reserved pad).
 *
 * @details
 * This struct represents the logical identity fields stored in EEPROM.
 * The reserved padding byte (offset 0x12) is not represented here, but is
 * still written deterministically to enforce a stable EEPROM block image.
 *
 * CRC is stored at offset 0x11 in the EEPROM block.
 */
typedef struct __attribute__((packed)) {
    char serial_number[DEVICE_SN_MAX_LEN + 1]; /**< Null-terminated serial number (16 bytes). */
    uint8_t region;                            /**< Region code (device_region_t). */
    uint8_t crc;                               /**< CRC-8 over serial_number+region (17 bytes). */
} device_identity_eeprom_t;

/* ==================== Public API - Initialization ==================== */

/**
 * @brief Initialize the device identity module.
 *
 * @details
 * Loads device identity from EEPROM into the RAM cache. If EEPROM data is
 * invalid (CRC mismatch or empty), the cache is marked invalid and default
 * placeholder values are used.
 *
 * This function must be called early in the boot sequence, after EEPROM
 * driver initialization but before any module that needs SN/region access.
 *
 * @note Thread-safe. May be called before scheduler starts.
 */
void DeviceIdentity_Init(void);

/* ==================== Public API - Read Access ==================== */

/**
 * @brief Get pointer to the device identity RAM cache.
 *
 * @return Pointer to device_identity_t cache (never NULL).
 */
const device_identity_t *DeviceIdentity_Get(void);

/**
 * @brief Get the cached serial number string.
 *
 * @return Pointer to null-terminated serial number string.
 */
const char *DeviceIdentity_GetSerialNumber(void);

/**
 * @brief Get the cached device region.
 *
 * @return device_region_t value.
 */
device_region_t DeviceIdentity_GetRegion(void);

/**
 * @brief Get the current limit in amperes based on cached region.
 *
 * @return Current limit in amperes as float.
 */
float DeviceIdentity_GetCurrentLimitA(void);

/**
 * @brief Check if device identity is valid (provisioned).
 *
 * @return true if cache contains valid provisioned data, false otherwise.
 */
bool DeviceIdentity_IsValid(void);

/* ==================== Public API - Provisioning ==================== */

/**
 * @brief Attempt to unlock provisioning with the given token.
 *
 * @param token Hex string token to validate.
 * @return true if unlock succeeded, false if token mismatch.
 */
bool DeviceIdentity_Unlock(const char *token);

/**
 * @brief Lock provisioning (close write window).
 */
void DeviceIdentity_Lock(void);

/**
 * @brief Check if provisioning is currently unlocked.
 *
 * @return true if unlocked and write window is open, false otherwise.
 */
bool DeviceIdentity_IsUnlocked(void);

/**
 * @brief Set the device serial number (requires unlock).
 *
 * @param serial_number New serial number string.
 * @return 0 on success, -1 if locked, -2 if invalid input, -3 if EEPROM error.
 */
int DeviceIdentity_SetSerialNumber(const char *serial_number);

/**
 * @brief Set the device region (requires unlock).
 *
 * @param region New region setting (DEVICE_REGION_EU or DEVICE_REGION_US).
 * @return 0 on success, -1 if locked, -2 if invalid region, -3 if EEPROM error.
 */
int DeviceIdentity_SetRegion(device_region_t region);

/* ==================== Public API - MAC Address ==================== */

/**
 * @brief Derive MAC address from the cached serial number.
 *
 * @param mac 6-byte output buffer for MAC address.
 */
void DeviceIdentity_FillMac(uint8_t mac[6]);

#endif /* DEVICE_IDENTITY_H */

/** @} */