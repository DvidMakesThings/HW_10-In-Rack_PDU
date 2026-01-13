/**
 * @file src/tasks/StorageTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks10 10. Storage Task
 * @ingroup tasks
 * @brief Persistent configuration manager with CAT24C256 EEPROM and queue-based access.
 * @{
 *
 * @version 3.1.0
 * @date 2025-01-01
 *
 * @details
 * This module implements a FreeRTOS task for managing all persistent configuration
 * data stored in the CAT24C256 EEPROM. It provides the exclusive interface to EEPROM
 * hardware, maintains RAM caches for frequently-accessed data, and implements
 * write-debouncing to extend EEPROM lifetime.
 *
 * Architecture:
 * - Single-owner model: StorageTask has exclusive access to CAT24C256 EEPROM
 * - Queue-based request processing: all external access via q_cfg message queue
 * - RAM caching: critical config mirrored in RAM for fast access without I2C
 * - Write debouncing: 2-second idle period before committing pending writes
 * - Modular design: each EEPROM section managed by dedicated submodule
 * - Thread-safe: mutex-protected EEPROM access, event-based readiness signaling
 *
 * Key Features:
 * - Network configuration storage (IP, subnet, gateway, DNS, MAC, DHCP mode)
 * - User preferences (device name, location, display settings)
 * - Relay state persistence (startup configuration, user-defined presets)
 * - Channel labels (8×64-character custom names, RAM cached)
 * - Sensor calibration data (HLW8032 voltage/current/power offsets per channel)
 * - Event logging (error and warning ring buffers with timestamps)
 * - Energy monitoring (historical energy consumption logs)
 * - Factory defaults system (first-boot initialization, reset capability)
 * - Device identity (serial number, region, MAC address derivation)
 *
 * Submodules (in storage_submodule/):
 * - storage_common: CRC-16 computation, MAC address utilities, shared types
 * - factory_defaults: First-boot detection and default configuration initialization
 * - user_output: Relay state persistence with preset system (5 presets + startup)
 * - network: Network configuration with CRC validation and fallback defaults
 * - calibration: Per-channel HLW8032 calibration coefficients
 * - energy_monitor: Ring buffer for historical energy consumption tracking
 * - event_log: Dual ring buffers for error and warning event history
 * - user_prefs: Device name, location, and display preference storage
 * - channel_labels: User-defined channel names with RAM cache and lazy loading
 *
 * Message Queue Interface:
 * - Asynchronous requests posted to q_cfg queue from any task
 * - Synchronous completion via optional semaphore in message structure
 * - Command types cover read, write, commit, clear, dump operations
 * - Output pointers allow direct result delivery to requester
 *
 * Write Debouncing:
 * - Dirty flags track sections with pending writes
 * - Timer resets on each modification (2-second idle requirement)
 * - Automatic commit when idle period expires
 * - Manual commit available via STORAGE_CMD_COMMIT
 * - Reduces EEPROM wear during configuration changes
 *
 * Configuration Readiness:
 * - CFG_READY_BIT event flag signals boot configuration loaded
 * - Other tasks wait on storage_wait_ready() before accessing config
 * - Deterministic boot sequencing ensures config availability
 *
 * Error Handling:
 * - Deferred error/warning queues prevent logging deadlocks
 * - EEPROM write failures logged with diagnostic codes
 * - CRC validation detects corruption and triggers fallback defaults
 * - I2C errors logged and retried with exponential backoff
 *
 * EEPROM Memory Layout:
 * - Organized into fixed-offset sections per EEPROM_MemoryMap.h
 * - Each section has dedicated submodule managing layout and CRC
 * - Factory marker region identifies virgin vs programmed devices
 * - Ring buffers use head/tail pointers for circular operation
 *
 * Integration Points:
 * - NetTask: loads network configuration at boot
 * - SwitchTask: applies relay states from startup preset
 * - MeterTask: loads sensor calibration coefficients
 * - HTTP/SNMP handlers: read device name, labels, config for display
 * - ConsoleTask: provides configuration commands and diagnostics
 *
 * @note Only StorageTask should access CAT24C256 hardware; all others use queue API.
 * @note Configuration changes auto-commit after 2-second idle period.
 * @note EEPROM has ~1 million write cycle endurance; debouncing extends lifetime.
 * @note RAM cache reduces I2C traffic for frequently-read configuration.
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
/**
 * @var SemaphoreHandle_t eepromMtx
 * @ingroup tasks10
 */
extern SemaphoreHandle_t eepromMtx;

/** Config ready event group - signals when boot config loaded */
/**
 * @var EventGroupHandle_t cfgEvents
 * @ingroup tasks10
 */
extern EventGroupHandle_t cfgEvents;

/**
 * @brief Queue handles for deferred error/warning logging.
 */
/**
 * @var QueueHandle_t g_errorCodeQueue
 * @ingroup tasks10
 */
extern QueueHandle_t g_errorCodeQueue;
/**
 * @var QueueHandle_t g_warningCodeQueue
 * @ingroup tasks10
 */
