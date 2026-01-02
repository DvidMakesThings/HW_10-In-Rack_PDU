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
#define STORAGE_TASK_TAG "[STORAGE]"

#define STORAGE_TASK_STACK_SIZE 2048
#define STORAGE_TASK_PRIORITY 2
#define STORAGE_DEBOUNCE_MS 2000  /* 2 seconds idle before writing */
#define STORAGE_QUEUE_POLL_MS 100 /* Check queue every 100ms */

#define ERROR_LOG_QUEUE_LENGTH 32
#define WARNING_LOG_QUEUE_LENGTH 32
#define STORAGE_LOG_MAX_WRITES 4 /* max EEPROM writes per loop */

/* ==================== Global Handles ==================== */

SemaphoreHandle_t eepromMtx = NULL;
EventGroupHandle_t cfgEvents = NULL;
QueueHandle_t g_errorCodeQueue = NULL;
QueueHandle_t g_warningCodeQueue = NULL;

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

/* ==================== EEPROM ERASE STATE ==================== */

/** @brief Incremental full EEPROM erase active flag */
static bool s_erase_active = false;

/** @brief Next EEPROM address to erase */
static uint32_t s_erase_next_addr = 0;

#define EEPROM_ERASE_CHUNK_BYTES 32U
#define EEPROM_ERASE_PAGE_DELAY_MS 6U
#define EEPROM_ERASE_PROGRESS_BYTES 1024U

/* ====================================================================== */
/*                        EEPROM DUMP (utility)                           */
/* ====================================================================== */

/** @brief Internal flag indicating an incremental EEPROM dump is in progress. */
static bool s_dump_active = false;

/** @brief Next EEPROM address to dump (0..CAT24C256_TOTAL_SIZE-1). */
static uint32_t s_dump_next_addr = 0;

/** @brief Completion semaphore for blocking dump callers. */
static SemaphoreHandle_t s_dump_done_sem = NULL;

/** @brief Internal flag indicating an incremental error log dump is in progress. */
static bool s_err_dump_active = false;

/** @brief Next offset within the error log region to dump. */
static uint16_t s_err_dump_next_offset = 0;

/** @brief Internal flag indicating an incremental warning log dump is in progress. */
static bool s_warn_dump_active = false;

/** @brief Next offset within the warning log region to dump. */
static uint16_t s_warn_dump_next_offset = 0;

/** @brief Internal flag indicating an incremental error log clear is in progress. */
static bool s_err_clear_active = false;

/** @brief Next offset within the error log region to clear. */
static uint16_t s_err_clear_next_offset = 0;

/** @brief Internal flag indicating an incremental warning log clear is in progress. */
static bool s_warn_clear_active = false;

/** @brief Next offset within the warning log region to clear. */
static uint16_t s_warn_clear_next_offset = 0;

