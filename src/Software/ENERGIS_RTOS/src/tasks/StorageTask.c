/**
 * @file StorageTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 3.0
 * @date 2025-11-14
 *
 * @details
 * StorageTask Architecture:
 * - Owns ALL EEPROM access (only this task touches CAT24C256)
 * - Maintains RAM cache of critical config
 * - Debounces writes (2 second idle period)
 * - Processes requests from q_cfg queue
 * - Delegates EEPROM operations to submodules in storage_submodule/
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* External queue declaration - created by ConsoleTask_Init() */
extern QueueHandle_t q_cfg;

/* ==================== Configuration ==================== */
#define STORAGE_TASK_TAG "[Storage]"
#define STORAGE_TASK_STACK_SIZE 2048
#define STORAGE_TASK_PRIORITY 2
#define STORAGE_DEBOUNCE_MS 2000  /* 2 seconds idle before writing */
#define STORAGE_QUEUE_POLL_MS 100 /* Check queue every 100ms */

/* ==================== Global Handles ==================== */

SemaphoreHandle_t eepromMtx = NULL;
EventGroupHandle_t cfgEvents = NULL;

/* ==================== Default Values ==================== */

/** @brief Default relay status array (all OFF). */
const uint8_t DEFAULT_RELAY_STATUS[8] = {0};

/** @brief Default network configuration (MAC suffix filled at runtime). */
const networkInfo DEFAULT_NETWORK = {
    .ip = ENERGIS_DEFAULT_IP,
    .gw = ENERGIS_DEFAULT_GW,
    .sn = ENERGIS_DEFAULT_SN,
    .mac = {ENERGIS_MAC_PREFIX0, ENERGIS_MAC_PREFIX1, ENERGIS_MAC_PREFIX2, 0x00, 0x00, 0x00},
    .dns = ENERGIS_DEFAULT_DNS,
    .dhcp = ENERGIS_DEFAULT_DHCP};

/** @brief Default user preferences. */
const userPrefInfo DEFAULT_USER_PREFS = {
    .device_name = DEFAULT_NAME, .location = DEFAULT_LOCATION, .temp_unit = 0};

/** @brief Default energy data (placeholder). */
const uint8_t DEFAULT_ENERGY_DATA[64] = {0};

/** @brief Default event log data (placeholder). */
const uint8_t DEFAULT_LOG_DATA[64] = {0};

/* ==================== Private State ==================== */

/** RAM cache (only accessed by StorageTask) */
static storage_cache_t g_cache;

/** Config ready flag */
static volatile bool eth_netcfg_ready = false;

bool Storage_Config_IsReady(void) { return eth_netcfg_ready; }

/* ====================================================================== */
/*                        EEPROM DUMP (utility)                           */
/* ====================================================================== */

/** @brief Internal flag indicating an incremental EEPROM dump is in progress. */
static bool s_dump_active = false;

/** @brief Next EEPROM address to dump (0..CAT24C256_TOTAL_SIZE-1). */
static uint32_t s_dump_next_addr = 0;

/** @brief Completion semaphore for blocking dump callers. */
static SemaphoreHandle_t s_dump_done_sem = NULL;

/**
 * @brief Incremental formatted EEPROM dump worker.
 *
 * Called from @ref StorageTask main loop whenever @ref s_dump_active is true.
 * Prints a small slice of the CAT24C256 contents in 16-byte rows using
 * @ref log_printf_force() so output is not affected by logger mute.
 *
 * The function:
 * - On the very first call (when @ref s_dump_next_addr is 0) prints the header.
 * - Reads a fixed number of rows per invocation to avoid long blocking.
 * - Sends a Health heartbeat for @ref HEALTH_ID_STORAGE on each slice.
 * - On completion prints the EE_DUMP_END marker, pops logger mute, clears
 *   @ref s_dump_active and signals @ref s_dump_done_sem if non-NULL.
 *
 * @note Must only be called from StorageTask context. It acquires and
 *       releases @ref eepromMtx internally with a bounded critical section.
 */