extern QueueHandle_t g_warningCodeQueue;

/** Config ready bit flag */
#ifdef __cplusplus
#endif
/**
 * @name Task Flags
 * @{
 */
#define CFG_READY_BIT (1 << 0)
/** @} */

/* ==================== Message Structures ==================== */

/**
 * @enum storage_cmd_t
 * @brief Command types processed by StorageTask via q_cfg.
 * @ingroup tasks10
 * @details
 * - All requests are enqueued and executed by StorageTask (single-owner model).
 * - Write operations are debounced; use `STORAGE_CMD_COMMIT` or `storage_commit_now()`
 *   to force immediate EEPROM writes.
 * - Sections referenced are defined in `EEPROM_MemoryMap.h` (network, prefs, labels, logs).
 */
/** Storage request types */
typedef enum {
    /**
     * @name Configuration Commands
     * @brief Read/write cache operations, commit, and defaults.
     * @{
     */
    STORAGE_CMD_READ_NETWORK,        /**< Read network config to RAM cache */
    STORAGE_CMD_WRITE_NETWORK,       /**< Update network config in RAM, schedule write */
    STORAGE_CMD_READ_PREFS,          /**< Read user preferences to RAM cache */
    STORAGE_CMD_WRITE_PREFS,         /**< Update prefs in RAM, schedule write */
    STORAGE_CMD_READ_RELAY_STATES,   /**< Read relay states to RAM cache */
    STORAGE_CMD_WRITE_RELAY_STATES,  /**< Update relay states in RAM, schedule write */
    STORAGE_CMD_READ_CHANNEL_LABEL,  /**< Read one channel label (from cache) */
    STORAGE_CMD_WRITE_CHANNEL_LABEL, /**< Write one channel label (cache + EEPROM) */
    STORAGE_CMD_COMMIT, /**< Force immediate write of pending changes; see `storage_commit_now()` */
    STORAGE_CMD_LOAD_DEFAULTS, /**< Reset to factory defaults; see `storage_load_defaults()` */
    /** @} */

    /**
     * @name Sensor Calibration
     * @brief Per-channel HLW8032 calibration access.
     * @{
     */
    STORAGE_CMD_READ_SENSOR_CAL,  /**< Read sensor calibration for channel */
    STORAGE_CMD_WRITE_SENSOR_CAL, /**< Write sensor calibration for channel */
    /** @} */

    /**
     * @name Event Logs
     * @brief Error and warning ring buffer operations.
     * @{
     */
    STORAGE_CMD_DUMP_ERROR_LOG,    /**< Dump error event log region */
    STORAGE_CMD_DUMP_WARNING_LOG,  /**< Dump warning event log region */
    STORAGE_CMD_CLEAR_ERROR_LOG,   /**< Clear error event log region */
    STORAGE_CMD_CLEAR_WARNING_LOG, /**< Clear warning event log region */
    /** @} */

    /**
     * @name Maintenance
     * @brief Whole-EEPROM operations with watchdog-safe chunking.
     * @{
     */
    STORAGE_CMD_ERASE_ALL, /**< Incremental full EEPROM erase */
    /** @} */

    /**
     * @name SIL Testing Commands
     * @brief Debug/verification helpers (non-production operations).
     * @{
     */
    STORAGE_CMD_DUMP_FORMATTED, /**< Dump EEPROM in formatted hex (SIL testing) */
    /** @} */

    /**
     * @name User Output Preset Commands
     * @brief Preset management for relay masks and startup selection.
     * @{
     */
    STORAGE_CMD_SAVE_USER_OUTPUT_PRESET,   /**< Save preset (index, name, mask) */
    STORAGE_CMD_DELETE_USER_OUTPUT_PRESET, /**< Delete preset (index) */
    STORAGE_CMD_SET_STARTUP_PRESET,        /**< Set startup preset (index) */
    STORAGE_CMD_CLEAR_STARTUP_PRESET       /**< Clear startup preset */
    /** @} */
} storage_cmd_t;

/**
 * @struct storage_msg_t
 * @brief Storage request message (posted to q_cfg)
 * @ingroup tasks10
 */
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

        /* For user output presets */
        struct {
            uint8_t index;                           /**< Preset index 0-4 */
            char name[USER_OUTPUT_NAME_MAX_LEN + 1]; /**< Preset name */
            uint8_t mask;                            /**< Relay mask */
        } user_output;
        struct {
            uint8_t index; /**< Startup preset index */
        } startup;
    } data;

    /** Optional: pointer to output buffer for read operations */
    void *output_ptr;

    /** Optional: semaphore to signal completion */
    SemaphoreHandle_t done_sem;
} storage_msg_t;

/* ==================== RAM Config Cache ==================== */

