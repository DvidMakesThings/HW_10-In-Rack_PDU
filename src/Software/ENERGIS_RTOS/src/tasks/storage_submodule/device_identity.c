/**
 * @file src/tasks/storage_submodule/device_identity.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.1
 * @date 2025-12-14
 *
 * @details
 * Implementation of device identity management module. Provides centralized
 * storage and access for serial number and region settings with EEPROM
 * persistence and RAM caching.
 *
 * Thread Safety:
 * - Read operations are lock-free (atomic cache access)
 * - Write operations acquire eepromMtx for EEPROM access
 * - Unlock state uses atomic timestamp comparison
 *
 * EEPROM Block Contract:
 * - EEPROM_DEVICE_IDENTITY_START / EEPROM_DEVICE_IDENTITY_SIZE define the stored block
 * - Region is stored at offset 0x10, CRC at 0x11, pad at 0x12
 * - CRC is calculated over bytes 0x00..0x10 (serial + region)
 * - Pad byte is written deterministically as 0x00 for a stable block image
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define DEVICE_IDENTITY_TAG "[DEV-ID]"

/* ==================== EEPROM Layout Constants ==================== */

/**
 * @brief EEPROM address for device identity record.
 * @details Uses EEPROM layout defined in EEPROM_MemoryMap.h.
 */
#define EEPROM_DEVICE_IDENTITY_ADDR EEPROM_DEVICE_IDENTITY_START

/**
 * @brief Total size of device identity EEPROM block.
 * @details Uses EEPROM layout defined in EEPROM_MemoryMap.h (includes pad byte).
 */
#define EEPROM_DEVICE_IDENTITY_TOTAL_SIZE EEPROM_DEVICE_IDENTITY_SIZE

/**
 * @brief Offset of region within identity EEPROM block.
 */
#define EEPROM_DEVICE_IDENTITY_REGION_OFFSET 0x10u

/**
 * @brief Offset of CRC within identity EEPROM block.
 */
#define EEPROM_DEVICE_IDENTITY_CRC_OFFSET 0x11u

/**
 * @brief Offset of reserved pad within identity EEPROM block.
 */
#define EEPROM_DEVICE_IDENTITY_PAD_OFFSET 0x12u

/**
 * @brief Number of bytes covered by CRC calculation (serial + region).
 */
#define EEPROM_DEVICE_IDENTITY_CRC_LEN 0x11u /* 0x00..0x10 inclusive => 17 bytes */

/* ==================== Current Limit Constants ==================== */

/**
 * @brief EU region current limit in amperes.
 */
#define CURRENT_LIMIT_EU_A 10.0f

/**
 * @brief US region current limit in amperes.
 */
#define CURRENT_LIMIT_US_A 15.0f

/**
 * @brief Default (safe) current limit for unknown region.
 */
#define CURRENT_LIMIT_DEFAULT_A 10.0f

/* ==================== Module State ==================== */

/**
 * @brief RAM cache for device identity.
 * @details Populated at init, updated by provisioning writes.
 */
static device_identity_t s_cache = {
    .serial_number = "UNPROVISIONED", .region = DEVICE_REGION_UNKNOWN, .valid = false};

/**
 * @brief Provisioning unlock timestamp (ms since boot).
 * @details Set when unlock succeeds. 0 means locked.
 */
static volatile uint32_t s_unlock_timestamp_ms = 0;

/**
 * @brief Module initialized flag.
 */
static volatile bool s_initialized = false;

/* ==================== External Dependencies ==================== */

extern SemaphoreHandle_t eepromMtx;
extern uint8_t calculate_crc8(const uint8_t *data, size_t len);

/* ==================== Internal Helpers ==================== */

/**
 * @brief Calculate CRC-8 for device identity EEPROM block image.
 *
 * @details
 * CRC is calculated over bytes 0x00..0x10 (serial + region),
 * and stored at offset 0x11.
 *
 * @param raw Pointer to raw EEPROM block image.
 * @return Calculated CRC-8 value.
 */
static uint8_t calc_identity_crc_raw(const uint8_t *raw) {
    return calculate_crc8(raw, (size_t)EEPROM_DEVICE_IDENTITY_CRC_LEN);
}