static void storage_dump_eeprom_formatted(void) {
    if (!s_dump_active) {
        return;
    }

    /* Print header once at the very beginning */
    if (s_dump_next_addr == 0) {
        log_printf_force("EE_DUMP_START\r\n");
        log_printf_force("Addr   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F \r\n");
    }

    /* Number of 16-byte lines to emit per slice (keep it tiny to avoid long dt) */
    const uint32_t lines_per_slice = 1U;
    char line[256];
    char field[16];
    uint8_t buffer[16];

    /* EEPROM access is protected by the mutex */
    xSemaphoreTake(eepromMtx, portMAX_DELAY);

    for (uint32_t i = 0; i < lines_per_slice && s_dump_next_addr < CAT24C256_TOTAL_SIZE; i++) {
        uint16_t addr = (uint16_t)s_dump_next_addr;

        /* CAT24C256_ReadBuffer is void, just fill the buffer */
        CAT24C256_ReadBuffer(addr, buffer, sizeof(buffer));

        snprintf(line, sizeof(line), "0x%04X ", addr);
        for (uint8_t b = 0; b < 16; b++) {
            snprintf(field, sizeof(field), "%02X ", buffer[b]);
            strncat(line, field, sizeof(line) - strlen(line) - 1);
            Health_Heartbeat(HEALTH_ID_STORAGE);
        }

        log_printf_force("%s\r\n", line);
        s_dump_next_addr += 16U;
    }

    xSemaphoreGive(eepromMtx);

    /* Explicit heartbeat while doing SIL dump work so Health doesn't mark us stale */
    Health_Heartbeat(HEALTH_ID_STORAGE);

    /* Finished the last slice */
    if (s_dump_next_addr >= CAT24C256_TOTAL_SIZE) {
        log_printf_force("EE_DUMP_END\r\n");
        Logger_MutePop();
        s_dump_active = false;

        if (s_dump_done_sem != NULL) {
            xSemaphoreGive(s_dump_done_sem);
            s_dump_done_sem = NULL;
        }
    }

    /* Short yield so other tasks (Meter/Net/Button/Console) get CPU time */
    vTaskDelay(pdMS_TO_TICKS(1));
}

/**
 * @brief Trigger a formatted EEPROM dump asynchronously.
 *
 * Enqueues a @ref STORAGE_CMD_DUMP_FORMATTED message and returns immediately
 * without waiting for completion. The dump is executed by StorageTask in the
 * background, and log output is generated from there.
 *
 * @return true if the request was enqueued, false if the queue was full or
 *         storage is not ready.
 */
bool storage_dump_formatted_async(void) {
    if (!eth_netcfg_ready)
        return false;

    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_DUMP_FORMATTED;
    msg.output_ptr = NULL;
    msg.done_sem = NULL;

    if (xQueueSend(q_cfg, &msg, 0) != pdPASS) {
        return false;
    }

    return true;
}

/* ====================================================================== */
/*                        RTOS STORAGE TASK                              */
/* ====================================================================== */

/**
 * @brief Load all config from EEPROM to RAM cache.
 *
 * Called once during task startup to populate RAM cache with stored config.
 * Uses submodule functions to load each section.
 */
static void load_config_from_eeprom(void) {
    xSemaphoreTake(eepromMtx, portMAX_DELAY);

    /* Load network config (handles MAC repair internally) */
    g_cache.network = LoadUserNetworkConfig();

    /* Load user preferences (handles defaults internally) */
    g_cache.preferences = LoadUserPreferences();

    /* Load relay states */
    if (EEPROM_ReadUserOutput(g_cache.relay_states, sizeof(g_cache.relay_states)) != 0) {
        WARNING_PRINT("%s Failed to read relay states, using defaults\r\n", STORAGE_TASK_TAG);
        memcpy(g_cache.relay_states, DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS));
    }

    /* Load sensor calibration for all channels */
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (EEPROM_ReadSensorCalibrationForChannel(ch, &g_cache.sensor_cal[ch]) != 0) {
            WARNING_PRINT("%s Failed to read calibration for CH%d\r\n", STORAGE_TASK_TAG, ch);
        }
    }

    xSemaphoreGive(eepromMtx);

    /* Clear dirty flags */
    g_cache.network_dirty = false;
    g_cache.prefs_dirty = false;
    g_cache.relay_dirty = false;
    for (uint8_t i = 0; i < 8; i++) {
        g_cache.sensor_cal_dirty[i] = false;
    }
    g_cache.last_change_tick = 0;

    INFO_PRINT("%s Config loaded from EEPROM\r\n", STORAGE_TASK_TAG);
}