/**
 * @brief Incremental formatted EEPROM dump worker.
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

    /* Process 32 bytes (two lines) per call for better I2C efficiency */
    if (s_dump_next_addr < CAT24C256_TOTAL_SIZE) {
        uint16_t addr = (uint16_t)s_dump_next_addr;
        uint8_t buffer[32];
        uint16_t read_size = 32;

        /* Don't read past end of EEPROM */
        if (s_dump_next_addr + 32 > CAT24C256_TOTAL_SIZE) {
            read_size = CAT24C256_TOTAL_SIZE - s_dump_next_addr;
        }

        /* Acquire mutex only for the actual EEPROM read */
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        CAT24C256_ReadBuffer(addr, buffer, read_size);
        xSemaphoreGive(eepromMtx);

        /* Print two lines from the 32-byte buffer */
        for (uint8_t line_num = 0; line_num < 2 && s_dump_next_addr < CAT24C256_TOTAL_SIZE;
             line_num++) {
            uint16_t line_addr = addr + (line_num * 16);
            uint8_t *line_data = &buffer[line_num * 16];

            /* Format efficiently using pointer arithmetic */
            char line[80];
            char *p = line;
            p += snprintf(p, sizeof(line), "0x%04X ", line_addr);

            uint8_t bytes_in_line = 16;
            if (line_addr + 16 > CAT24C256_TOTAL_SIZE) {
                bytes_in_line = CAT24C256_TOTAL_SIZE - line_addr;
            }

            for (uint8_t b = 0; b < bytes_in_line; b++) {
                p += snprintf(p, sizeof(line) - (p - line), "%02X ", line_data[b]);
            }

            log_printf_force("%s\r\n", line);
            s_dump_next_addr += bytes_in_line;
        }

        /* Critical: Heartbeat after processing the chunk */
        Health_Heartbeat(HEALTH_ID_STORAGE);
    }

    /* Check if dump is complete */
    if (s_dump_next_addr >= CAT24C256_TOTAL_SIZE) {
        log_printf_force("EE_DUMP_END\r\n");
        Logger_MutePop();
        s_dump_active = false;

        if (s_dump_done_sem != NULL) {
            xSemaphoreGive(s_dump_done_sem);
            s_dump_done_sem = NULL;
        }
    }

    /* Yield immediately to let other tasks run */
    vTaskDelay(pdMS_TO_TICKS(1));
}

/**
 * @brief Incrementally dump an event log region in formatted hex.
 *
 * This helper prints the specified EEPROM region in the same format and 16-byte
 * lines as the full EEPROM dump, but limited to a given base address and size.
 *
 * @param base   Start address of the region.
 * @param size   Size in bytes of the region.
 * @param offset [in,out] Current offset within the region (0..size).
 * @param active [in,out] Pointer to active flag; cleared when dump completes.
 */
static void storage_dump_event_region(uint16_t base, uint16_t size, uint16_t *offset,
                                      bool *active) {
    if (!active || !*active || !offset) {
        return;
    }

    /* Print header once at the very beginning of this region */
    if (*offset == 0U) {
        log_printf_force("START\r\n");
        log_printf_force("Addr   00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F \r\n");
    }

    if (*offset >= size) {
        log_printf_force("END\r\n");
        Logger_MutePop();
        *active = false;
        return;
    }

    uint16_t remaining = (uint16_t)(size - *offset);
    uint16_t chunk = (remaining > 32U) ? 32U : remaining;
    uint16_t addr = (uint16_t)(base + *offset);
    uint8_t buffer[32];

    xSemaphoreTake(eepromMtx, portMAX_DELAY);
    CAT24C256_ReadBuffer(addr, buffer, chunk);
    xSemaphoreGive(eepromMtx);

    uint16_t produced = 0;
    while (produced < chunk) {
        uint16_t line_addr = (uint16_t)(addr + produced);
        uint8_t *line_data = &buffer[produced];

        char line[80];
        char *p = line;
        p += snprintf(p, sizeof(line), "0x%04X ", line_addr);

        uint8_t bytes_in_line = (uint8_t)((chunk - produced) > 16U ? 16U : (chunk - produced));
        for (uint8_t b = 0; b < bytes_in_line; b++) {
            p += snprintf(p, (size_t)(sizeof(line) - (size_t)(p - line)), "%02X ", line_data[b]);
        }

        log_printf_force("%s\r\n", line);
        produced = (uint16_t)(produced + bytes_in_line);
    }

    *offset = (uint16_t)(*offset + chunk);

    Health_Heartbeat(HEALTH_ID_STORAGE);
    vTaskDelay(pdMS_TO_TICKS(1));

    if (*offset >= size) {
        log_printf_force("END\r\n");
        Logger_MutePop();
        *active = false;
    }
}

/**
 * @brief Incrementally clear an event log region to 0xFF bytes.
 *
 * The region is erased in small chunks with watchdog-safe yielding and short
 * EEPROM mutex holds so that the operation remains non-blocking for other
 * tasks.
 *
 * @param base   Start address of the region.
 * @param size   Size in bytes of the region.
 * @param offset [in,out] Current offset within the region (0..size).
 * @param active [in,out] Pointer to active flag; cleared when clear completes.
 */
