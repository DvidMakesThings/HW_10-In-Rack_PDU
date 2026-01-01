/**
 * @file StorageTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks05 5. Storage Task
 * @ingroup tasks
 * @brief EEPROM/Config Storage Task Implementation (RTOS version)
 * @{
 *
 * @version 3.1.0
 * @date 2025-01-01
 *
 * @details
 * StorageTask Architecture:
 * - Owns ALL EEPROM access (only this task touches CAT24C256)
 * - Maintains RAM cache of critical config (network, prefs, relay states, labels)
 * - Debounces writes (2 second idle period)
 * - Processes requests from q_cfg queue
 * - Uses modular subcomponents for EEPROM section management
 *
 * Submodules (in storage_submodule/):
 * - storage_common: CRC and MAC utilities
 * - factory_defaults: First-boot initialization
 * - user_output: Relay state persistence
 * - network: Network config with CRC
 * - calibration: Sensor calibration data
 * - energy_monitor: Energy logging ring buffer
 * - event_log: Event logging ring buffer
 * - user_prefs: Device name/location/settings
 * - channel_labels: User-defined channel labels (RAM cached)
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STORAGE_TASK_H
#define STORAGE_TASK_H

#include "../CONFIG.h"

/* Include all storage submodules */

/* Note: All includes come via CONFIG.h */
/* CONFIG.h provides: FreeRTOS, event_groups.h, EEPROM_MemoryMap.h, etc. */

/* ==================== Mutex and Event Handles ==================== */

/** EEPROM I2C bus mutex - only StorageTask takes this */
extern SemaphoreHandle_t eepromMtx;

/** Config ready event group - signals when boot config loaded */
extern EventGroupHandle_t cfgEvents;

/**
 * @brief Queue handles for deferred error/warning logging.
 */
extern QueueHandle_t g_errorCodeQueue;
extern QueueHandle_t g_warningCodeQueue;

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
    STORAGE_CMD_READ_CHANNEL_LABEL,  /**< Read one channel label (from cache) */
    STORAGE_CMD_WRITE_CHANNEL_LABEL, /**< Write one channel label (cache + EEPROM) */
    STORAGE_CMD_COMMIT,              /**< Force immediate write of pending changes */
    STORAGE_CMD_LOAD_DEFAULTS,       /**< Reset to factory defaults */
    STORAGE_CMD_READ_SENSOR_CAL,     /**< Read sensor calibration for channel */
    STORAGE_CMD_WRITE_SENSOR_CAL,    /**< Write sensor calibration for channel */
    STORAGE_CMD_DUMP_ERROR_LOG,      /**< Dump error event log region */
    STORAGE_CMD_DUMP_WARNING_LOG,    /**< Dump warning event log region */
    STORAGE_CMD_CLEAR_ERROR_LOG,     /**< Clear error event log region */
    STORAGE_CMD_CLEAR_WARNING_LOG,   /**< Clear warning event log region */
    STORAGE_CMD_ERASE_ALL,           /**< Incremental full EEPROM erase */

    /* SIL Testing Commands */
    STORAGE_CMD_DUMP_FORMATTED, /**< Dump EEPROM in formatted hex (SIL testing) */
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

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

/**
 * @brief Trigger a formatted EEPROM dump asynchronously.
 *
 * Enqueues a @ref STORAGE_CMD_DUMP_FORMATTED message and returns immediately
 * without waiting for completion. The dump is executed by StorageTask in the
 * background, and log output is generated from there.
 * @return true if the request was enqueued, false if the queue was full or
 *         storage is not ready.
 */
bool storage_dump_formatted_async(void);

/**
 * @brief Start full EEPROM erase (async, watchdog-safe).
 *
 * @return true if erase was started, false if storage not ready or already busy.
 */
bool storage_erase_all_async(void);

/**
 * @brief Check if EEPROM erase is in progress.
 *
 * @return true if erase active.
 */
bool storage_erase_all_is_busy(void);

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
 * @return None
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
 * @return true if config is ready, false otherwise.
 */
bool Storage_Config_IsReady(void);

/**
 * @brief Wait for config to be loaded from EEPROM.
 *
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @return true if config became ready within timeout, false otherwise.
 */
bool storage_wait_ready(uint32_t timeout_ms);

/**
 * @brief Get current network config (from RAM cache).
 *
 * @param out Pointer to output structure (not NULL).
 * @return true on success, false on error.
 */
bool storage_get_network(networkInfo *out);

/**
 * @brief Set network config (update RAM cache, schedule EEPROM write).
 *
 * @param net Pointer to new network config (not NULL).
 * @return true on success, false on error.
 */
bool storage_set_network(const networkInfo *net);

/**
 * @brief Get current user preferences (from RAM cache).
 *
 * @param out Pointer to output structure (not NULL).
 * @return true on success, false on error.
 */
bool storage_get_prefs(userPrefInfo *out);

