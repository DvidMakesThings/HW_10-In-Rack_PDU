/**
 * @file src/tasks/storage_submodule/user_output.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details
 * Implementation of user output configuration presets and apply-on-startup.
 * Manages up to 5 relay configuration presets in EEPROM with RAM caching
 * for fast access without blocking CPU on EEPROM reads.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define ST_USER_OUTPUT_TAG "[ST-USRO]"

/* ==================== RAM Cache ==================== */

/** RAM cache for user output configuration. */
static user_output_data_t s_cache;

/** Cache initialized flag. */
static bool s_cache_valid = false;

/** Mutex for cache access (uses global eepromMtx for EEPROM ops). */
static SemaphoreHandle_t s_cacheMtx = NULL;

/* ==================== Internal Helpers ==================== */

/**
 * @brief Compute CRC16-CCITT over a buffer.
 *
 * Polynomial 0x1021, initial value 0xFFFF, byte-wise processing.
 * Used to protect `user_output_data_t` header+presets region.
 *
 * @param data Pointer to input buffer.
 * @param len Number of bytes.
 * @return 16-bit CRC value.
 */
static uint16_t user_output_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Initialize RAM cache with empty defaults.
 *
 * Sets header magic, clears presets and startup selection, and computes CRC
 * over header+presets (excluding the CRC field).
 */
static void user_output_init_defaults(void) {
    memset(&s_cache, 0, sizeof(s_cache));
    s_cache.magic = USER_OUTPUT_HEADER_MAGIC;
    s_cache.startup_preset = USER_OUTPUT_STARTUP_NONE;

    /* Mark all presets as invalid (empty). */
    for (uint8_t i = 0; i < USER_OUTPUT_MAX_PRESETS; i++) {
        s_cache.presets[i].valid = 0x00;
        s_cache.presets[i].name[0] = '\0';
        s_cache.presets[i].relay_mask = 0x00;
    }

    /* Compute CRC over header + presets (excludes CRC field itself). */
    size_t crc_len = offsetof(user_output_data_t, crc);
    s_cache.crc = user_output_crc16((const uint8_t *)&s_cache, crc_len);
}

/**
 * @brief Persist current cache to EEPROM (atomic block).
 *
 * Recomputes CRC, writes in page-aligned chunks, and performs a full read-back
 * verification. Must be called with `eepromMtx` held by the caller (StorageTask).
 *
 * @return true on success; false on write or verify failure.
 */
static bool user_output_write_eeprom(void) {
    /* Recompute CRC before write. */
    size_t crc_len = offsetof(user_output_data_t, crc);
    s_cache.crc = user_output_crc16((const uint8_t *)&s_cache, crc_len);

    /* Bounds check. */
    if (sizeof(user_output_data_t) > EEPROM_USER_OUTPUT_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x10);
        ERROR_PRINT_CODE(err_code, "%s Data size exceeds EEPROM block\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return false;
    }

    /* Write to EEPROM in page-aligned chunks to avoid crossing boundaries. */
    uint16_t base = EEPROM_USER_OUTPUT_START;
    const uint8_t *src = (const uint8_t *)&s_cache;
    uint16_t total = (uint16_t)sizeof(user_output_data_t);
    uint16_t written = 0;
    while (written < total) {
        uint16_t addr = (uint16_t)(base + written);
        uint16_t page_remaining = (uint16_t)(CAT24C256_PAGE_SIZE - (addr % CAT24C256_PAGE_SIZE));
        uint16_t remaining = (uint16_t)(total - written);
        uint16_t chunk = (remaining < page_remaining) ? remaining : page_remaining;

        int wrc = CAT24C256_WriteBuffer(addr, src + written, chunk);
        if (wrc != 0) {
#if ERRORLOGGER
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x11);
            ERROR_PRINT_CODE(err_code, "%s EEPROM write failed at 0x%04X (len=%u)\r\n",
                             ST_USER_OUTPUT_TAG, addr, (unsigned)chunk);
            Storage_EnqueueErrorCode(err_code);
#endif
            return false;
        }
        written = (uint16_t)(written + chunk);
    }

    /* Read back and verify written content. */
    uint8_t verify_buf[sizeof(user_output_data_t)];
    CAT24C256_ReadBuffer(EEPROM_USER_OUTPUT_START, verify_buf,
                         (uint32_t)sizeof(user_output_data_t));
    if (memcmp(verify_buf, &s_cache, sizeof(user_output_data_t)) != 0) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x12);
        ERROR_PRINT_CODE(err_code, "%s EEPROM verify mismatch\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return false;
    }

    return true;
}