static void storage_clear_event_region(uint16_t base, uint16_t size, uint16_t *offset,
                                       bool *active) {
    if (!active || !*active || !offset) {
        return;
    }

    if (*offset >= size) {
        *active = false;
        return;
    }

    uint8_t blank[32];
    memset(blank, 0xFF, sizeof(blank));

    uint16_t remaining = (uint16_t)(size - *offset);
    uint16_t chunk = (remaining > (uint16_t)sizeof(blank)) ? (uint16_t)sizeof(blank) : remaining;
    uint16_t addr = (uint16_t)(base + *offset);

    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(5)) != pdTRUE) {
        return;
    }

    if (CAT24C256_WriteBuffer(addr, blank, chunk) != 0) {
        ERROR_PRINT("%s Failed to clear event log at 0x%04X\r\n", STORAGE_TASK_TAG, addr);
        xSemaphoreGive(eepromMtx);
        *active = false;
        return;
    }

    xSemaphoreGive(eepromMtx);

    *offset = (uint16_t)(*offset + chunk);

    Health_Heartbeat(HEALTH_ID_STORAGE);
    vTaskDelay(pdMS_TO_TICKS(1));

    if (*offset >= size) {
        INFO_PRINT("%s Event log cleared [0x%04X..0x%04X]\r\n", STORAGE_TASK_TAG, base,
                   (uint16_t)(base + size - 1U));
        *active = false;
    }
}

static void storage_erase_step(void) {
    static uint8_t ff_page[EEPROM_ERASE_CHUNK_BYTES];
    static bool ff_init = false;

    if (!s_erase_active) {
        return;
    }

    if (!ff_init) {
        memset(ff_page, 0xFF, sizeof(ff_page));
        ff_init = true;
    }

    if (s_erase_next_addr >= CAT24C256_TOTAL_SIZE) {
        s_erase_active = false;
        INFO_PRINT("[Storage] Erase complete\r\n");
        return;
    }

    xSemaphoreTake(eepromMtx, portMAX_DELAY);

    int rc = CAT24C256_WriteBuffer((uint16_t)s_erase_next_addr, ff_page, (uint16_t)sizeof(ff_page));

    xSemaphoreGive(eepromMtx);

    if (rc != 0) {
        vTaskDelay(pdMS_TO_TICKS(10));

        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        rc = CAT24C256_WriteBuffer((uint16_t)s_erase_next_addr, ff_page, (uint16_t)sizeof(ff_page));
        xSemaphoreGive(eepromMtx);

        if (rc != 0) {
            ERROR_PRINT("[Storage] Erase failed at 0x%04lX\r\n", (unsigned long)s_erase_next_addr);
            s_erase_active = false;
            return;
        }
    }

    s_erase_next_addr += sizeof(ff_page);

    if ((s_erase_next_addr % EEPROM_ERASE_PROGRESS_BYTES) == 0u) {
        INFO_PRINT("[Storage] Erased %lu/%u bytes\r\n", (unsigned long)s_erase_next_addr,
                   CAT24C256_TOTAL_SIZE);
    }

    vTaskDelay(pdMS_TO_TICKS(EEPROM_ERASE_PAGE_DELAY_MS));
}

/* ====================================================================== */
/*                        RTOS STORAGE TASK                              */
/* ====================================================================== */