/**
 * @brief Commit all dirty sections to EEPROM.
 *
 * Checks dirty flags and writes changed sections to EEPROM using submodule functions.
 * Optionally triggers reboot if STORAGE_REBOOT_ON_CONFIG_SAVE is enabled.
 */
static void commit_dirty_sections(void) {
#ifndef STORAGE_REBOOT_ON_CONFIG_SAVE
#define STORAGE_REBOOT_ON_CONFIG_SAVE 0
#endif

    xSemaphoreTake(eepromMtx, portMAX_DELAY);

    /* Commit network config if dirty */
    if (g_cache.network_dirty) {
        if (EEPROM_WriteUserNetworkWithChecksum(&g_cache.network) == 0) {
            g_cache.network_dirty = false;
            ECHO("%s Network config committed\r\n", STORAGE_TASK_TAG);
#if STORAGE_REBOOT_ON_CONFIG_SAVE
            vTaskDelay(pdMS_TO_TICKS(100));
            Health_RebootNow("Settings applied");
#endif
        } else {
            ERROR_PRINT("%s Failed to commit network config\r\n", STORAGE_TASK_TAG);
        }
    }

    /* Commit user prefs if dirty */
    if (g_cache.prefs_dirty) {
        if (EEPROM_WriteUserPrefsWithChecksum(&g_cache.preferences) == 0) {
            g_cache.prefs_dirty = false;
            ECHO("%s User prefs committed\r\n", STORAGE_TASK_TAG);
#if STORAGE_REBOOT_ON_CONFIG_SAVE
            vTaskDelay(pdMS_TO_TICKS(100));
            Health_RebootNow("Settings applied");
#endif
        } else {
            ERROR_PRINT("%s Failed to commit user prefs\r\n", STORAGE_TASK_TAG);
        }
    }

    /* Commit relay states if dirty */
    if (g_cache.relay_dirty) {
        if (EEPROM_WriteUserOutput(g_cache.relay_states, sizeof(g_cache.relay_states)) == 0) {
            g_cache.relay_dirty = false;
            ECHO("%s Relay states committed\r\n", STORAGE_TASK_TAG);
        } else {
            ERROR_PRINT("%s Failed to commit relay states\r\n", STORAGE_TASK_TAG);
        }
    }

    /* Commit sensor calibration if any channel is dirty */
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (g_cache.sensor_cal_dirty[ch]) {
            if (EEPROM_WriteSensorCalibrationForChannel(ch, &g_cache.sensor_cal[ch]) == 0) {
                g_cache.sensor_cal_dirty[ch] = false;
                ECHO("%s Sensor cal CH%d committed\r\n", STORAGE_TASK_TAG, ch);
            } else {
                ERROR_PRINT("%s Failed to commit sensor cal CH%d\r\n", STORAGE_TASK_TAG, ch);
            }
        }
    }

    xSemaphoreGive(eepromMtx);
}

/**
 * @brief Process storage request message.
 *
 * Handles all storage commands by updating RAM cache, setting dirty flags,
 * or directly accessing EEPROM (for channel labels and testing).
 *
 * @param msg Pointer to storage message structure
 */
