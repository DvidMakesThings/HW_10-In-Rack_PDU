/**
 * @file src/tasks/StorageTask.c
 * @author DvidMakesThings - David Sipos
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

/** @brief Default network configuration. */
const networkInfo DEFAULT_NETWORK = {.ip = {192, 168, 0, 22},
                                     .gw = {192, 168, 0, 1},
                                     .sn = {255, 255, 255, 0},
                                     .mac = {0x1C, 0x08, 0xDC, 0xBE, 0xEF, 0x91},
                                     .dns = {8, 8, 8, 8},
                                     .dhcp = EEPROM_NETINFO_STATIC};

/** @brief Default user preferences. */
static const userPrefInfo DEFAULT_USER_PREFS = {
    .device_name = DEFAULT_NAME, .location = DEFAULT_LOCATION, .temp_unit = 0};

/** @brief Default energy data (placeholder). */
const uint8_t DEFAULT_ENERGY_DATA[64] = {0};

/** @brief Default event log data (placeholder). */
const uint8_t DEFAULT_LOG_DATA[64] = {0};

/** @brief Default user pref data (placeholder). */
const uint8_t DEFAULT_USER_PREF[64] = {0};

/* ==================== Private State ==================== */

/** RAM cache (only accessed by StorageTask) */
static storage_cache_t g_cache;

/** Config ready flag */
static volatile bool eth_netcfg_ready = false;

bool Storage_Config_IsReady(void) { return eth_netcfg_ready; }

/* ==================== CRC Helper ==================== */

/**
 * @brief Calculate CRC-8 checksum.
 * @param data Input buffer
 * @param len Buffer length
 * @return CRC-8 checksum value
 */
static uint8_t calculate_crc8(const uint8_t *data, size_t len) {
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

/* ====================================================================== */
/*                  EEPROM_* FUNCTIONS (from old code)                   */
/* ====================================================================== */

/**
 * @brief Erase entire EEPROM to 0xFF.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @return 0 on success, -1 on failure
 */
int EEPROM_EraseAll(void) {
    uint8_t blank[32];
    memset(blank, 0xFF, sizeof(blank));

    for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += sizeof(blank)) {
        if (CAT24C512_WriteBuffer(addr, blank, sizeof(blank)) != 0) {
            return -1;
        }
    }
    return 0;
}

/* ---- System Info ---- */

int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, data, (uint16_t)len);
}

int EEPROM_ReadSystemInfo(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, data, (uint32_t)len);
    return 0;
}

int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1)
        return -1;

    uint8_t buffer[EEPROM_SYS_INFO_SIZE];
    memcpy(buffer, data, len);
    buffer[len] = calculate_crc8(data, len);

    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, buffer, (uint16_t)(len + 1));
}

int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1)
        return -1;

    uint8_t buffer[EEPROM_SYS_INFO_SIZE];
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, buffer, (uint32_t)(len + 1));

    uint8_t crc = calculate_crc8(buffer, len);
    if (crc != buffer[len]) {
        ERROR_PRINT("%s System info CRC mismatch\r\n", STORAGE_TASK_TAG);
        return -1;
    }

    memcpy(data, buffer, len);
    return 0;
}

/* ---- Factory Defaults ---- */

/**
 * @brief Write factory defaults to all sections.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @return 0 on success, -1 if any write fails
 */