/**
 * @brief Flush queued error and warning codes into EEPROM event logs.
 *
 * @details
 * This function is invoked periodically by @ref StorageTask to persist
 * error and warning codes that have been enqueued during runtime into their
 * respective EEPROM circular buffers. Each entry is written as a 16-bit
 * event code in big-endian format.
 *
 * To prevent repetitive flooding of the failure memory by the same error
 * code (e.g., when a function repeatedly detects the same fault in a loop),
 * this implementation tracks the last stored error and warning code and
 * skips consecutive duplicates. This ensures that the event log records
 * unique error transitions rather than continuous repetitions.
 *
 * Operation is non-blocking and watchdog-safe:
 * - Each EEPROM transaction is performed under @ref eepromMtx protection.
 * - Only a limited number of writes per call are allowed.
 * - The function yields periodically to feed the watchdog.
 *
 * @note
 * The ring buffer layout in EEPROM:
 * - [0..1]  : 16-bit write pointer (entry index)
 * - [2..N]  : Event entries (16-bit each, big-endian)
 *
 * @note
 * Only the first occurrence of an identical code (after a different one)
 * is written. Subsequent identical codes are ignored until a new code
 * appears. Both error and warning queues are handled independently.
 */

static void Storage_FlushEventQueues(void) {
    uint16_t code;
    uint16_t writes = 0;

    static uint16_t last_err_code = 0;
    static bool last_err_valid = false;
    static uint16_t last_warn_code = 0;
    static bool last_warn_valid = false;

    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(5)) != pdTRUE)
        return;

    /* ---- Process Error Queue ---- */
    while (writes < STORAGE_LOG_MAX_WRITES && g_errorCodeQueue &&
           xQueueReceive(g_errorCodeQueue, &code, 0) == pdTRUE) {

        if (last_err_valid && code == last_err_code)
            continue;

        /* Convert to big-endian before writing */
        uint8_t bytes[2];
        bytes[0] = (uint8_t)((code >> 8) & 0xFF);
        bytes[1] = (uint8_t)(code & 0xFF);

        (void)EEPROM_AppendErrorCode(bytes);
        last_err_code = code;
        last_err_valid = true;

        writes++;
        Health_Heartbeat(HEALTH_ID_STORAGE);
    }

    /* ---- Process Warning Queue ---- */
    while (writes < STORAGE_LOG_MAX_WRITES && g_warningCodeQueue &&
           xQueueReceive(g_warningCodeQueue, &code, 0) == pdTRUE) {

        if (last_warn_valid && code == last_warn_code)
            continue;

        uint8_t bytes[2];
        bytes[0] = (uint8_t)((code >> 8) & 0xFF);
        bytes[1] = (uint8_t)(code & 0xFF);

        (void)EEPROM_AppendWarningCode(bytes);
        last_warn_code = code;
        last_warn_valid = true;

        writes++;
        Health_Heartbeat(HEALTH_ID_STORAGE);
    }

    xSemaphoreGive(eepromMtx);
}

/**
 * @brief Load all config from EEPROM to RAM cache.
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
    /* Load channel labels into RAM cache */
    ChannelLabels_LoadFromEEPROM();

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
 * Checks dirty flags and writes changed sections to EEPROM using submodule functions.
 * Optionally triggers reboot if STORAGE_REBOOT_ON_CONFIG_SAVE is enabled.
 */
