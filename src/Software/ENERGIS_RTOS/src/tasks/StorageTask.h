/**
 * @file src/tasks/StorageTask.c
 * @author DvidMakesThings - David Sipos
 * 
 * @defgroup tasks05 5. Storage Task
 * @ingroup tasks
 * @brief EEPROM/Config Storage Task Implementation (RTOS version)
 * @{
 *
 * @version 2.0
 * @date 2025-11-06
 *
 * @details
 * StorageTask Architecture:
 * - Owns ALL EEPROM access (only this task touches CAT24C512)
 * - Maintains RAM cache of critical config
 * - Debounces writes (2 second idle period)
 * - Processes requests from q_cfg queue
 * - Implements ALL EEPROM_* functions from old EEPROM_MemoryMap.c
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STORAGE_TASK_H
#define STORAGE_TASK_H

#include "../CONFIG.h"

/* Note: All includes come via CONFIG.h */
/* CONFIG.h provides: FreeRTOS, event_groups.h, EEPROM_MemoryMap.h, etc. */

/* ==================== Mutex and Event Handles ==================== */

/** EEPROM I2C bus mutex - only StorageTask takes this */
extern SemaphoreHandle_t eepromMtx;

/** Config ready event group - signals when boot config loaded */
extern EventGroupHandle_t cfgEvents;

/** Config ready bit flag */
#define CFG_READY_BIT (1 << 0)

/* ==================== Message Structures ==================== */

/** Storage request types */
typedef enum {
    STORAGE_CMD_READ_NETWORK,        /**< Read network config to RAM cache */
    STORAGE_CMD_WRITE_NETWORK,       /**< Update network config in RAM, schedule write */
    STORAGE_CMD_READ_PREFS,          /**< Read user preferences to RAM cache */
    STORAGE_CMD_WRITE_PREFS,         /**< Update prefs in RAM, schedule write */
    STORAGE_CMD_READ_RELAY_STATES,   /**< Read relay states to RAM cache */
    STORAGE_CMD_WRITE_RELAY_STATES,  /**< Update relay states in RAM, schedule write */
    STORAGE_CMD_READ_CHANNEL_LABEL,  /**< Read one channel label */
    STORAGE_CMD_WRITE_CHANNEL_LABEL, /**< Write one channel label */
    STORAGE_CMD_COMMIT,              /**< Force immediate write of pending changes */
    STORAGE_CMD_LOAD_DEFAULTS,       /**< Reset to factory defaults */
    STORAGE_CMD_READ_SENSOR_CAL,     /**< Read sensor calibration for channel */
    STORAGE_CMD_WRITE_SENSOR_CAL,    /**< Write sensor calibration for channel */
    /* SIL Testing Commands */
    STORAGE_CMD_DUMP_FORMATTED, /**< Dump EEPROM in formatted hex (SIL testing) */
    STORAGE_CMD_SELF_TEST,      /**< Run EEPROM self-test (SIL validation) */
} storage_cmd_t;

/** Storage request message (posted to q_cfg) */
typedef struct {
    storage_cmd_t cmd; /**< Command type */
    union {
        /* For network config */
        networkInfo net_info;

        /* For user preferences */
        userPrefInfo user_prefs;

        /* For relay states */
        struct {
            uint8_t states[8]; /**< Relay states 0=off, 1=on */
        } relay;

        /* For channel labels */
        struct {
            uint8_t channel; /**< Channel index 0-7 */
            char label[64];  /**< Label string (null-terminated) */
        } ch_label;

        /* For sensor calibration */
        struct {
            uint8_t channel;   /**< Channel index 0-7 */
            hlw_calib_t calib; /**< Calibration data */
        } sensor_cal;

        /* For SIL testing */
        struct {
            uint16_t test_addr; /**< Test address for self-test */
            bool *result;       /**< Pointer to result for self-test */
        } sil_test;
    } data;

    /** Optional: pointer to output buffer for read operations */
    void *output_ptr;

    /** Optional: semaphore to signal completion */
    SemaphoreHandle_t done_sem;
} storage_msg_t;

/* ==================== RAM Config Cache ==================== */

/** RAM cache structure (owned by StorageTask) */
typedef struct {
    networkInfo network;       /**< Network configuration */
    userPrefInfo preferences;  /**< User preferences */
    uint8_t relay_states[8];   /**< Relay power-on states */
    hlw_calib_t sensor_cal[8]; /**< Sensor calibration per channel */

    /** Dirty flags for debounced writes */
    bool network_dirty;
    bool prefs_dirty;
    bool relay_dirty;
    bool sensor_cal_dirty[8];

    /** Timestamp of last change (for debouncing) */
    TickType_t last_change_tick;

} storage_cache_t;

/* ==================== Public API ==================== */