/**
 * @brief Check if the provisioning window is still open.
 *
 * @return true if unlocked and within timeout window.
 */
static bool is_unlock_window_open(void) {
    if (s_unlock_timestamp_ms == 0) {
        return false;
    }

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed_ms = now_ms - s_unlock_timestamp_ms;

    if (elapsed_ms > PROV_UNLOCK_TIMEOUT_MS) {
        /* Window expired, auto-lock */
        s_unlock_timestamp_ms = 0;
        return false;
    }

    return true;
}

/**
 * @brief Read device identity from EEPROM and validate CRC.
 *
 * @param rec Destination buffer for parsed EEPROM fields.
 * @return 0 on success with valid CRC, -1 on CRC mismatch or read error.
 *
 * @note Must be called with eepromMtx held.
 */
static int read_identity_from_eeprom(device_identity_eeprom_t *rec) {
    if (!rec) {
        return -1;
    }

    uint8_t raw[EEPROM_DEVICE_IDENTITY_TOTAL_SIZE];
    memset(raw, 0, sizeof(raw));

    CAT24C256_ReadBuffer(EEPROM_DEVICE_IDENTITY_ADDR, raw, EEPROM_DEVICE_IDENTITY_TOTAL_SIZE);

    uint8_t stored_crc = raw[EEPROM_DEVICE_IDENTITY_CRC_OFFSET];
    uint8_t expected_crc = calc_identity_crc_raw(raw);

    if (stored_crc != expected_crc) {
        DEBUG_PRINT("%s CRC mismatch: stored=0x%02X, calc=0x%02X\r\n", DEVICE_IDENTITY_TAG,
                    stored_crc, expected_crc);
        return -1;
    }

    memset(rec, 0, sizeof(*rec));
    memcpy(rec->serial_number, &raw[0], DEVICE_SN_MAX_LEN + 1u);
    rec->region = raw[EEPROM_DEVICE_IDENTITY_REGION_OFFSET];
    rec->crc = stored_crc;

    return 0;
}

/**
 * @brief Write device identity to EEPROM using the defined block image.
 *
 * @param rec Source record to write (CRC will be calculated and set).
 * @return 0 on success, -1 on write error.
 *
 * @note Must be called with eepromMtx held.
 */
static int write_identity_to_eeprom(device_identity_eeprom_t *rec) {
    if (!rec) {
        return -1;
    }

    uint8_t raw[EEPROM_DEVICE_IDENTITY_TOTAL_SIZE];
    memset(raw, 0, sizeof(raw));

    /* Serial number occupies 0x00..0x0F */
    memcpy(&raw[0], rec->serial_number, DEVICE_SN_MAX_LEN + 1u);

    /* Region at 0x10 */
    raw[EEPROM_DEVICE_IDENTITY_REGION_OFFSET] = rec->region;

    /* Pad byte written deterministically */
    raw[EEPROM_DEVICE_IDENTITY_PAD_OFFSET] = 0x00u;

    /* CRC at 0x11 over 0x00..0x10 */
    rec->crc = calc_identity_crc_raw(raw);
    raw[EEPROM_DEVICE_IDENTITY_CRC_OFFSET] = rec->crc;

    /*
     * IMPORTANT: Some CAT24C256 low-level drivers only support small write sizes
     * per transaction (often 16 bytes) even though the EEPROM page size is larger.
     * The device identity block crosses the 0x0030..0x0042 range, so a single
     * 19-byte write can silently truncate to 16 bytes on such drivers.
     *
     * To make this robust across driver implementations, write in two chunks:
     * - First 16 bytes: serial number (0x00..0x0F)
     * - Remaining bytes: region + CRC + pad (0x10..0x12)
     */
    int ret = CAT24C256_WriteBuffer(EEPROM_DEVICE_IDENTITY_ADDR, raw, 16u);
    if (ret == 0) {
        ret = CAT24C256_WriteBuffer((uint16_t)(EEPROM_DEVICE_IDENTITY_ADDR + 16u), &raw[16],
                                    (uint16_t)(EEPROM_DEVICE_IDENTITY_TOTAL_SIZE - 16u));
    }

    if (ret != 0) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_DEVICE_IDENTITY, 0x1);
        ERROR_PRINT_CODE(err_code, "%s EEPROM write failed\r\n", DEVICE_IDENTITY_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    return 0;
}