static void commit_dirty_sections(void) {
#ifndef STORAGE_REBOOT_ON_CONFIG_SAVE
#define STORAGE_REBOOT_ON_CONFIG_SAVE 1
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
 * Handles all storage commands by updating RAM cache, setting dirty flags,
 * or directly accessing EEPROM (for channel labels and testing).
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
        if (msg->done_sem) {
            xSemaphoreGive(msg->done_sem);
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

    /* Channel label operations (RAM cached) */
    case STORAGE_CMD_READ_CHANNEL_LABEL:
        if (msg->data.ch_label.channel < 8 && msg->output_ptr) {
            /* Read from RAM cache - no EEPROM access needed */
            ChannelLabels_GetCached(msg->data.ch_label.channel, (char *)msg->output_ptr, 64);
        }
        break;

    case STORAGE_CMD_WRITE_CHANNEL_LABEL:
        if (msg->data.ch_label.channel < 8) {
            xSemaphoreTake(eepromMtx, portMAX_DELAY);
            /* Updates RAM cache and writes to EEPROM */
            ChannelLabels_SetAndWrite(msg->data.ch_label.channel, msg->data.ch_label.label);
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

    /* SIL testing operations: incremental formatted dump (full EEPROM) */
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
        /* For the dump, do NOT signal done_sem here; completion is signaled
         * from the incremental worker when EE_DUMP_END is printed. */
        return;

    /* Event log dump operations (async, non-blocking) */
    case STORAGE_CMD_DUMP_ERROR_LOG:
        if (s_err_dump_active) {
            WARNING_PRINT("%s Error log dump already in progress\r\n", STORAGE_TASK_TAG);
        } else {
            s_err_dump_active = true;
            s_err_dump_next_offset = 0U;
            Logger_MutePush();
        }
        return;

    case STORAGE_CMD_DUMP_WARNING_LOG:
        if (s_warn_dump_active) {
            WARNING_PRINT("%s Warning log dump already in progress\r\n", STORAGE_TASK_TAG);
        } else {
            s_warn_dump_active = true;
            s_warn_dump_next_offset = 0U;
            Logger_MutePush();
        }
        return;

    /* Event log clear operations (async, non-blocking) */
    case STORAGE_CMD_CLEAR_ERROR_LOG:
        if (s_err_clear_active) {
            WARNING_PRINT("%s Error log clear already in progress\r\n", STORAGE_TASK_TAG);
        } else {
            s_err_clear_active = true;
            s_err_clear_next_offset = 0U;
        }
        break;

    case STORAGE_CMD_CLEAR_WARNING_LOG:
        if (s_warn_clear_active) {
            WARNING_PRINT("%s Warning log clear already in progress\r\n", STORAGE_TASK_TAG);
        } else {
            s_warn_clear_active = true;
            s_warn_clear_next_offset = 0U;
        }
        break;

    case STORAGE_CMD_ERASE_ALL:
        if (!s_dump_active && !s_err_dump_active && !s_warn_dump_active && !s_err_clear_active &&
            !s_warn_clear_active && !s_erase_active) {

            s_erase_active = true;
            s_erase_next_addr = 0;

            INFO_PRINT("[Storage] Erasing EEPROM (%u bytes)...\r\n", CAT24C256_TOTAL_SIZE);
        }
        break;

    /* ===== User Output Preset Commands ===== */
    case STORAGE_CMD_SAVE_USER_OUTPUT_PRESET: {
        uint8_t idx = msg->data.user_output.index;
        const char *name = msg->data.user_output.name;
        uint8_t mask = msg->data.user_output.mask;
        (void)UserOutput_SavePreset(idx, name, mask);
        break;
    }
    case STORAGE_CMD_DELETE_USER_OUTPUT_PRESET: {
        uint8_t idx = msg->data.user_output.index;
        (void)UserOutput_DeletePreset(idx);
        break;
    }
    case STORAGE_CMD_SET_STARTUP_PRESET: {
        uint8_t idx = msg->data.startup.index;
        (void)UserOutput_SetStartupPreset(idx);
        break;
    }
    case STORAGE_CMD_CLEAR_STARTUP_PRESET: {
        (void)UserOutput_ClearStartupPreset();
        break;
    }

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
 * - Flush queued error/warning codes to EEPROM (bounded)
 * - Drive incremental EEPROM dump when @ref s_dump_active is true
 */
static void StorageTask(void *arg) {
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(1000));
    ECHO("%s Task started\r\n", STORAGE_TASK_TAG);

    /* Check and write factory defaults if first boot */
    check_factory_defaults();

    /* Load all config from EEPROM to RAM cache */
    load_config_from_eeprom();

    /* Initialize user output presets subsystem */
    if (!UserOutput_Init()) {
        WARNING_PRINT("%s UserOutput init failed, presets unavailable\r\n", STORAGE_TASK_TAG);
    }

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

        /* While any long-running operation is active, poll the queue more often */
        TickType_t wait_ticks = (s_dump_active || s_err_dump_active || s_warn_dump_active ||
                                 s_err_clear_active || s_warn_clear_active || s_erase_active)
                                    ? pdMS_TO_TICKS(10)
                                    : poll_ticks;

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

        /* Flush queued error/warning codes to EEPROM without blocking */
        Storage_FlushEventQueues();

        /* Drive incremental full EEPROM dump, if one is active */
        if (s_dump_active) {
            storage_dump_eeprom_formatted();
        }

        /* Drive incremental full EEPROM erase, if active */
        if (s_erase_active) {
            storage_erase_step();
        }

        /* Drive incremental error/warning log dumps, if active */
        if (s_err_dump_active) {
            storage_dump_event_region(EEPROM_EVENT_ERR_START, EEPROM_EVENT_ERR_SIZE,
                                      &s_err_dump_next_offset, &s_err_dump_active);
        }
        if (s_warn_dump_active) {
            storage_dump_event_region(EEPROM_EVENT_WARN_START, EEPROM_EVENT_WARN_SIZE,
                                      &s_warn_dump_next_offset, &s_warn_dump_active);
        }

        /* Drive incremental error/warning log clears, if active */
        if (s_err_clear_active) {
            storage_clear_event_region(EEPROM_EVENT_ERR_START, EEPROM_EVENT_ERR_SIZE,
                                       &s_err_clear_next_offset, &s_err_clear_active);
        }
        if (s_warn_clear_active) {
            storage_clear_event_region(EEPROM_EVENT_WARN_START, EEPROM_EVENT_WARN_SIZE,
                                       &s_warn_clear_next_offset, &s_warn_clear_active);
        }
    }
}

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

/* ********************************************************************** */
/*                        EEPROM DUMP (utility)                           */
/* ********************************************************************** */
/**
 * @brief Trigger a formatted EEPROM dump asynchronously.
 * Enqueues a @ref STORAGE_CMD_DUMP_FORMATTED message and returns immediately
 * without waiting for completion. The dump is executed by StorageTask in the
 * background, and log output is generated from there.
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

/**
 * @brief Asynchronously dump the error event log region in hex format.
 */
bool storage_dump_error_log_async(void) {
    if (!Storage_Config_IsReady()) {
        return false;
    }

    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_DUMP_ERROR_LOG;
    msg.output_ptr = NULL;
    msg.done_sem = NULL;

    if (xQueueSend(q_cfg, &msg, 0) != pdPASS) {
        return false;
    }

    return true;
}

/**
 * @brief Asynchronously dump the warning event log region in hex format.
 */
bool storage_dump_warning_log_async(void) {
    if (!Storage_Config_IsReady()) {
        return false;
    }

    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_DUMP_WARNING_LOG;
    msg.output_ptr = NULL;
    msg.done_sem = NULL;

    if (xQueueSend(q_cfg, &msg, 0) != pdPASS) {
        return false;
    }

    return true;
}

/**
 * @brief Asynchronously clear the error event log region.
 */
bool storage_clear_error_log_async(void) {
    if (!Storage_Config_IsReady()) {
        return false;
    }

    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_CLEAR_ERROR_LOG;
    msg.output_ptr = NULL;
    msg.done_sem = NULL;

    if (xQueueSend(q_cfg, &msg, 0) != pdPASS) {
        return false;
    }

    return true;
}

/**
 * @brief Asynchronously clear the warning event log region.
 */
bool storage_clear_warning_log_async(void) {
    if (!Storage_Config_IsReady()) {
        return false;
    }

    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_CLEAR_WARNING_LOG;
    msg.output_ptr = NULL;
    msg.done_sem = NULL;

    if (xQueueSend(q_cfg, &msg, 0) != pdPASS) {
        return false;
    }

    return true;
}

/* ********************************************************************** */
/*                    EEPROM_EraseAll (utility)                           */
/* ********************************************************************** */

/**
 * @brief Erase entire EEPROM to 0xFF with watchdog-safe yielding.
 * Writes 0xFF to all EEPROM addresses in 32-byte chunks with periodic yields.
 */
int EEPROM_EraseAll(void) {
    uint8_t blank[32];
    memset(blank, 0xFF, sizeof(blank));

    uint32_t chunk_count = 0;
    const uint32_t total_chunks = EEPROM_SIZE / sizeof(blank);

    INFO_PRINT("[Storage] Erasing EEPROM (%u bytes)...\r\n", EEPROM_SIZE);

    for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += sizeof(blank)) {
        if (CAT24C256_WriteBuffer(addr, blank, sizeof(blank)) != 0) {
            ERROR_PRINT("[Storage] Erase failed at 0x%04X\r\n", addr);
            return -1;
        }

        chunk_count++;

        /* Yield every 4 chunks (128 bytes) to allow other tasks to run */
        if ((chunk_count & 0x03) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            Health_Heartbeat(HEALTH_ID_STORAGE);
        }

        /* Every 32 chunks (1KB), report progress and take longer break */
        if ((chunk_count & 0x1F) == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            INFO_PRINT("[Storage] Erased %lu/%u bytes\r\n", (unsigned long)addr + sizeof(blank),
                       EEPROM_SIZE);
        }
    }

    INFO_PRINT("[Storage] Erase complete\r\n");
    return 0;
}