/**
 * @brief Initialize and start the Storage task with a deterministic enable gate.
 *
 * @details
 * - Deterministic boot order step 3/6. Waits for Console to be READY.
 * - If @p enable is false, storage is skipped and marked NOT ready.
 * - Signals CFG_READY via cfgEvents; READY latch is set after task creation.
 *
 * @instructions
 * Call after ConsoleTask_Init(true):
 *   StorageTask_Init(true);
 * Query readiness via Storage_IsReady().
 *
 * @param enable Gate that allows or skips starting this subsystem.
 */
void StorageTask_Init(bool enable);
/**
 * @brief Storage subsystem readiness query (configuration loaded).
 *
 * @details
 * Returns true once StorageTask has signaled CFG_READY in cfgEvents. This directly
 * reflects the config-ready bit instead of relying on any separate latch.
 *
 * @return true if configuration is ready, false otherwise.
 */
bool Storage_IsReady(void);

/**
 * @brief Check if config is loaded and ready.
 */
bool Storage_Config_IsReady(void);

/**
 * @brief Wait for config to be loaded from EEPROM.
 */
bool storage_wait_ready(uint32_t timeout_ms);

/**
 * @brief Get current network config (from RAM cache).
 */
bool storage_get_network(networkInfo *out);

/**
 * @brief Set network config (update RAM cache, schedule EEPROM write).
 */
bool storage_set_network(const networkInfo *net);

/**
 * @brief Get current user preferences (from RAM cache).
 */
bool storage_get_prefs(userPrefInfo *out);

/**
 * @brief Set user preferences (update RAM cache, schedule EEPROM write).
 */
bool storage_set_prefs(const userPrefInfo *prefs);

/**
 * @brief Get relay power-on states (from RAM cache).
 */
bool storage_get_relay_states(uint8_t *out);

/**
 * @brief Set relay power-on states (update RAM cache, schedule write).
 */
bool storage_set_relay_states(const uint8_t *states);

/**
 * @brief Force immediate commit of all pending changes to EEPROM.
 */
bool storage_commit_now(uint32_t timeout_ms);

/**
 * @brief Reset all config to factory defaults.
 */
bool storage_load_defaults(uint32_t timeout_ms);

/**
 * @brief Get sensor calibration for one channel (from RAM cache).
 */
bool storage_get_sensor_cal(uint8_t channel, hlw_calib_t *out);

/**
 * @brief Set sensor calibration for one channel (update cache, schedule write).
 */
bool storage_set_sensor_cal(uint8_t channel, const hlw_calib_t *cal);

/* ==================== SIL Testing API ==================== */

/**
 * @brief Dump entire EEPROM in formatted hex table (SIL testing).
 */
bool storage_dump_formatted(uint32_t timeout_ms);

/**
 * @brief Run EEPROM self-test for SIL validation.
 */
bool storage_self_test(uint16_t test_addr, uint32_t timeout_ms);

/* ==================== EEPROM Logical Sections API (Prototypes) ==================== */
/* Place this block in StorageTask.h (after includes). All functions return 0 on success, -1 on
 * failure. */

/**
 * @brief Erase the entire CAT24C512 by writing 0xFF over all addresses.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on I2C/write failure.
 */
int EEPROM_EraseAll(void);

/* ---- System Info ---- */

/**
 * @brief Write raw System Info bytes (e.g., "SN\0SWVERSION\0") to EEPROM.
 * @param data Pointer to source buffer.
 * @param len  Number of bytes to write. Must be <= EEPROM_SYS_INFO_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds or write error.
 */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len);

/**
 * @brief Read raw System Info bytes from EEPROM.
 * @param data Destination buffer.
 * @param len  Number of bytes to read. Must be <= EEPROM_SYS_INFO_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds error.
 */
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len);

/**
 * @brief Write System Info with trailing CRC-8 byte.
 * @param data Pointer to payload (without CRC).
 * @param len  Payload length. Must be <= (EEPROM_SYS_INFO_SIZE - 1).
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds or write error.
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len);

/**
 * @brief Read and verify System Info with CRC-8.
 * @param data Destination for payload (CRC byte is verified and not copied).
 * @param len  Payload length expected (without CRC).
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 if CRC OK, -1 on bounds or CRC mismatch.
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len);

/* ---- User Output (Relay States) ---- */

/**
 * @brief Write 8 relay power-on states.
 * @param data Pointer to 8-byte array (0=off, 1=on).
 * @param len  Must be <= EEPROM_USER_OUTPUT_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len);

/**
 * @brief Read relay power-on states.
 * @param data Destination buffer.
 * @param len  Must be <= EEPROM_USER_OUTPUT_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len);

/* ---- Network Configuration ---- */

/**
 * @brief Write raw network block (MAC, IP, SN, GW, DNS, DHCP).
 * @param data Pointer to source block.
 * @param len  Must be <= EEPROM_USER_NETWORK_SIZE.
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len);

/**
 * @brief Read raw network block.
 * @param data Destination buffer.
 * @param len  Must be <= EEPROM_USER_NETWORK_SIZE.
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len);

/**
 * @brief Write network configuration with CRC-8 appended.
 * @param net_info Pointer to structured network config.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error or null input.
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info);

/**
 * @brief Read and verify network configuration with CRC-8.
 * @param net_info Destination structure for network config.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 if CRC OK, -1 on CRC mismatch or null pointer.
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info);

/* ---- Sensor Calibration ---- */