int EEPROM_WriteFactoryDefaults(void) {
    int status = 0;

    /* Serial Number + SWVERSION */
    char sys_info_buf[64];
    size_t sn_len = strlen(DEFAULT_SN) + 1;
    size_t swv_len = strlen(SWVERSION) + 1;
    memset(sys_info_buf, 0, sizeof(sys_info_buf));
    memcpy(sys_info_buf, DEFAULT_SN, sn_len);
    memcpy(sys_info_buf + sn_len, SWVERSION, swv_len);
    status |= EEPROM_WriteSystemInfo((const uint8_t *)sys_info_buf, sn_len + swv_len);
    INFO_PRINT("%s Serial Number and SWVERSION written\r\n", STORAGE_TASK_TAG);

    /* Relay Status (all OFF) */
    status |= EEPROM_WriteUserOutput(DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS));
    INFO_PRINT("%s Relay Status written\r\n", STORAGE_TASK_TAG);

    /* Network Configuration (with CRC) */
    status |= EEPROM_WriteUserNetworkWithChecksum(&DEFAULT_NETWORK);
    INFO_PRINT("%s Network Configuration written\r\n", STORAGE_TASK_TAG);

    /* Sensor Calibration (defaults for all channels) */
    hlw_calib_data_t zero_cal;
    memset(&zero_cal, 0xFF, sizeof(zero_cal));
    for (int i = 0; i < 8; i++) {
        zero_cal.channels[i].voltage_factor = HLW8032_VF;
        zero_cal.channels[i].current_factor = HLW8032_CF;
        zero_cal.channels[i].r1_actual = 1880000.0f;
        zero_cal.channels[i].r2_actual = 1000.0f;
        zero_cal.channels[i].shunt_actual = 0.001f;
        zero_cal.channels[i].voltage_offset = 0.0f;
        zero_cal.channels[i].current_offset = 0.0f;
        zero_cal.channels[i].calibrated = 0xFF;
        zero_cal.channels[i].zero_calibrated = 0xFF;
    }
    status |= EEPROM_WriteSensorCalibration((const uint8_t *)&zero_cal, sizeof(zero_cal));
    INFO_PRINT("%s Sensor Calibration written\r\n", STORAGE_TASK_TAG);

    /* Energy Monitoring Data */
    status |= EEPROM_WriteEnergyMonitoring(DEFAULT_ENERGY_DATA, sizeof(DEFAULT_ENERGY_DATA));
    INFO_PRINT("%s Energy Monitoring Data written\r\n", STORAGE_TASK_TAG);

    /* Event Logs */
    status |= EEPROM_WriteEventLogs(DEFAULT_LOG_DATA, sizeof(DEFAULT_LOG_DATA));
    INFO_PRINT("%s Event Logs written\r\n", STORAGE_TASK_TAG);

    /* User Preferences (default name/location) */
    status |= EEPROM_WriteDefaultNameLocation();
    INFO_PRINT("%s User Preferences written\r\n", STORAGE_TASK_TAG);

    if (status == 0) {
        INFO_PRINT("%s Factory defaults written successfully\r\n", STORAGE_TASK_TAG);
    } else {
        ERROR_PRINT("%s Factory defaults write failed\r\n", STORAGE_TASK_TAG);
    }
    return status;
}

int EEPROM_ReadFactoryDefaults(void) {
    /* Serial Number check */
    char stored_sn[32];
    EEPROM_ReadSystemInfo((uint8_t *)stored_sn, strlen(DEFAULT_SN) + 1);
    if (memcmp(stored_sn, DEFAULT_SN, strlen(DEFAULT_SN)) != 0) {
        WARNING_PRINT("%s Invalid Serial Number in EEPROM\r\n", STORAGE_TASK_TAG);
    }

    /* Network (CRC verified) */
    networkInfo stored_network;
    if (EEPROM_ReadUserNetworkWithChecksum(&stored_network) != 0) {
        WARNING_PRINT("%s Invalid Network Configuration in EEPROM\r\n", STORAGE_TASK_TAG);
    }

    /* Sensor Calibration — presence check */
    hlw_calib_data_t tmp;
    if (EEPROM_ReadSensorCalibration((uint8_t *)&tmp, sizeof(tmp)) != 0) {
        WARNING_PRINT("%s Sensor Calibration missing/invalid\r\n", STORAGE_TASK_TAG);
    }

    INFO_PRINT("%s EEPROM content checked\r\n", STORAGE_TASK_TAG);
    return 0;
}

/* ---- User Output ---- */

int EEPROM_WriteUserOutput(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_OUTPUT_START, data, (uint16_t)len);
}

int EEPROM_ReadUserOutput(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_OUTPUT_START, data, (uint32_t)len);
    return 0;
}

/* ---- Network (raw + CRC) ---- */

int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, data, (uint16_t)len);
}