/**
 * @struct storage_cache_t
 * @brief RAM cache structure (owned by StorageTask)
 * @ingroup tasks10
 */
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
 * @name Public API
 * @{
 */
/**
 * @brief Trigger a formatted EEPROM dump asynchronously.
 *
 * Enqueues a @ref STORAGE_CMD_DUMP_FORMATTED message and returns immediately
 * without waiting for completion. The dump is executed by StorageTask in the
 * background, and log output is generated from there.
 * @return true if the request was enqueued, false if the queue was full or
 *         storage is not ready.
 */
/** @ingroup tasks10 */
bool storage_dump_formatted_async(void);

/**
 * @brief Start full EEPROM erase (async, watchdog-safe).
 *
 * @return true if erase was started, false if storage not ready or already busy.
 */
/** @ingroup tasks10 */
bool storage_erase_all_async(void);

/**
 * @brief Check if EEPROM erase is in progress.
 *
 * @return true if erase active.
 */
/** @ingroup tasks10 */
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
/** @ingroup tasks10 */
void StorageTask_Init(bool enable);

/* ===== User Output Preset API (routed via StorageTask) ===== */
bool storage_save_preset(uint8_t index, const char *name, uint8_t mask);
bool storage_delete_preset(uint8_t index);
bool storage_set_startup_preset(uint8_t index);
bool storage_clear_startup_preset(void);
/** @ingroup tasks10 */

/**
 * @brief Storage subsystem readiness query (configuration loaded).
 *
 * @details
 * Returns true once StorageTask has signaled CFG_READY in cfgEvents. This directly
 * reflects the config-ready bit instead of relying on any separate latch.
 *
 * @return true if configuration is ready, false otherwise.
 */
/** @ingroup tasks10 */
bool Storage_IsReady(void);

/**
 * @brief Check if config is loaded and ready.
 * @return true if config is ready, false otherwise.
 */
/** @ingroup tasks10 */
bool Storage_Config_IsReady(void);

/**
 * @brief Wait for config to be loaded from EEPROM.
 *
 * @param timeout_ms Maximum time to wait in milliseconds.
 * @return true if config became ready within timeout, false otherwise.
 */
/** @ingroup tasks10 */
bool storage_wait_ready(uint32_t timeout_ms);

/**
 * @brief Get current network config (from RAM cache).
 *
 * @param out Pointer to output structure (not NULL).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_get_network(networkInfo *out);

/**
 * @brief Set network config (update RAM cache, schedule EEPROM write).
 *
 * @param net Pointer to new network config (not NULL).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_set_network(const networkInfo *net);

/**
 * @brief Get current user preferences (from RAM cache).
 *
 * @param out Pointer to output structure (not NULL).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_get_prefs(userPrefInfo *out);

/**
 * @brief Set user preferences (update RAM cache, schedule EEPROM write).
 *
 * @param prefs Pointer to new user preferences (not NULL).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_set_prefs(const userPrefInfo *prefs);

/**
 * @brief Get relay power-on states (from RAM cache).
 *
 * @param out Pointer to output array (not NULL, must be at least 8 bytes).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_get_relay_states(uint8_t *out);

/**
 * @brief Set relay power-on states (update RAM cache, schedule write).
 *
 * @param states Pointer to new relay states array (not NULL).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_set_relay_states(const uint8_t *states);

/**
 * @brief Force immediate commit of all pending changes to EEPROM.
 *
 * @param timeout_ms Maximum time to wait for the commit in milliseconds.
 * @return true on success, false on timeout or error.
 */
/** @ingroup tasks10 */
bool storage_commit_now(uint32_t timeout_ms);

/**
 * @brief Reset all config to factory defaults.
 *
 * @param timeout_ms Maximum time to wait for the operation in milliseconds.
 * @return true on success, false on timeout or error.
 */
/** @ingroup tasks10 */
bool storage_load_defaults(uint32_t timeout_ms);

/**
 * @brief Get sensor calibration for one channel (from RAM cache).
 *
 * @param channel Channel index (0-7).
 * @param out Pointer to output calibration structure (not NULL).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_get_sensor_cal(uint8_t channel, hlw_calib_t *out);

/**
 * @brief Set sensor calibration for one channel (update cache, schedule write).
 *
 * @param channel Channel index (0-7).
 * @param cal Pointer to new calibration structure (not NULL).
 * @return true on success, false on error.
 */
/** @ingroup tasks10 */
bool storage_set_sensor_cal(uint8_t channel, const hlw_calib_t *cal);

/**
 * @brief Non-blocking enqueue of error/warning codes.
 * These can be safely called from any task context.
 *
 * @param code Error or warning code to enqueue.
 * @return None
 */
/** @ingroup tasks10 */
void Storage_EnqueueErrorCode(uint16_t code);

/**
 * @brief Non-blocking enqueue of warning codes.
 * These can be safely called from any task context.
 *
 * @param code Warning code to enqueue.
 * @return None
 */
/** @ingroup tasks10 */
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
/** @ingroup tasks10 */
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
/** @ingroup tasks10 */
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
/** @ingroup tasks10 */
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
/** @ingroup tasks10 */
bool storage_clear_warning_log_async(void);

/** @} */

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