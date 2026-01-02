/**
 * @file src/tasks/storage_submodule/user_output.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage03 3. User Output
 * @ingroup storage
 * @brief User output configuration presets and apply-on-startup persistence
 * @{
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details
 * Manages up to 5 saveable relay configuration presets in EEPROM. Each preset
 * contains a user-defined name (max 25 chars) and an 8-bit relay state mask.
 * One preset can be designated as "apply-on-startup" to automatically configure
 * relay states after device boot.
 *
 * Memory Layout (0x0200-0x02FF, 256 bytes):
 * - Byte 0: Header magic (0xC5)
 * - Byte 1: Startup preset ID (0-4) or 0xFF for none
 * - Bytes 2-3: Reserved
 * - Bytes 4-143: 5 presets × 28 bytes each
 *   - Bytes 0-24: Name (25 chars)
 *   - Byte 25: Null terminator
 *   - Byte 26: Relay mask (bit N = channel N state)
 *   - Byte 27: Valid flag (0xA5 = valid, else empty)
 * - Bytes 144-145: CRC16 over bytes 0-143
 *
 * RAM Caching:
 * - Presets are loaded into RAM on startup
 * - API functions operate on RAM cache
 * - EEPROM is written only on changes
 * - Thread-safe via eepromMtx
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef USER_OUTPUT_H
#define USER_OUTPUT_H

#include "../../CONFIG.h"

/* ==================== Constants ==================== */

/** Maximum number of user-configurable presets. */
#define USER_OUTPUT_MAX_PRESETS 5u

/** Maximum preset name length (excluding null terminator). */
#define USER_OUTPUT_NAME_MAX_LEN 25u

/** Preset valid marker byte. */
#define USER_OUTPUT_PRESET_VALID 0xA5u

/** Header magic byte for data integrity check. */
#define USER_OUTPUT_HEADER_MAGIC 0xC5u

/** Value indicating no startup preset is configured. */
#define USER_OUTPUT_STARTUP_NONE 0xFFu

/* ==================== Data Structures ==================== */

/**
 * @struct user_output_preset_t
 * @brief Single configuration preset entry.
 */
typedef struct __attribute__((packed)) {
    char name[USER_OUTPUT_NAME_MAX_LEN + 1]; /**< Preset name, null-terminated. */
    uint8_t relay_mask;                      /**< Bit mask: bit N = channel N ON. */
    uint8_t valid;                           /**< 0xA5 if valid, else empty slot. */
} user_output_preset_t;

/**
 * @struct user_output_data_t
 * @brief Complete user output configuration block.
 */
typedef struct __attribute__((packed)) {
    uint8_t magic;                                         /**< Header magic (0xC5). */
    uint8_t startup_preset;                                /**< Startup preset ID (0-4) or 0xFF. */
    uint8_t reserved[2];                                   /**< Reserved for future use. */
    user_output_preset_t presets[USER_OUTPUT_MAX_PRESETS]; /**< Preset array. */
    uint16_t crc;                                          /**< CRC16 over header+presets. */
} user_output_data_t;

/* ==================== Public API ==================== */

/**
 * @brief Initialize user output subsystem and load presets from EEPROM.
 *
 * Reads the user output configuration block from EEPROM into RAM cache.
 * If data is invalid or corrupted, initializes with empty defaults.
 *
 * @note Called by StorageTask during boot. Thread-safe (uses eepromMtx).
 * @return true on success, false on EEPROM read error.
 */
bool UserOutput_Init(void);

/**
 * @brief Get all presets (from RAM cache).
 *
 * Copies current preset array to caller's buffer. Non-blocking,
 * operates on RAM cache.
 *
 * @param out Array to receive presets (must hold USER_OUTPUT_MAX_PRESETS entries).
 * @return true on success, false if out is NULL.
 */
bool UserOutput_GetAllPresets(user_output_preset_t out[USER_OUTPUT_MAX_PRESETS]);

/**
 * @brief Get a single preset by index (from RAM cache).
 *
 * @param index Preset index (0-4).
 * @param out Pointer to receive preset data.
 * @return true on success, false if index invalid or out is NULL.
 */
bool UserOutput_GetPreset(uint8_t index, user_output_preset_t *out);

/**
 * @brief Save a preset at specified index.
 *
 * Updates RAM cache and schedules EEPROM write.
 *
 * @param index Preset index (0-4).
 * @param name Preset name (max 25 chars, will be truncated).
 * @param relay_mask 8-bit relay state mask.
 * @return true on success, false on invalid index or EEPROM write error.
 */
bool UserOutput_SavePreset(uint8_t index, const char *name, uint8_t relay_mask);

/**
 * @brief Delete (clear) a preset at specified index.
 *
 * Marks the preset as invalid in RAM cache and schedules EEPROM write.
 * If the deleted preset was the startup preset, clears startup selection.
 *
 * @param index Preset index (0-4).
 * @return true on success, false on invalid index or EEPROM write error.
 */
bool UserOutput_DeletePreset(uint8_t index);

/**
 * @brief Get the startup preset index.
 *
 * @return Startup preset index (0-4), or USER_OUTPUT_STARTUP_NONE (0xFF) if none.
 */
uint8_t UserOutput_GetStartupPreset(void);

/**
 * @brief Set the startup preset index.
 *
 * Updates RAM cache and schedules EEPROM write. The specified preset
 * must exist (be valid), otherwise the operation fails.
 *
 * @param index Preset index (0-4) to apply on startup.
 * @return true on success, false if preset doesn't exist or EEPROM write error.
 */
bool UserOutput_SetStartupPreset(uint8_t index);

/**
 * @brief Clear the startup preset selection.
 *
 * After clearing, no preset will be applied automatically on boot.
 * Updates RAM cache and schedules EEPROM write.
 *
 * @return true on success, false on EEPROM write error.
 */
bool UserOutput_ClearStartupPreset(void);

/**
 * @brief Apply a preset's relay configuration immediately.
 *
 * Reads the preset from RAM cache and applies its relay mask using
 * Switch_SetMask(). Does not affect startup selection.
 *
 * @param index Preset index (0-4).
 * @return true on success, false if preset doesn't exist or switch error.
 */
bool UserOutput_ApplyPreset(uint8_t index);

/**
 * @brief Apply the startup preset if configured.
 *
 * Called during system boot after SwitchTask is ready. If a valid
 * startup preset is configured, applies its relay mask.
 *
 * @return true if startup preset was applied (or none configured), false on error.
 */
bool UserOutput_ApplyStartupPreset(void);

/**
 * @brief Check if a preset slot is valid (contains data).
 *
 * @param index Preset index (0-4).
 * @return true if preset is valid, false if empty or invalid index.
 */
bool UserOutput_IsPresetValid(uint8_t index);

/* ==================== Legacy API (deprecated) ==================== */

/**
 * @brief Write relay states to EEPROM user output section (legacy).
 * @deprecated Use UserOutput_SavePreset() instead.
 *
 * @param data Pointer to relay state array.
 * @param len Number of bytes to write.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len);

/**
 * @brief Read relay states from EEPROM user output section (legacy).
 * @deprecated Use UserOutput_GetPreset() instead.
 *
 * @param data Destination buffer for relay states.
 * @param len Number of bytes to read.
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len);

#endif /* USER_OUTPUT_H */

/** @} */