static void process_storage_msg(const storage_msg_t *msg) {
    if (!msg)
        return;

    switch (msg->cmd) {
    /* Network config operations */
    case STORAGE_CMD_READ_NETWORK:
        if (msg->output_ptr) {
            memcpy(msg->output_ptr, &g_cache.network, sizeof(networkInfo));
        }
        break;

    case STORAGE_CMD_WRITE_NETWORK:
        memcpy(&g_cache.network, &msg->data.net_info, sizeof(networkInfo));
        g_cache.network_dirty = true;
        g_cache.last_change_tick = xTaskGetTickCount();
        break;

    /* User preferences operations */
    case STORAGE_CMD_READ_PREFS:
        if (msg->output_ptr) {
            memcpy(msg->output_ptr, &g_cache.preferences, sizeof(userPrefInfo));
        }
        break;

    case STORAGE_CMD_WRITE_PREFS:
        memcpy(&g_cache.preferences, &msg->data.user_prefs, sizeof(userPrefInfo));
        g_cache.prefs_dirty = true;
        g_cache.last_change_tick = xTaskGetTickCount();
        break;

    /* Relay state operations */
    case STORAGE_CMD_READ_RELAY_STATES:
        if (msg->output_ptr) {
            memcpy(msg->output_ptr, g_cache.relay_states, 8);
        }
        break;

    case STORAGE_CMD_WRITE_RELAY_STATES:
        memcpy(g_cache.relay_states, msg->data.relay.states, 8);
        g_cache.relay_dirty = true;
        g_cache.last_change_tick = xTaskGetTickCount();
        break;

    /* Sensor calibration operations */
    case STORAGE_CMD_READ_SENSOR_CAL:
        if (msg->data.sensor_cal.channel < 8 && msg->output_ptr) {
            memcpy(msg->output_ptr, &g_cache.sensor_cal[msg->data.sensor_cal.channel],
                   sizeof(hlw_calib_t));
        }
        break;

    case STORAGE_CMD_WRITE_SENSOR_CAL:
        if (msg->data.sensor_cal.channel < 8) {
            memcpy(&g_cache.sensor_cal[msg->data.sensor_cal.channel], &msg->data.sensor_cal.calib,
                   sizeof(hlw_calib_t));
            g_cache.sensor_cal_dirty[msg->data.sensor_cal.channel] = true;
            g_cache.last_change_tick = xTaskGetTickCount();
        }
        break;

    /* Channel label operations (direct EEPROM access, no caching) */
    case STORAGE_CMD_READ_CHANNEL_LABEL:
        if (msg->data.ch_label.channel < 8 && msg->output_ptr) {
            xSemaphoreTake(eepromMtx, portMAX_DELAY);
            EEPROM_ReadChannelLabel(msg->data.ch_label.channel, (char *)msg->output_ptr, 64);
            xSemaphoreGive(eepromMtx);
        }
        break;

    case STORAGE_CMD_WRITE_CHANNEL_LABEL:
        if (msg->data.ch_label.channel < 8) {
            xSemaphoreTake(eepromMtx, portMAX_DELAY);
            EEPROM_WriteChannelLabel(msg->data.ch_label.channel, msg->data.ch_label.label);
            xSemaphoreGive(eepromMtx);
        }
        break;

    /* Commit and defaults operations */
    case STORAGE_CMD_COMMIT:
        commit_dirty_sections();
        break;

    case STORAGE_CMD_LOAD_DEFAULTS:
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        EEPROM_WriteFactoryDefaults();
        xSemaphoreGive(eepromMtx);
        load_config_from_eeprom();
        break;

    /* SIL testing operations: incremental formatted dump */
    case STORAGE_CMD_DUMP_FORMATTED:
        if (s_dump_active) {
            WARNING_PRINT("%s EEPROM dump already in progress\r\n", STORAGE_TASK_TAG);
            if (msg->done_sem) {
                xSemaphoreGive(msg->done_sem);
            }
        } else {
            /* Start incremental dump: remember completion semaphore and mute logger */
            s_dump_active = true;
            s_dump_next_addr = 0U;
            s_dump_done_sem = msg->done_sem;

            Logger_MutePush();
            /* First slice (header) will be emitted from StorageTask loop */
        }
        /* For the dump we do NOT signal done_sem here; completion is signaled
         * from the incremental worker when EE_DUMP_END is printed. */
        return;

    case STORAGE_CMD_SELF_TEST:
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        bool result = CAT24C256_SelfTest(msg->data.sil_test.test_addr);
        if (msg->data.sil_test.result) {
            *msg->data.sil_test.result = result;
        }
        xSemaphoreGive(eepromMtx);
        break;

    default:
        WARNING_PRINT("%s Unknown command: %d\r\n", STORAGE_TASK_TAG, msg->cmd);
        break;
    }

    /* Signal completion if semaphore provided (synchronous commands only) */
    if (msg->done_sem) {
        xSemaphoreGive(msg->done_sem);
    }
}