/**
 * @brief Asynchronously erase the entire EEPROM.
 * Enqueues a @ref STORAGE_CMD_ERASE_ALL message to trigger the operation
 * in StorageTask. Returns immediately without waiting for completion.
 * @return true if the erase command was successfully enqueued, false otherwise.
 */
bool storage_erase_all_async(void) {
    storage_msg_t msg;

    if (!Storage_IsReady()) {
        return false;
    }

    if (s_erase_active) {
        return false;
    }

    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_ERASE_ALL;

    return (xQueueSend(q_cfg, &msg, 0) == pdPASS);
}

/**
 * @brief Query if an EEPROM erase-all operation is in progress.
 * @return true if an erase-all operation is active, false otherwise.
 */
bool storage_erase_all_is_busy(void) { return s_erase_active; }

/* ********************************************************************** */
/*                        FAILURE MEMORY WRITE                            */
/* ********************************************************************** */

/**
 * @brief Enqueue an error code for deferred EEPROM logging (non-blocking).
 */
void Storage_EnqueueErrorCode(uint16_t code) {
    if (g_errorCodeQueue) {
        (void)xQueueSend(g_errorCodeQueue, &code, 0); /* drop if full */
    }
}

/**
 * @brief Enqueue a warning code for deferred EEPROM logging (non-blocking).
 */