/**
 * @brief Update RAM cache from EEPROM record.
 *
 * @param rec Validated EEPROM record.
 */
static void update_cache_from_record(const device_identity_eeprom_t *rec) {
    /* Copy serial number with bounds check */
    memset(s_cache.serial_number, 0, sizeof(s_cache.serial_number));
    strncpy(s_cache.serial_number, rec->serial_number, DEVICE_SN_MAX_LEN);
    s_cache.serial_number[DEVICE_SN_MAX_LEN] = '\0';

    /* Map region code */
    switch (rec->region) {
    case DEVICE_REGION_EU:
        s_cache.region = DEVICE_REGION_EU;
        break;
    case DEVICE_REGION_US:
        s_cache.region = DEVICE_REGION_US;
        break;
    default:
        s_cache.region = DEVICE_REGION_UNKNOWN;
        break;
    }

    /* Mark as valid only if serial number is non-empty */
    s_cache.valid = (s_cache.serial_number[0] != '\0');
}

/**
 * @brief Validate serial number string.
 *
 * @param sn Serial number string to validate.
 * @return true if valid, false otherwise.
 */
static bool validate_serial_number(const char *sn) {
    if (!sn || sn[0] == '\0') {
        return false;
    }

    size_t len = strlen(sn);
    if (len > DEVICE_SN_MAX_LEN) {
        return false;
    }

    /* Allow alphanumeric and hyphen */
    for (size_t i = 0; i < len; i++) {
        char c = sn[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '-')) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate region code.
 *
 * @param region Region code to validate.
 * @return true if valid, false otherwise.
 */
static bool validate_region(device_region_t region) {
    return (region == DEVICE_REGION_EU || region == DEVICE_REGION_US);
}

/* ==================== Public API - Initialization ==================== */

void DeviceIdentity_Init(void) {
    device_identity_eeprom_t rec;

    /* Acquire EEPROM mutex */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(500)) != pdTRUE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_DEVICE_IDENTITY, 0x2);
        ERROR_PRINT_CODE(err_code, "%s Init failed: EEPROM mutex timeout\r\n", DEVICE_IDENTITY_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        s_initialized = true;
        return;
    }

    /* Try to read valid identity from EEPROM */
    if (read_identity_from_eeprom(&rec) == 0) {
        update_cache_from_record(&rec);
        INFO_PRINT("%s Loaded: SN=%s, Region=%s\r\n", DEVICE_IDENTITY_TAG, s_cache.serial_number,
                   s_cache.region == DEVICE_REGION_EU   ? "EU"
                   : s_cache.region == DEVICE_REGION_US ? "US"
                                                        : "UNKNOWN");
    } else {
        /* EEPROM invalid or empty - use defaults */
        strncpy(s_cache.serial_number, "UNPROVISIONED", DEVICE_SN_MAX_LEN);
        s_cache.serial_number[DEVICE_SN_MAX_LEN] = '\0';
        s_cache.region = DEVICE_REGION_UNKNOWN;
        s_cache.valid = false;

        INFO_PRINT("%s No valid identity in EEPROM, device unprovisioned\r\n", DEVICE_IDENTITY_TAG);
    }

    xSemaphoreGive(eepromMtx);

    s_initialized = true;
}

/* ==================== Public API - Read Access ==================== */

const device_identity_t *DeviceIdentity_Get(void) { return &s_cache; }

const char *DeviceIdentity_GetSerialNumber(void) { return s_cache.serial_number; }

device_region_t DeviceIdentity_GetRegion(void) { return s_cache.region; }

float DeviceIdentity_GetCurrentLimitA(void) {
    switch (s_cache.region) {
    case DEVICE_REGION_EU:
        return CURRENT_LIMIT_EU_A;
    case DEVICE_REGION_US:
        return CURRENT_LIMIT_US_A;
    default:
        return CURRENT_LIMIT_DEFAULT_A;
    }
}