/**
 * @brief Storage task main function.
 *
 * Initialization sequence:
 * 1. Check/write factory defaults (first boot)
 * 2. Load config from EEPROM to RAM cache
 * 3. Signal CFG_READY
 *
 * Main loop:
 * - Process storage messages from q_cfg queue
 * - Auto-commit dirty sections after debounce period
 * - Send periodic heartbeat
 * - Drive incremental EEPROM dump when @ref s_dump_active is true
 *
 * @param arg Unused task parameter
 */
static void StorageTask(void *arg) {
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(1000));
    ECHO("%s Task started\r\n", STORAGE_TASK_TAG);

    /* Check and write factory defaults if first boot */
    check_factory_defaults();

    /* Load all config from EEPROM to RAM cache */
    load_config_from_eeprom();

    /* Signal that config is ready */
    eth_netcfg_ready = true;
    xEventGroupSetBits(cfgEvents, CFG_READY_BIT);
    INFO_PRINT("%s Config ready\r\n", STORAGE_TASK_TAG);

    storage_msg_t msg;
    const TickType_t poll_ticks = pdMS_TO_TICKS(STORAGE_QUEUE_POLL_MS);

    static uint32_t hb_stor_ms = 0;

    /* Main task loop */
    for (;;) {
        /* Send heartbeat (baseline) */
        uint32_t __now = to_ms_since_boot(get_absolute_time());
        if ((__now - hb_stor_ms) >= STORAGETASKBEAT_MS) {
            hb_stor_ms = __now;
            Health_Heartbeat(HEALTH_ID_STORAGE);
        }

        /* While dumping, poll the queue more often to stay responsive */
        TickType_t wait_ticks = s_dump_active ? pdMS_TO_TICKS(10) : poll_ticks;

        /* Process queue messages */
        if (xQueueReceive(q_cfg, &msg, wait_ticks) == pdPASS) {
            process_storage_msg(&msg);
        }

        /* Auto-commit after debounce period */
        if (g_cache.last_change_tick != 0) {
            TickType_t now = xTaskGetTickCount();
            if ((now - g_cache.last_change_tick) >= pdMS_TO_TICKS(STORAGE_DEBOUNCE_MS)) {
                commit_dirty_sections();
                g_cache.last_change_tick = 0;
            }
        }

        /* Drive incremental EEPROM dump, if one is active */
        if (s_dump_active) {
            storage_dump_eeprom_formatted();
        }
    }
}

/* ====================================================================== */
/*                       PUBLIC API FUNCTIONS                            */
/* ====================================================================== */

/* ********************************************************************** */
/*                    EEPROM_EraseAll (utility)                           */
/* ********************************************************************** */

/**
 * @brief Erase entire EEPROM to 0xFF.
 *
 * Writes 0xFF to all EEPROM addresses in 32-byte chunks.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @return 0 on success, -1 on failure
 */
int EEPROM_EraseAll(void) {
    uint8_t blank[32];
    memset(blank, 0xFF, sizeof(blank));

    for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += sizeof(blank)) {
        if (CAT24C256_WriteBuffer(addr, blank, sizeof(blank)) != 0) {
            return -1;
        }
    }
    return 0;
}

/**
 * @brief Erase the entire EEPROM via the Storage subsystem.
 *
 * This helper acquires the EEPROM mutex, invokes EEPROM_EraseAll(), and releases
 * the mutex again. Erase is performed in 32-byte chunks with write-cycle delays
 * inside CAT24C256_WriteBuffer(), so other tasks continue to run and the watchdog
 * is periodically fed.
 *
 * @param timeout_ms Maximum time to wait for the EEPROM mutex in milliseconds.
 *                   If 0, a default of 30000 ms is used.
 *
 * @return true if the erase completed successfully, false on timeout or driver error.
 */