/**
 * @brief Load configuration block from EEPROM into cache and validate.
 *
 * Validates header magic, CRC, and startup preset bounds/validity. On any
 * validation failure, returns false so defaults can be initialized and saved.
 *
 * @return true if valid data was loaded; false if defaults should be used.
 */
static bool user_output_read_eeprom(void) {
    user_output_data_t temp;

    /* Read from EEPROM. */
    CAT24C256_ReadBuffer(EEPROM_USER_OUTPUT_START, (uint8_t *)&temp,
                         (uint32_t)sizeof(user_output_data_t));

    /* Validate magic byte. */
    if (temp.magic != USER_OUTPUT_HEADER_MAGIC) {
        INFO_PRINT("%s No valid data (magic=0x%02X), using defaults\n", ST_USER_OUTPUT_TAG,
                   temp.magic);
        return false;
    }

    /* Validate CRC. */
    size_t crc_len = offsetof(user_output_data_t, crc);
    uint16_t computed_crc = user_output_crc16((const uint8_t *)&temp, crc_len);
    if (computed_crc != temp.crc) {
        INFO_PRINT("%s CRC mismatch (stored=0x%04X, computed=0x%04X), using defaults\n",
                   ST_USER_OUTPUT_TAG, temp.crc, computed_crc);
        return false;
    }

    /* Validate startup preset reference. */
    if (temp.startup_preset != USER_OUTPUT_STARTUP_NONE &&
        temp.startup_preset >= USER_OUTPUT_MAX_PRESETS) {
        INFO_PRINT("%s Invalid startup preset (%u), clearing\n", ST_USER_OUTPUT_TAG,
                   temp.startup_preset);
        temp.startup_preset = USER_OUTPUT_STARTUP_NONE;
    }

    /* If startup preset points to an invalid slot, clear it. */
    if (temp.startup_preset != USER_OUTPUT_STARTUP_NONE &&
        temp.presets[temp.startup_preset].valid != USER_OUTPUT_PRESET_VALID) {
        INFO_PRINT("%s Startup preset %u is empty, clearing\n", ST_USER_OUTPUT_TAG,
                   temp.startup_preset);
        temp.startup_preset = USER_OUTPUT_STARTUP_NONE;
    }

    /* Copy validated data to cache. */
    memcpy(&s_cache, &temp, sizeof(s_cache));
    return true;
}

/* ==================== Public API Implementation ==================== */

/**
 * @brief Initialize the User Output subsystem and load presets.
 *
 * Creates local cache mutex, takes `eepromMtx` to read EEPROM, and loads
 * configuration via `user_output_read_eeprom()`. If invalid, initializes
 * defaults and writes them to EEPROM. Marks cache valid on success.
 *
 * @return true on success (cache valid); false if EEPROM mutex timed-out.
 */