bool DeviceIdentity_IsValid(void) { return s_cache.valid; }

/* ==================== Public API - Provisioning ==================== */

bool DeviceIdentity_Unlock(const char *token) {
    if (!token) {
        return false;
    }

    /* Case-insensitive comparison */
    if (strcasecmp(token, PROV_UNLOCK_TOKEN) != 0) {
        INFO_PRINT("%s Unlock failed: invalid token\r\n", DEVICE_IDENTITY_TAG);
        return false;
    }

    /* Open write window */
    s_unlock_timestamp_ms = to_ms_since_boot(get_absolute_time());

    INFO_PRINT("%s Provisioning UNLOCKED for %u seconds\r\n", DEVICE_IDENTITY_TAG,
               PROV_UNLOCK_TIMEOUT_MS / 1000);

    return true;
}

void DeviceIdentity_Lock(void) {
    s_unlock_timestamp_ms = 0;
    INFO_PRINT("%s Provisioning LOCKED\r\n", DEVICE_IDENTITY_TAG);
}

bool DeviceIdentity_IsUnlocked(void) { return is_unlock_window_open(); }

int DeviceIdentity_SetSerialNumber(const char *serial_number) {
    /* Check unlock state */
    if (!is_unlock_window_open()) {
        INFO_PRINT("%s Set SN rejected: provisioning locked\r\n", DEVICE_IDENTITY_TAG);
        return -1;
    }

    /* Validate input */
    if (!validate_serial_number(serial_number)) {
        INFO_PRINT("%s Set SN rejected: invalid format\r\n", DEVICE_IDENTITY_TAG);
        return -2;
    }

    /* Prepare EEPROM record */
    device_identity_eeprom_t rec;
    memset(&rec, 0, sizeof(rec));
    strncpy(rec.serial_number, serial_number, DEVICE_SN_MAX_LEN);
    rec.serial_number[DEVICE_SN_MAX_LEN] = '\0';

    /*
     * Preserve current region when only SN is set.
     * Region will be written at offset 0x10 of the EEPROM block image.
     */
    rec.region = (uint8_t)s_cache.region;

    /* Write to EEPROM */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(500)) != pdTRUE) {
        return -3;
    }

    int ret = write_identity_to_eeprom(&rec);
    xSemaphoreGive(eepromMtx);

    if (ret != 0) {
        return -3;
    }

    /* Update cache */
    update_cache_from_record(&rec);

    INFO_PRINT("%s Serial number set: %s\r\n", DEVICE_IDENTITY_TAG, s_cache.serial_number);

    return 0;
}

int DeviceIdentity_SetRegion(device_region_t region) {
    /* Check unlock state */
    if (!is_unlock_window_open()) {
        INFO_PRINT("%s Set region rejected: provisioning locked\r\n", DEVICE_IDENTITY_TAG);
        return -1;
    }

    /* Validate input */
    if (!validate_region(region)) {
        INFO_PRINT("%s Set region rejected: invalid region code\r\n", DEVICE_IDENTITY_TAG);
        return -2;
    }

    /* Prepare EEPROM record */
    device_identity_eeprom_t rec;
    memset(&rec, 0, sizeof(rec));
    strncpy(rec.serial_number, s_cache.serial_number, DEVICE_SN_MAX_LEN);
    rec.serial_number[DEVICE_SN_MAX_LEN] = '\0';
    rec.region = (uint8_t)region;

    /* Write to EEPROM */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(500)) != pdTRUE) {
        return -3;
    }

    int ret = write_identity_to_eeprom(&rec);
    xSemaphoreGive(eepromMtx);

    if (ret != 0) {
        return -3;
    }

    /* Update cache */
    update_cache_from_record(&rec);

    INFO_PRINT("%s Region set: %s\r\n", DEVICE_IDENTITY_TAG,
               region == DEVICE_REGION_EU ? "EU" : "US");

    return 0;
}

/* ==================== Public API - MAC Address ==================== */

void DeviceIdentity_FillMac(uint8_t mac[6]) {
    /* FNV-1a hash initialization */
    uint32_t h = 0x811C9DC5u;
    const char *s = s_cache.serial_number;

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