void Storage_EnqueueWarningCode(uint16_t code) {
    if (g_warningCodeQueue) {
        (void)xQueueSend(g_warningCodeQueue, &code, 0); /* drop if full */
    }
}

/* ********************************************************************** */
/*                             STORAGE TASK                               */
/* ********************************************************************** */

/**
 * @brief Initialize and start the Storage task with a deterministic enable gate.
 * Creates mutex, event group, and task. Waits for Console readiness before proceeding.
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

    /* Create error and warning code queues */
    g_errorCodeQueue = xQueueCreate(ERROR_LOG_QUEUE_LENGTH, sizeof(uint16_t));
    g_warningCodeQueue = xQueueCreate(WARNING_LOG_QUEUE_LENGTH, sizeof(uint16_t));

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

/**
 * @brief Wait for config to be loaded from EEPROM.
 */
bool storage_wait_ready(uint32_t timeout_ms) {
    EventBits_t bits =
        xEventGroupWaitBits(cfgEvents, CFG_READY_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (bits & CFG_READY_BIT) != 0;
}

/**
 * @brief Get network configuration.
 */
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

/**
 * @brief Set network configuration.
 */
bool storage_set_network(const networkInfo *net) {
    if (!net || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_WRITE_NETWORK, .done_sem = NULL};
    memcpy(&msg.data.net_info, net, sizeof(networkInfo));

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

/**
 * @brief Get user preferences (synchronous, from RAM cache).
 *
 * @param out Pointer to output structure (not NULL).
 * @return true on success, false on error or timeout.
 */
bool storage_get_prefs(userPrefInfo *out) {
    if (!out || !eth_netcfg_ready)
        return false;

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    if (!done_sem)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_READ_PREFS, .output_ptr = out, .done_sem = done_sem};

    if (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) != pdPASS) {
        vSemaphoreDelete(done_sem);
        return false;
    }

    bool ok = xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000)) == pdPASS;
    vSemaphoreDelete(done_sem);
    return ok;
}