bool UserOutput_Init(void) {
    /* Create cache mutex if needed. */
    if (s_cacheMtx == NULL) {
        s_cacheMtx = xSemaphoreCreateMutex();
        if (s_cacheMtx == NULL) {
#if ERRORLOGGER
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x01);
            ERROR_PRINT_CODE(err_code, "%s Mutex creation failed\r\n", ST_USER_OUTPUT_TAG);
            Storage_EnqueueErrorCode(err_code);
#endif
            return false;
        }
    }

    /* Take EEPROM mutex for read. */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) != pdTRUE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x02);
        ERROR_PRINT_CODE(err_code, "%s EEPROM mutex timeout on init\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        user_output_init_defaults();
        s_cache_valid = true;
        return false;
    }

    /* Try to load from EEPROM. */
    if (!user_output_read_eeprom()) {
        /* Initialize defaults and write to EEPROM. */
        user_output_init_defaults();
        (void)user_output_write_eeprom();
    }

    xSemaphoreGive(eepromMtx);

    s_cache_valid = true;
    INFO_PRINT("%s Initialized, startup=%s\n", ST_USER_OUTPUT_TAG,
               (s_cache.startup_preset == USER_OUTPUT_STARTUP_NONE)
                   ? "none"
                   : s_cache.presets[s_cache.startup_preset].name);

    return true;
}

/**
 * @brief Copy all presets from RAM cache.
 * @param out Destination array for `USER_OUTPUT_MAX_PRESETS` entries.
 * @return true on success; false on NULL or cache not valid.
 */
bool UserOutput_GetAllPresets(user_output_preset_t out[USER_OUTPUT_MAX_PRESETS]) {
    if (out == NULL || !s_cache_valid) {
        return false;
    }

    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    memcpy(out, s_cache.presets, sizeof(s_cache.presets));

    xSemaphoreGive(s_cacheMtx);
    return true;
}

/**
 * @brief Get a single preset from RAM cache.
 * @param index Preset index [0..USER_OUTPUT_MAX_PRESETS-1].
 * @param out Destination for preset.
 * @return true on success; false on bad index/NULL/not valid.
 */
bool UserOutput_GetPreset(uint8_t index, user_output_preset_t *out) {
    if (out == NULL || index >= USER_OUTPUT_MAX_PRESETS || !s_cache_valid) {
        return false;
    }

    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    memcpy(out, &s_cache.presets[index], sizeof(user_output_preset_t));

    xSemaphoreGive(s_cacheMtx);
    return true;
}

/**
 * @brief Save or update a preset in cache and commit to EEPROM.
 *
 * Thread-safe: takes cache mutex, then `eepromMtx` for commit.
 * @param index Preset index.
 * @param name Preset name (truncated to max length; may be NULL).
 * @param relay_mask Bitmask of relay states.
 * @return true on success; false on mutex timeout or EEPROM error.
 */
bool UserOutput_SavePreset(uint8_t index, const char *name, uint8_t relay_mask) {
    if (index >= USER_OUTPUT_MAX_PRESETS || !s_cache_valid) {
        return false;
    }

    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    /* Update cache. */
    user_output_preset_t *p = &s_cache.presets[index];
    memset(p->name, 0, sizeof(p->name));
    if (name != NULL && name[0] != '\0') {
        strncpy(p->name, name, USER_OUTPUT_NAME_MAX_LEN);
        p->name[USER_OUTPUT_NAME_MAX_LEN] = '\0';
    }
    p->relay_mask = relay_mask;
    p->valid = USER_OUTPUT_PRESET_VALID;

    xSemaphoreGive(s_cacheMtx);

    /* Write to EEPROM. */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) != pdTRUE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x20);
        ERROR_PRINT_CODE(err_code, "%s EEPROM mutex timeout on save\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return false;
    }

    bool ret = user_output_write_eeprom();
    xSemaphoreGive(eepromMtx);

    if (ret) {
        INFO_PRINT("%s Preset %u saved: '%s' mask=0x%02X\n", ST_USER_OUTPUT_TAG, index, p->name,
                   relay_mask);
    }

    return ret;
}

/**
 * @brief Delete a preset from cache and commit to EEPROM.
 * @param index Preset index.
 * @return true on success; false on invalid index or EEPROM error.
 */
