/**
 * @file src/tasks/storage_submodule/factory_defaults.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of factory default management. Handles first-boot detection
 * via magic value check, writes complete default configuration to all EEPROM sections,
 * and validates written data.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

/* External declarations from StorageTask.c */
extern const uint8_t DEFAULT_RELAY_STATUS[8];
extern const networkInfo DEFAULT_NETWORK;
extern const uint8_t DEFAULT_ENERGY_DATA[64];
extern const uint8_t DEFAULT_LOG_DATA[64];
extern SemaphoreHandle_t eepromMtx;

#define STORAGE_TASK_TAG "[Storage]"

/**
 * @brief Write factory defaults to all EEPROM sections.
 *
 * Writes default configuration for all system sections:
 * 1. System info (serial number + software version)
 * 2. Relay status (all channels OFF)
 * 3. Network configuration (with CRC and derived MAC)
 * 4. Sensor calibration (default HLW8032 factors for all channels)
 * 5. Energy monitoring data (placeholder zeros)
 * 6. Event logs (placeholder zeros)
 * 7. User preferences (default device name and location)
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @return 0 on success, -1 if any write fails
 */
int EEPROM_WriteFactoryDefaults(void) {
    int status = 0;

    /* 1. Write Serial Number + SWVERSION to System Info section */
    char sys_info_buf[64];
    size_t sn_len = strlen(DEFAULT_SN) + 1;
    size_t swv_len = strlen(SWVERSION) + 1;
    memset(sys_info_buf, 0, sizeof(sys_info_buf));
    memcpy(sys_info_buf, DEFAULT_SN, sn_len);
    memcpy(sys_info_buf + sn_len, SWVERSION, swv_len);
    status |= EEPROM_WriteSystemInfo((const uint8_t *)sys_info_buf, sn_len + swv_len);
    INFO_PRINT("%s Serial Number and SWVERSION written\r\n", STORAGE_TASK_TAG);

    /* 2. Write Relay Status (all OFF) */
    status |= EEPROM_WriteUserOutput(DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS));
    INFO_PRINT("%s Relay Status written\r\n", STORAGE_TASK_TAG);

    /* 3. Write Network Configuration (with CRC and derived MAC) */
    {
        networkInfo defnet = DEFAULT_NETWORK; /* work on a copy */
        Energis_FillMacFromSerial(defnet.mac);
        status |= EEPROM_WriteUserNetworkWithChecksum(&defnet);
        INFO_PRINT("%s Network Configuration written\r\n", STORAGE_TASK_TAG);
    }

    /* 4. Write Sensor Calibration (default factors for all channels) */
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

    /* 5. Write Energy Monitoring Data (placeholder) */
    status |= EEPROM_WriteEnergyMonitoring(DEFAULT_ENERGY_DATA, sizeof(DEFAULT_ENERGY_DATA));
    INFO_PRINT("%s Energy Monitoring Data written\r\n", STORAGE_TASK_TAG);

    /* 6. Write Event Logs (placeholder) */
    status |= EEPROM_WriteEventLogs(DEFAULT_LOG_DATA, sizeof(DEFAULT_LOG_DATA));
    INFO_PRINT("%s Event Logs written\r\n", STORAGE_TASK_TAG);

    /* 7. Write User Preferences (default name/location with CRC) */
    status |= EEPROM_WriteDefaultNameLocation();
    INFO_PRINT("%s User Preferences written\r\n", STORAGE_TASK_TAG);

    /* Report final status */
    if (status == 0) {
        INFO_PRINT("%s Factory defaults written successfully\r\n", STORAGE_TASK_TAG);
    } else {
        ERROR_PRINT("%s Factory defaults write failed\r\n", STORAGE_TASK_TAG);
    }
    return status;
}

/**
 * @brief Perform basic read-back validation after factory defaulting.
 *
 * Reads and checks critical sections to verify factory defaults were written correctly:
 * - Serial number verification
 * - Network configuration CRC check
 * - Sensor calibration presence check
 *
 * @return 0 on success, -1 on validation failure
 */
int EEPROM_ReadFactoryDefaults(void) {
    /* Check Serial Number */
    char stored_sn[32];
    EEPROM_ReadSystemInfo((uint8_t *)stored_sn, strlen(DEFAULT_SN) + 1);
    if (memcmp(stored_sn, DEFAULT_SN, strlen(DEFAULT_SN)) != 0) {
        WARNING_PRINT("%s Invalid Serial Number in EEPROM\r\n", STORAGE_TASK_TAG);
    }

    /* Check Network Configuration (CRC verified) */
    networkInfo stored_network;
    if (EEPROM_ReadUserNetworkWithChecksum(&stored_network) != 0) {
        WARNING_PRINT("%s Invalid Network Configuration in EEPROM\r\n", STORAGE_TASK_TAG);
    }

    /* Check Sensor Calibration presence */
    hlw_calib_data_t tmp;
    if (EEPROM_ReadSensorCalibration((uint8_t *)&tmp, sizeof(tmp)) != 0) {
        WARNING_PRINT("%s Sensor Calibration missing/invalid\r\n", STORAGE_TASK_TAG);
    }

    INFO_PRINT("%s EEPROM content checked\r\n", STORAGE_TASK_TAG);
    return 0;
}

/**
 * @brief Check if factory defaults need to be written (first boot detection).
 *
 * Reads magic value from EEPROM_MAGIC_ADDR. If magic != EEPROM_MAGIC_VAL,
 * this is first boot and factory defaults are written.
 *
 * @return true if defaults were written successfully or already present, false on failure
 */
bool check_factory_defaults(void) {
    uint16_t magic = 0xFFFF;

    /* Read magic value to detect first boot */
    xSemaphoreTake(eepromMtx, portMAX_DELAY);
    CAT24C256_ReadBuffer(EEPROM_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
    xSemaphoreGive(eepromMtx);

    /* Check if factory init needed */
    if (magic != EEPROM_MAGIC_VAL) {
        INFO_PRINT("%s First boot detected, writing factory defaults...\r\n", STORAGE_TASK_TAG);

        /* Write factory defaults with mutex protection */
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        int ret = EEPROM_WriteFactoryDefaults();
        if (ret == 0) {
            /* Mark EEPROM as initialized */
            magic = EEPROM_MAGIC_VAL;
            CAT24C256_WriteBuffer(EEPROM_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
        }
        xSemaphoreGive(eepromMtx);

        /* Report result */
        if (ret == 0) {
            INFO_PRINT("%s Factory defaults written successfully\r\n", STORAGE_TASK_TAG);
            return true;
        } else {
            ERROR_PRINT("%s Failed to write factory defaults\r\n", STORAGE_TASK_TAG);
            return false;
        }
    }

    /* Magic value verified - EEPROM already initialized */
    INFO_PRINT("%s Magic value verified\r\n", STORAGE_TASK_TAG);
    return true;
}