bool storage_erase_all(uint32_t timeout_ms) {
    if (!eth_netcfg_ready)
        return false;

    if (timeout_ms == 0) {
        timeout_ms = 30000;
    }

    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return false;
    }

    int res = EEPROM_EraseAll();
    xSemaphoreGive(eepromMtx);

    return (res == 0);
}

/**
 * @brief Initialize and start the Storage task with a deterministic enable gate.
 *
 * Creates mutex, event group, and task. Waits for Console readiness before proceeding.
 *
 * @param enable Gate that allows or skips starting this subsystem
 */
void StorageTask_Init(bool enable) {
    /* TU-local READY flag accessor (no file-scope globals added). */
    static volatile bool ready_val = false;
#define STORAGE_READY() (ready_val)

    STORAGE_READY() = false;

    if (!enable) {
        return;
    }

    /* Gate on Console readiness deterministically */
    extern bool Console_IsReady(void);
    TickType_t const t0 = xTaskGetTickCount();
    TickType_t const deadline = t0 + pdMS_TO_TICKS(5000);
    while (!Console_IsReady() && xTaskGetTickCount() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Create mutex and event group */
    extern void StorageTask(void *arg);
    eepromMtx = xSemaphoreCreateMutex();
    cfgEvents = xEventGroupCreate();

    if (!eepromMtx || !cfgEvents) {
        ERROR_PRINT("%s Failed to create mutex/events\r\n", STORAGE_TASK_TAG);
        return;
    }

    /* Create task */
    if (xTaskCreate(StorageTask, "Storage", STORAGE_TASK_STACK_SIZE, NULL, STORAGE_TASK_PRIORITY,
                    NULL) != pdPASS) {
        ERROR_PRINT("%s Failed to create task\r\n", STORAGE_TASK_TAG);
        return;
    }

    INFO_PRINT("%s Task initialized\r\n", STORAGE_TASK_TAG);
    STORAGE_READY() = true;
}

/**
 * @brief Storage subsystem readiness query (configuration loaded).
 *
 * Returns true once StorageTask has signaled CFG_READY in cfgEvents.
 *
 * @return true if configuration is ready, false otherwise
 */
bool Storage_IsReady(void) {
    /* Fast path: once StorageTask finished boot load it sets this flag. */
    extern volatile bool eth_netcfg_ready;
    if (eth_netcfg_ready) {
        return true;
    }

    /* Fallback to event bit (in case other modules rely on it). */
    extern EventGroupHandle_t cfgEvents;
    if (cfgEvents) {
        EventBits_t bits = xEventGroupGetBits(cfgEvents);
        if ((bits & CFG_READY_BIT) != 0) {
            return true;
        }
    }
    return false;
}

bool storage_wait_ready(uint32_t timeout_ms) {
    EventBits_t bits =
        xEventGroupWaitBits(cfgEvents, CFG_READY_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (bits & CFG_READY_BIT) != 0;
}

bool storage_get_network(networkInfo *out) {
    if (!out || !eth_netcfg_ready)
        return false;

    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (!done)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_READ_NETWORK, .output_ptr = out, .done_sem = done};

    bool queued = (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS);
    bool ok = queued && (xSemaphoreTake(done, pdMS_TO_TICKS(1000)) == pdPASS);
    vSemaphoreDelete(done);
    return ok;
}

bool storage_set_network(const networkInfo *net) {
    if (!net || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_WRITE_NETWORK, .done_sem = NULL};
    memcpy(&msg.data.net_info, net, sizeof(networkInfo));

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

bool storage_get_prefs(userPrefInfo *out) {
    if (!out || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_READ_PREFS, .output_ptr = out, .done_sem = NULL};

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

bool storage_set_prefs(const userPrefInfo *prefs) {
    if (!prefs || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_WRITE_PREFS, .done_sem = NULL};
    memcpy(&msg.data.user_prefs, prefs, sizeof(userPrefInfo));

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

bool storage_get_relay_states(uint8_t *out) {
    if (!out || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_READ_RELAY_STATES, .output_ptr = out, .done_sem = NULL};

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

bool storage_set_relay_states(const uint8_t *states) {
    if (!states || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_WRITE_RELAY_STATES, .done_sem = NULL};
    memcpy(msg.data.relay.states, states, 8);

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

bool storage_commit_now(uint32_t timeout_ms) {
    if (!eth_netcfg_ready)
        return false;

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    if (!done_sem)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_COMMIT, .done_sem = done_sem};

    if (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(timeout_ms)) != pdPASS) {
        vSemaphoreDelete(done_sem);
        return false;
    }

    bool ok = xSemaphoreTake(done_sem, pdMS_TO_TICKS(timeout_ms)) == pdPASS;
    vSemaphoreDelete(done_sem);
    return ok;
}

bool storage_load_defaults(uint32_t timeout_ms) {
    if (!eth_netcfg_ready)
        return false;

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    if (!done_sem)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_LOAD_DEFAULTS, .done_sem = done_sem};

    if (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(timeout_ms)) != pdPASS) {
        vSemaphoreDelete(done_sem);
        return false;
    }

    bool ok = xSemaphoreTake(done_sem, pdMS_TO_TICKS(timeout_ms)) == pdPASS;
    vSemaphoreDelete(done_sem);
    return ok;
}

bool storage_get_sensor_cal(uint8_t channel, hlw_calib_t *out) {
    if (channel >= 8 || !out || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_READ_SENSOR_CAL, .output_ptr = out, .done_sem = NULL};
    msg.data.sensor_cal.channel = channel;

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

bool storage_set_sensor_cal(uint8_t channel, const hlw_calib_t *cal) {
    if (channel >= 8 || !cal || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_WRITE_SENSOR_CAL, .done_sem = NULL};
    msg.data.sensor_cal.channel = channel;
    memcpy(&msg.data.sensor_cal.calib, cal, sizeof(hlw_calib_t));

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

/**
 * @brief Trigger a formatted EEPROM dump in StorageTask context.
 *
 * Enqueues a @ref STORAGE_CMD_DUMP_FORMATTED request and waits for completion.
 * The dump itself is executed incrementally from @ref StorageTask, so other
 * tasks (Net, Meter, Buttons, Health) remain responsive and watchdog feeding
 * is not impacted. While the dump runs, logger output from other tasks is
 * muted via @ref Logger_MutePush()/Logger_MutePop(), so the hex dump remains
 * clean and contiguous. Critical errors and warnings still use
 * @ref log_printf_force() and will appear.
 *
 * @param timeout_ms Maximum time to wait for the dump to finish. If 0, a
 *                   default of 60000 ms is used.
 *
 * @return true on success, false on timeout or queue/semaphore failure.
 */
bool storage_dump_formatted(uint32_t timeout_ms) {
    if (!Storage_Config_IsReady()) {
        return false;
    }

    if (timeout_ms == 0) {
        timeout_ms = 60000U;
    }

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    if (done_sem == NULL) {
        return false;
    }

    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_DUMP_FORMATTED;
    msg.done_sem = done_sem;

    if (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(timeout_ms)) != pdPASS) {
        vSemaphoreDelete(done_sem);
        return false;
    }

    if (xSemaphoreTake(done_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        vSemaphoreDelete(done_sem);
        return false;
    }

    vSemaphoreDelete(done_sem);
    return true;
}

bool storage_self_test(uint16_t test_addr, uint32_t timeout_ms) {
    if (!eth_netcfg_ready)
        return false;

    bool result = false;
    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    if (!done_sem)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_SELF_TEST, .done_sem = done_sem};
    msg.data.sil_test.test_addr = test_addr;
    msg.data.sil_test.result = &result;

    if (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(timeout_ms)) != pdPASS) {
        vSemaphoreDelete(done_sem);
        return false;
    }

    xSemaphoreTake(done_sem, pdMS_TO_TICKS(timeout_ms));
    vSemaphoreDelete(done_sem);
    return result;
}