/**
 * @brief Set user preferences.
 */
bool storage_set_prefs(const userPrefInfo *prefs) {
    if (!prefs || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_WRITE_PREFS, .done_sem = NULL};
    memcpy(&msg.data.user_prefs, prefs, sizeof(userPrefInfo));

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

/**
 * @brief Get relay power-on states.
 */
bool storage_get_relay_states(uint8_t *out) {
    if (!out || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_READ_RELAY_STATES, .output_ptr = out, .done_sem = NULL};

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

/**
 * @brief Set relay states.
 */
bool storage_set_relay_states(const uint8_t *states) {
    if (!states || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_WRITE_RELAY_STATES, .done_sem = NULL};
    memcpy(msg.data.relay.states, states, 8);

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

/**
 * @brief Load factory defaults via StorageTask.
 */
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

/**
 * @brief Load factory defaults into EEPROM and RAM cache.
 */
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

/**
 * @brief Read sensor calibration for a channel.
 */
bool storage_get_sensor_cal(uint8_t channel, hlw_calib_t *out) {
    if (channel >= 8 || !out || !eth_netcfg_ready)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_READ_SENSOR_CAL, .output_ptr = out, .done_sem = NULL};
    msg.data.sensor_cal.channel = channel;

    return xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS;
}

/**
 * @brief Set sensor calibration for a channel.
 */
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

/**
 * @brief Save a user output preset via StorageTask.
 * Enqueues a message to perform the save on the StorageTask thread.
 */
bool storage_save_preset(uint8_t index, const char *name, uint8_t mask) {
    if (!Storage_Config_IsReady()) {
        return false;
    }
    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_SAVE_USER_OUTPUT_PRESET;
    msg.data.user_output.index = index;
    msg.data.user_output.mask = mask;
    if (name) {
        strncpy(msg.data.user_output.name, name, USER_OUTPUT_NAME_MAX_LEN);
        msg.data.user_output.name[USER_OUTPUT_NAME_MAX_LEN] = '\0';
    } else {
        msg.data.user_output.name[0] = '\0';
    }
    return xQueueSend(q_cfg, &msg, 0) == pdPASS;
}

/**
 * @brief Delete a user output preset via StorageTask.
 */
bool storage_delete_preset(uint8_t index) {
    if (!Storage_Config_IsReady()) {
        return false;
    }
    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_DELETE_USER_OUTPUT_PRESET;
    msg.data.user_output.index = index;
    return xQueueSend(q_cfg, &msg, 0) == pdPASS;
}

/**
 * @brief Set startup preset via StorageTask.
 */
bool storage_set_startup_preset(uint8_t index) {
    if (!Storage_Config_IsReady()) {
        return false;
    }
    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_SET_STARTUP_PRESET;
    msg.data.startup.index = index;
    return xQueueSend(q_cfg, &msg, 0) == pdPASS;
}

/**
 * @brief Clear startup preset via StorageTask.
 */
bool storage_clear_startup_preset(void) {
    if (!Storage_Config_IsReady()) {
        return false;
    }
    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_CLEAR_STARTUP_PRESET;
    return xQueueSend(q_cfg, &msg, 0) == pdPASS;
}