int EEPROM_ReadUserNetwork(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, data, (uint32_t)len);
    return 0;
}

int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info) {
    if (!net_info)
        return -1;

    uint8_t buffer[24];
    memcpy(&buffer[0], net_info->mac, 6);
    memcpy(&buffer[6], net_info->ip, 4);
    memcpy(&buffer[10], net_info->sn, 4);
    memcpy(&buffer[14], net_info->gw, 4);
    memcpy(&buffer[18], net_info->dns, 4);
    buffer[22] = net_info->dhcp;
    buffer[23] = calculate_crc8(buffer, 23);

    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, buffer, 24);
}

int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info) {
    if (!net_info)
        return -1;

    uint8_t buffer[24];
    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, buffer, 24);

    if (calculate_crc8(buffer, 23) != buffer[23]) {
        ERROR_PRINT("%s Network config CRC mismatch\r\n", STORAGE_TASK_TAG);
        return -1;
    }

    memcpy(net_info->mac, &buffer[0], 6);
    memcpy(net_info->ip, &buffer[6], 4);
    memcpy(net_info->sn, &buffer[10], 4);
    memcpy(net_info->gw, &buffer[14], 4);
    memcpy(net_info->dns, &buffer[18], 4);
    net_info->dhcp = buffer[22];

    return 0;
}

/* ---- Sensor Calibration ---- */

int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_SENSOR_CAL_START, data, (uint16_t)len);
}

int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in) {
    if (ch >= 8 || !in)
        return -1;

    uint16_t addr = EEPROM_SENSOR_CAL_START + (ch * sizeof(hlw_calib_t));
    return CAT24C512_WriteBuffer(addr, (const uint8_t *)in, sizeof(hlw_calib_t));
}

int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_SENSOR_CAL_START, data, (uint32_t)len);
    return 0;
}

int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out) {
    if (ch >= 8 || !out)
        return -1;

    uint16_t addr = EEPROM_SENSOR_CAL_START + (ch * sizeof(hlw_calib_t));
    CAT24C512_ReadBuffer(addr, (uint8_t *)out, sizeof(hlw_calib_t));

    /* Apply sane defaults if not calibrated */
    if (out->calibrated != 0xCA) {
        out->voltage_factor = HLW8032_VF;
        out->current_factor = HLW8032_CF;
        out->r1_actual = 1880000.0f;
        out->r2_actual = 1000.0f;
        out->shunt_actual = 0.001f;
    }

    return 0;
}

/* ---- Energy Monitoring ---- */

int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START, data, (uint16_t)len);
}

int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_ENERGY_MON_START, data, (uint32_t)len);
    return 0;
}

int EEPROM_AppendEnergyRecord(const uint8_t *data) {
    /* Read current pointer */
    uint16_t ptr = 0;
    CAT24C512_ReadBuffer(EEPROM_ENERGY_MON_START, (uint8_t *)&ptr, 2);

    /* Calculate address */
    uint16_t addr = EEPROM_ENERGY_MON_START + ENERGY_MON_POINTER_SIZE + (ptr * ENERGY_RECORD_SIZE);

    /* Write record */
    if (CAT24C512_WriteBuffer(addr, data, ENERGY_RECORD_SIZE) != 0)
        return -1;

    /* Update pointer (wrap around) */
    ptr++;
    if ((ptr * ENERGY_RECORD_SIZE) >= (EEPROM_ENERGY_MON_SIZE - ENERGY_MON_POINTER_SIZE))
        ptr = 0;

    return CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START, (uint8_t *)&ptr, 2);
}

/* ---- Event Logs ---- */

int EEPROM_WriteEventLogs(const uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START, data, (uint16_t)len);
}

int EEPROM_ReadEventLogs(uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_EVENT_LOG_START, data, (uint32_t)len);
    return 0;
}