bool UserOutput_DeletePreset(uint8_t index) {
    if (index >= USER_OUTPUT_MAX_PRESETS || !s_cache_valid) {
        return false;
    }

    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    /* Clear preset in cache. */
    user_output_preset_t *p = &s_cache.presets[index];
    memset(p, 0, sizeof(user_output_preset_t));
    p->valid = 0x00;

    /* If this was the startup preset, clear it. */
    if (s_cache.startup_preset == index) {
        s_cache.startup_preset = USER_OUTPUT_STARTUP_NONE;
    }

    xSemaphoreGive(s_cacheMtx);

    /* Write to EEPROM. */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) != pdTRUE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x30);
        ERROR_PRINT_CODE(err_code, "%s EEPROM mutex timeout on delete\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return false;
    }

    bool ret = user_output_write_eeprom();
    xSemaphoreGive(eepromMtx);

    if (ret) {
        INFO_PRINT("%s Preset %u deleted\n", ST_USER_OUTPUT_TAG, index);
    }

    return ret;
}

/**
 * @brief Get the startup preset selection from cache.
 * @return Preset index or `USER_OUTPUT_STARTUP_NONE` if none.
 */
uint8_t UserOutput_GetStartupPreset(void) {
    if (!s_cache_valid) {
        return USER_OUTPUT_STARTUP_NONE;
    }

    uint8_t result;
    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        result = s_cache.startup_preset;
        xSemaphoreGive(s_cacheMtx);
    } else {
        result = USER_OUTPUT_STARTUP_NONE;
    }

    return result;
}

/**
 * @brief Set the startup preset selection and commit to EEPROM.
 * @param index Preset index to use on startup.
 * @return true on success; false if preset invalid or EEPROM error.
 */
bool UserOutput_SetStartupPreset(uint8_t index) {
    if (index >= USER_OUTPUT_MAX_PRESETS || !s_cache_valid) {
        return false;
    }

    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    /* Verify preset is valid. */
    if (s_cache.presets[index].valid != USER_OUTPUT_PRESET_VALID) {
        xSemaphoreGive(s_cacheMtx);
        INFO_PRINT("%s Cannot set startup: preset %u is empty\n", ST_USER_OUTPUT_TAG, index);
        return false;
    }

    s_cache.startup_preset = index;
    xSemaphoreGive(s_cacheMtx);

    /* Write to EEPROM. */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) != pdTRUE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x40);
        ERROR_PRINT_CODE(err_code, "%s EEPROM mutex timeout on set startup\r\n",
                         ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return false;
    }

    bool ret = user_output_write_eeprom();
    xSemaphoreGive(eepromMtx);

    if (ret) {
        INFO_PRINT("%s Startup preset set to %u\n", ST_USER_OUTPUT_TAG, index);
    }

    return ret;
}

/**
 * @brief Clear the startup preset selection and commit to EEPROM.
 * @return true on success; false on mutex or EEPROM error.
 */
bool UserOutput_ClearStartupPreset(void) {
    if (!s_cache_valid) {
        return false;
    }

    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }

    s_cache.startup_preset = USER_OUTPUT_STARTUP_NONE;
    xSemaphoreGive(s_cacheMtx);

    /* Write to EEPROM. */
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) != pdTRUE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x50);
        ERROR_PRINT_CODE(err_code, "%s EEPROM mutex timeout on clear startup\r\n",
                         ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return false;
    }

    bool ret = user_output_write_eeprom();
    xSemaphoreGive(eepromMtx);

    if (ret) {
        INFO_PRINT("%s Startup preset cleared\n", ST_USER_OUTPUT_TAG);
    }

    return ret;
}

/**
 * @brief Apply the specified preset's relay mask immediately.
 * @param index Preset index.
 * @return true on success; false if empty or switch error.
 */