/**
 * @brief Set user preferences (update RAM cache, schedule EEPROM write).
 *
 * @param prefs Pointer to new user preferences (not NULL).
 * @return true on success, false on error.
 */
bool storage_set_prefs(const userPrefInfo *prefs);

/**
 * @brief Get relay power-on states (from RAM cache).
 *
 * @param out Pointer to output array (not NULL, must be at least 8 bytes).
 * @return true on success, false on error.
 */
bool storage_get_relay_states(uint8_t *out);

/**
 * @brief Set relay power-on states (update RAM cache, schedule write).
 *
 * @param states Pointer to new relay states array (not NULL).
 * @return true on success, false on error.
 */
bool storage_set_relay_states(const uint8_t *states);

/**
 * @brief Force immediate commit of all pending changes to EEPROM.
 *
 * @param timeout_ms Maximum time to wait for the commit in milliseconds.
 * @return true on success, false on timeout or error.
 */
bool storage_commit_now(uint32_t timeout_ms);

/**
 * @brief Reset all config to factory defaults.
 *
 * @param timeout_ms Maximum time to wait for the operation in milliseconds.
 * @return true on success, false on timeout or error.
 */
bool storage_load_defaults(uint32_t timeout_ms);

/**
 * @brief Get sensor calibration for one channel (from RAM cache).
 *
 * @param channel Channel index (0-7).
 * @param out Pointer to output calibration structure (not NULL).
 * @return true on success, false on error.
 */
bool storage_get_sensor_cal(uint8_t channel, hlw_calib_t *out);

/**
 * @brief Set sensor calibration for one channel (update cache, schedule write).
 *
 * @param channel Channel index (0-7).
 * @param cal Pointer to new calibration structure (not NULL).
 * @return true on success, false on error.
 */
bool storage_set_sensor_cal(uint8_t channel, const hlw_calib_t *cal);

/**
 * @brief Non-blocking enqueue of error/warning codes.
 * These can be safely called from any task context.
 *
 * @param code Error or warning code to enqueue.
 * @return None
 */
void Storage_EnqueueErrorCode(uint16_t code);

/**
 * @brief Non-blocking enqueue of warning codes.
 * These can be safely called from any task context.
 *
 * @param code Warning code to enqueue.
 * @return None
 */
void Storage_EnqueueWarningCode(uint16_t code);

/**
 * @brief Asynchronously dump the error event log region in hex format.
 *
 * @details
 * Enqueues a @ref STORAGE_CMD_DUMP_ERROR_LOG message to StorageTask and
 * returns immediately. StorageTask then prints the error event log region
 * in the same EE_DUMP_START / EE_DUMP_END framed hex format as the full
 * EEPROM dump, but restricted to the configured error log address range.
 *
 * @return true if the request was enqueued, false if storage is not ready or
 *         the queue was full.
 */
bool storage_dump_error_log_async(void);

/**
 * @brief Asynchronously dump the warning event log region in hex format.
 *
 * @details
 * Enqueues a @ref STORAGE_CMD_DUMP_WARNING_LOG message to StorageTask and
 * returns immediately. StorageTask then prints the warning event log region
 * in the same EE_DUMP_START / EE_DUMP_END framed hex format as the full
 * EEPROM dump, but restricted to the configured warning log address range.
 *
 * @return true if the request was enqueued, false if storage is not ready or
 *         the queue was full.
 */
bool storage_dump_warning_log_async(void);

/**
 * @brief Asynchronously clear the error event log region in EEPROM.
 *
 * @details
 * Enqueues a @ref STORAGE_CMD_CLEAR_ERROR_LOG message and returns immediately.
 * StorageTask then erases the corresponding EEPROM region in small chunks,
 * feeding the watchdog and yielding between writes. After this operation the
 * error event log region contains only 0xFF bytes.
 *
 * @return true if the request was enqueued, false if storage is not ready or
 *         the queue was full.
 */
bool storage_clear_error_log_async(void);

/**
 * @brief Asynchronously clear the warning event log region in EEPROM.
 *
 * @details
 * Enqueues a @ref STORAGE_CMD_CLEAR_WARNING_LOG message and returns immediately.
 * StorageTask then erases the corresponding EEPROM region in small chunks,
 * feeding the watchdog and yielding between writes. After this operation the
 * warning event log region contains only 0xFF bytes.
 *
 * @return true if the request was enqueued, false if storage is not ready or
 *         the queue was full.
 */
bool storage_clear_warning_log_async(void);

/* ##################################################################### */
/*                    CHANNEL LABEL API FUNCTIONS                        */
/*        (Declared in channel_labels.h, listed here for reference)      */
/* ##################################################################### */

/* These functions are declared in channel_labels.h:
 *
 * bool storage_get_channel_label(uint8_t channel, char *out, size_t out_len);
 * bool storage_set_channel_label(uint8_t channel, const char *label);
 * bool storage_get_all_channel_labels(char labels[ENERGIS_NUM_CHANNELS][26], size_t
 * label_buf_size);
 */

#endif /* STORAGE_TASK_H */

/** @} */