int EEPROM_AppendEventLog(const uint8_t *entry) {
    /* Read current pointer */
    uint16_t ptr = 0;
    CAT24C512_ReadBuffer(EEPROM_EVENT_LOG_START, (uint8_t *)&ptr, 2);

    /* Calculate address */
    uint16_t addr = EEPROM_EVENT_LOG_START + EVENT_LOG_POINTER_SIZE + (ptr * EVENT_LOG_ENTRY_SIZE);

    /* Write entry */
    if (CAT24C512_WriteBuffer(addr, entry, EVENT_LOG_ENTRY_SIZE) != 0)
        return -1;

    /* Update pointer (wrap around) */
    ptr++;
    if ((ptr * EVENT_LOG_ENTRY_SIZE) >= (EEPROM_EVENT_LOG_SIZE - EVENT_LOG_POINTER_SIZE))
        ptr = 0;

    return CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START, (uint8_t *)&ptr, 2);
}

/* ---- User Preferences ---- */

int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, data, (uint16_t)len);
}

int EEPROM_ReadUserPreferences(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, data, (uint32_t)len);
    return 0;
}

int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs) {
    if (!prefs)
        return -1;

    uint8_t buffer[66];
    memcpy(&buffer[0], prefs->device_name, 32);
    memcpy(&buffer[32], prefs->location, 32);
    buffer[64] = prefs->temp_unit;
    buffer[65] = calculate_crc8(buffer, 65);

    /* Split write (page boundary safety) */
    int res = 0;
    res |= CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, &buffer[0], 64);
    res |= CAT24C512_WriteBuffer(EEPROM_USER_PREF_START + 64, &buffer[64], 2);
    return res;
}

int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs) {
    if (!prefs)
        return -1;

    uint8_t record[66];
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, record, 66);

    if (calculate_crc8(&record[0], 65) != record[65]) {
        ERROR_PRINT("%s User prefs CRC mismatch\r\n", STORAGE_TASK_TAG);
        return -1;
    }

    memcpy(prefs->device_name, &record[0], 32);
    prefs->device_name[31] = '\0';
    memcpy(prefs->location, &record[32], 32);
    prefs->location[31] = '\0';
    prefs->temp_unit = record[64];

    return 0;
}

userPrefInfo LoadUserPreferences(void) {
    userPrefInfo prefs;
    if (EEPROM_ReadUserPrefsWithChecksum(&prefs) == 0) {
        INFO_PRINT("%s Loaded user prefs from EEPROM\r\n", STORAGE_TASK_TAG);
    } else {
        WARNING_PRINT("%s No saved prefs, using defaults\r\n", STORAGE_TASK_TAG);
        prefs = DEFAULT_USER_PREFS;
    }
    return prefs;
}

int EEPROM_WriteDefaultNameLocation(void) {
    return EEPROM_WriteUserPrefsWithChecksum(&DEFAULT_USER_PREFS);
}

networkInfo LoadUserNetworkConfig(void) {
    networkInfo net_info;
    if (EEPROM_ReadUserNetworkWithChecksum(&net_info) == 0) {
        INFO_PRINT("%s Loaded network config from EEPROM\r\n", STORAGE_TASK_TAG);
    } else {
        WARNING_PRINT("%s Invalid network config, using defaults\r\n", STORAGE_TASK_TAG);
        net_info = DEFAULT_NETWORK;
    }
    return net_info;
}

/* ---- Channel Labels ---- */

static inline uint16_t _LabelSlotAddr(uint8_t channel_index) {
    return (uint16_t)(EEPROM_CH_LABEL_START + (uint32_t)channel_index * EEPROM_CH_LABEL_SLOT);
}

int EEPROM_WriteChannelLabel(uint8_t channel_index, const char *label) {
    if (channel_index >= ENERGIS_NUM_CHANNELS || label == NULL)
        return -1;

    uint8_t buf[EEPROM_CH_LABEL_SLOT];
    size_t maxcpy = EEPROM_CH_LABEL_SLOT - 1u;

    size_t n = 0u;
    for (; n < maxcpy && label[n] != '\0'; ++n)
        buf[n] = (uint8_t)label[n];

    buf[n++] = 0x00;
    for (; n < EEPROM_CH_LABEL_SLOT; ++n)
        buf[n] = 0x00;

    return CAT24C512_WriteBuffer(_LabelSlotAddr(channel_index), buf, EEPROM_CH_LABEL_SLOT);
}