/**
 * @brief Write the entire sensor calibration area.
 * @param data Pointer to source buffer.
 * @param len  Must be <= EEPROM_SENSOR_CAL_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds or write error.
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len);

/**
 * @brief Read the entire sensor calibration area.
 * @param data Destination buffer.
 * @param len  Must be <= EEPROM_SENSOR_CAL_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds error.
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len);

/**
 * @brief Write calibration record for one channel.
 * @param ch  Channel index [0..7].
 * @param in  Pointer to calibration struct.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null input.
 */
int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in);

/**
 * @brief Read calibration record for one channel.
 * @param ch   Channel index [0..7].
 * @param out  Destination calibration struct.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null pointer.
 */
int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out);

/* ---- Energy Monitoring ---- */

/**
 * @brief Write the energy monitoring region.
 * @param data Pointer to source buffer.
 * @param len  Must be <= EEPROM_ENERGY_MON_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len);

/**
 * @brief Read the energy monitoring region.
 * @param data Destination buffer.
 * @param len  Must be <= EEPROM_ENERGY_MON_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len);

/**
 * @brief Append a fixed-size energy record to the ring buffer and update pointer.
 * @param data Pointer to one energy record of size ENERGY_RECORD_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on write error.
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data);

/* ---- Event Logs ---- */

/**
 * @brief Write the event log region.
 * @param data Pointer to source buffer.
 * @param len  Must be <= EEPROM_EVENT_LOG_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len);

/**
 * @brief Read the event log region.
 * @param data Destination buffer.
 * @param len  Must be <= EEPROM_EVENT_LOG_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len);

/**
 * @brief Append one event log entry to the ring buffer and update pointer.
 * @param entry Pointer to one entry of size EVENT_LOG_ENTRY_SIZE.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on write error.
 */
int EEPROM_AppendEventLog(const uint8_t *entry);

/* ---- User Preferences ---- */

/**
 * @brief Write raw user preferences block.
 * @param data Pointer to source buffer.
 * @param len  Must be <= EEPROM_USER_PREF_SIZE.
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len);

/**
 * @brief Read raw user preferences block.
 * @param data Destination buffer.
 * @param len  Must be <= EEPROM_USER_PREF_SIZE.
 * @warning Prefer the CRC-verified APIs for robustness.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len);

/**
 * @brief Write user preferences with CRC-8 verification.
 * @param prefs Pointer to structured preferences.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on null input or write error.
 */
int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs);

/**
 * @brief Read and verify user preferences with CRC-8.
 * @param prefs Destination structure for preferences.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 if CRC OK, -1 on CRC mismatch or null pointer.
 */
int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs);

/**
 * @brief Write default device name and location to preferences (with CRC).
 * @return 0 on success, -1 on write error.
 */
int EEPROM_WriteDefaultNameLocation(void);

/* ---- Channel Labels ---- */

/**
 * @brief Write null-terminated label string for a channel (stored in fixed slot).
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1].
 * @param label         Null-terminated label string.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null input.
 */
int EEPROM_WriteChannelLabel(uint8_t channel_index, const char *label);

/**
 * @brief Read label string for a channel.
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1].
 * @param out           Destination buffer for label.
 * @param out_len       Size of destination buffer in bytes.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null/zero-sized buffer.
 */
int EEPROM_ReadChannelLabel(uint8_t channel_index, char *out, size_t out_len);

/**
 * @brief Clear one channel label slot (fills with zeros).
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1].
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or write error.
 */
int EEPROM_ClearChannelLabel(uint8_t channel_index);

/**
 * @brief Clear all channel label slots.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on write error.
 */
int EEPROM_ClearAllChannelLabels(void);

/* ---- Factory Defaults and Loaders ---- */

/**
 * @brief Check if factory defaults need to be written (first boot).
 * @return true if defaults were written, false otherwise
 */
bool check_factory_defaults(void);

/**
 * @brief Write factory defaults to all EEPROM sections.
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 if any section write fails.
 */
int EEPROM_WriteFactoryDefaults(void);

/**
 * @brief Perform basic read-back checks after factory defaulting.
 * @return 0 on success, -1 on validation failure.
 */
int EEPROM_ReadFactoryDefaults(void);

/**
 * @brief Load user preferences from EEPROM or return built-in defaults on failure.
 * @return userPrefInfo structure with valid data.
 */
userPrefInfo LoadUserPreferences(void);

/**
 * @brief Load network configuration from EEPROM or return built-in defaults on failure.
 * @return networkInfo structure with valid data.
 */
networkInfo LoadUserNetworkConfig(void);

#endif /* STORAGE_TASK_H */

/** @} */