bool UserOutput_ApplyPreset(uint8_t index) {
    if (index >= USER_OUTPUT_MAX_PRESETS || !s_cache_valid) {
        return false;
    }

    user_output_preset_t preset;
    if (!UserOutput_GetPreset(index, &preset)) {
        return false;
    }

    if (preset.valid != USER_OUTPUT_PRESET_VALID) {
        INFO_PRINT("%s Cannot apply: preset %u is empty\n", ST_USER_OUTPUT_TAG, index);
        return false;
    }

    /* Apply relay mask using SwitchTask API. */
    switch_result_t result = Switch_SetMask(preset.relay_mask);
    if (result != SWITCH_OK) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_USER_OUTPUT, 0x60);
        ERROR_PRINT_CODE(err_code, "%s Apply preset %u failed (switch err=%d)\r\n",
                         ST_USER_OUTPUT_TAG, index, result);
        Storage_EnqueueWarningCode(err_code);
#endif
        return false;
    }

    INFO_PRINT("%s Applied preset %u '%s' mask=0x%02X\n", ST_USER_OUTPUT_TAG, index, preset.name,
               preset.relay_mask);
    return true;
}

/**
 * @brief Apply the configured startup preset, if any.
 * @return true if applied or none configured; false on error.
 */
bool UserOutput_ApplyStartupPreset(void) {
    if (!s_cache_valid) {
        return false;
    }

    uint8_t startup_id = UserOutput_GetStartupPreset();
    if (startup_id == USER_OUTPUT_STARTUP_NONE) {
        INFO_PRINT("%s No startup preset configured\n", ST_USER_OUTPUT_TAG);
        return true; /* Not an error - just nothing to do. */
    }

    INFO_PRINT("%s Applying startup preset %u\n", ST_USER_OUTPUT_TAG, startup_id);
    return UserOutput_ApplyPreset(startup_id);
}

/**
 * @brief Check whether a preset slot contains valid data.
 * @param index Preset index.
 * @return true if valid; false otherwise.
 */
bool UserOutput_IsPresetValid(uint8_t index) {
    if (index >= USER_OUTPUT_MAX_PRESETS || !s_cache_valid) {
        return false;
    }

    bool result = false;
    if (xSemaphoreTake(s_cacheMtx, pdMS_TO_TICKS(100)) == pdTRUE) {
        result = (s_cache.presets[index].valid == USER_OUTPUT_PRESET_VALID);
        xSemaphoreGive(s_cacheMtx);
    }

    return result;
}

/* ==================== Legacy API Implementation ==================== */

/**
 * @brief Legacy: Write raw relay states to EEPROM relay section.
 * @deprecated Use `UserOutput_SavePreset()` for structured presets.
 * @param data Pointer to relay states (<= `EEPROM_RELAY_STATES_SIZE`).
 * @param len Number of bytes.
 * @return 0 on success; -1 on error.
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len) {
    /* Bounds check. */
    if (len > EEPROM_RELAY_STATES_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x1);
        ERROR_PRINT_CODE(err_code, "%s Relay write length exceeds size\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    /* Write to EEPROM. */
    return CAT24C256_WriteBuffer(EEPROM_RELAY_STATES_START, data, (uint16_t)len);
}

/**
 * @brief Legacy: Read raw relay states from EEPROM relay section.
 * @deprecated Use `UserOutput_GetPreset()` for structured presets.
 * @param data Destination buffer.
 * @param len Number of bytes.
 * @return 0 on success; -1 on error.
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len) {
    /* Bounds check. */
    if (len > EEPROM_RELAY_STATES_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_USER_OUTPUT, 0x2);
        ERROR_PRINT_CODE(err_code, "%s Relay read length exceeds size\r\n", ST_USER_OUTPUT_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    /* Read from EEPROM. */
    CAT24C256_ReadBuffer(EEPROM_RELAY_STATES_START, data, (uint32_t)len);
    return 0;
}