int EEPROM_ReadChannelLabel(uint8_t channel_index, char *out, size_t out_len) {
    if (channel_index >= ENERGIS_NUM_CHANNELS || out == NULL || out_len == 0u)
        return -1;

    uint8_t buf[EEPROM_CH_LABEL_SLOT];
    CAT24C512_ReadBuffer(_LabelSlotAddr(channel_index), buf, EEPROM_CH_LABEL_SLOT);

    size_t i = 0u;
    while (i + 1u < out_len && i < EEPROM_CH_LABEL_SLOT && buf[i] != 0x00) {
        out[i] = (char)buf[i];
        ++i;
    }
    out[i] = '\0';
    return 0;
}

int EEPROM_ClearChannelLabel(uint8_t channel_index) {
    if (channel_index >= ENERGIS_NUM_CHANNELS)
        return -1;

    uint8_t zero[32];
    memset(zero, 0x00, sizeof(zero));

    for (uint16_t addr = 0; addr < EEPROM_CH_LABEL_SLOT; addr += sizeof(zero)) {
        if (CAT24C512_WriteBuffer(_LabelSlotAddr(channel_index) + addr, zero, sizeof(zero)) != 0)
            return -1;
    }
    return 0;
}

int EEPROM_ClearAllChannelLabels(void) {
    for (uint8_t ch = 0; ch < ENERGIS_NUM_CHANNELS; ++ch)
        if (EEPROM_ClearChannelLabel(ch) != 0)
            return -1;
    return 0;
}

/* ====================================================================== */
/*                        RTOS STORAGE TASK                              */
/* ====================================================================== */

/**
 * @brief Check if factory defaults need to be written (first boot).
 * @return true if defaults were written, false otherwise
 */
bool check_factory_defaults(void) {
    uint16_t magic = 0xFFFF;

    xSemaphoreTake(eepromMtx, portMAX_DELAY);
    CAT24C512_ReadBuffer(EEPROM_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
    xSemaphoreGive(eepromMtx);

    if (magic != EEPROM_MAGIC_VAL) {
        INFO_PRINT("%s First boot detected, writing factory defaults...\r\n", STORAGE_TASK_TAG);

        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        int ret = EEPROM_WriteFactoryDefaults();
        if (ret == 0) {
            magic = EEPROM_MAGIC_VAL;
            CAT24C512_WriteBuffer(EEPROM_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
        }
        xSemaphoreGive(eepromMtx);

        if (ret == 0) {
            INFO_PRINT("%s Factory defaults written successfully\r\n", STORAGE_TASK_TAG);
            return true;
        } else {
            ERROR_PRINT("%s Failed to write factory defaults\r\n", STORAGE_TASK_TAG);
            return false;
        }
    }
    INFO_PRINT("%s Magic value verified\r\n", STORAGE_TASK_TAG);

    return true;
}

/**
 * @brief Load all config from EEPROM to RAM cache.
 */
static void load_config_from_eeprom(void) {
    xSemaphoreTake(eepromMtx, portMAX_DELAY);

    /* Load network config */
    g_cache.network = LoadUserNetworkConfig();

    /* Load user preferences */
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
 */
static void commit_dirty_sections(void) {
#ifndef STORAGE_REBOOT_ON_CONFIG_SAVE
#define STORAGE_REBOOT_ON_CONFIG_SAVE 0
#endif

    xSemaphoreTake(eepromMtx, portMAX_DELAY);

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

    if (g_cache.relay_dirty) {
        if (EEPROM_WriteUserOutput(g_cache.relay_states, sizeof(g_cache.relay_states)) == 0) {
            g_cache.relay_dirty = false;
            ECHO("%s Relay states committed\r\n", STORAGE_TASK_TAG);
        } else {
            ERROR_PRINT("%s Failed to commit relay states\r\n", STORAGE_TASK_TAG);
        }
    }

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
 */
static void process_storage_msg(const storage_msg_t *msg) {
    if (!msg)
        return;

    switch (msg->cmd) {
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

    case STORAGE_CMD_COMMIT:
        commit_dirty_sections();
        break;

    case STORAGE_CMD_LOAD_DEFAULTS:
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        EEPROM_WriteFactoryDefaults();
        xSemaphoreGive(eepromMtx);
        load_config_from_eeprom();
        break;

    case STORAGE_CMD_DUMP_FORMATTED:
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        CAT24C512_DumpFormatted();
        xSemaphoreGive(eepromMtx);
        break;

    case STORAGE_CMD_SELF_TEST:
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        bool result = CAT24C512_SelfTest(msg->data.sil_test.test_addr);
        if (msg->data.sil_test.result) {
            *msg->data.sil_test.result = result;
        }
        xSemaphoreGive(eepromMtx);
        break;

    default:
        WARNING_PRINT("%s Unknown command: %d\r\n", STORAGE_TASK_TAG, msg->cmd);
        break;
    }

    /* Signal completion if semaphore provided */
    if (msg->done_sem) {
        xSemaphoreGive(msg->done_sem);
    }
}

/**
 * @brief Storage task main function.
 */
static void StorageTask(void *arg) {
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(1000));
    ECHO("%s Task started\r\n", STORAGE_TASK_TAG);

    check_factory_defaults();
    load_config_from_eeprom();

    eth_netcfg_ready = true;
    xEventGroupSetBits(cfgEvents, CFG_READY_BIT);
    INFO_PRINT("%s Config ready\r\n", STORAGE_TASK_TAG);

    storage_msg_t msg;
    const TickType_t poll_ticks = pdMS_TO_TICKS(STORAGE_QUEUE_POLL_MS);

    static uint32_t hb_stor_ms = 0;

    for (;;) {
        uint32_t __now = to_ms_since_boot(get_absolute_time());
        if ((__now - hb_stor_ms) >= STORAGETASKBEAT_MS) {
            hb_stor_ms = __now;
            Health_Heartbeat(HEALTH_ID_STORAGE);
        }

        if (xQueueReceive(q_cfg, &msg, poll_ticks) == pdPASS) {
            process_storage_msg(&msg);
        }

        if (g_cache.last_change_tick != 0) {
            TickType_t now = xTaskGetTickCount();
            if ((now - g_cache.last_change_tick) >= pdMS_TO_TICKS(STORAGE_DEBOUNCE_MS)) {
                commit_dirty_sections();
                g_cache.last_change_tick = 0;
            }
        }
    }
}

/* ====================================================================== */
/*                       PUBLIC API FUNCTIONS                            */
/* ====================================================================== */

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

    /* Original body preserved */
    extern void StorageTask(void *arg);
    eepromMtx = xSemaphoreCreateMutex();
    cfgEvents = xEventGroupCreate();

    if (!eepromMtx || !cfgEvents) {
        ERROR_PRINT("%s Failed to create mutex/events\r\n", STORAGE_TASK_TAG);
        return;
    }

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
 * @details
 * Returns true once StorageTask has signaled CFG_READY in cfgEvents. This directly
 * reflects the config-ready bit instead of relying on any separate latch.
 *
 * @return true if configuration is ready, false otherwise.
 */
bool Storage_IsReady(void) {
    /* Fast path: once StorageTask finished boot load it sets this flag. */
    extern volatile bool eth_netcfg_ready;
    if (eth_netcfg_ready) {
        return true;
    }

    /* Fallback to event bit (in case other modules rely on it). */
    extern EventGroupHandle_t cfgEvents; /* defined in StorageTask.c */
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

bool storage_dump_formatted(uint32_t timeout_ms) {
    if (!eth_netcfg_ready)
        return false;

    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    if (!done_sem)
        return false;

    storage_msg_t msg = {.cmd = STORAGE_CMD_DUMP_FORMATTED, .done_sem = done_sem};

    if (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(timeout_ms)) != pdPASS) {
        vSemaphoreDelete(done_sem);
        return false;
    }

    bool ok = xSemaphoreTake(done_sem, pdMS_TO_TICKS(timeout_ms)) == pdPASS;
    vSemaphoreDelete(done_sem);
    return ok;
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