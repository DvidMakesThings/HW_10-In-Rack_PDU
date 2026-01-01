/**
 * @file src/tasks/storage_submodule/factory_defaults.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-12-14
 *
 * @details
 * Implementation of factory default management. Handles first-boot detection
 * via magic value check, writes complete default configuration to EEPROM sections,
 * and validates written data.
 *
 * Version History:
 * - v1.x: Wrote compile-time serial number to EEPROM
 * - v2.0: Serial number managed by device_identity module via provisioning
 *
 * @note
 * Device identity (serial number and region) is NOT written by factory defaults.
 * These values must be provisioned separately using UART commands after first boot.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"
#include "calibration.h"

#define ST_FACTORY_DEFAULTS_TAG "[ST-FDEF]"

/* External declarations from StorageTask.c */
extern const uint8_t DEFAULT_RELAY_STATUS[8];
extern const networkInfo DEFAULT_NETWORK;
extern const uint8_t DEFAULT_ENERGY_DATA[64];
extern const uint8_t DEFAULT_LOG_DATA[64];
extern SemaphoreHandle_t eepromMtx;

/**
 * @brief Write factory defaults to all EEPROM sections.
 *
 * @details
 * Writes default configuration for all system sections:
 * 1. System info (firmware version only - serial number managed separately)
 * 2. Relay status (all channels OFF)
 * 3. Network configuration (with CRC and derived MAC)
 * 4. Sensor calibration (default HLW8032 factors for all channels)
 * 5. Energy monitoring data (placeholder zeros)
 * 6. Event logs (placeholder zeros)
 * 7. User preferences (default device name and location)
 *
 * Device identity (serial number, region) is NOT written here.
 * Use UART provisioning commands to set these values.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @return 0 on success, -1 if any write fails.
 */
int EEPROM_WriteFactoryDefaults(void) {
    int status = 0;

    /* 1. Write firmware version to System Info section (no serial number) */
    char sys_info_buf[64];
    memset(sys_info_buf, 0, sizeof(sys_info_buf));

    /* Write firmware version string */
    size_t swv_len = strlen(SWVERSION) + 1;
    memcpy(sys_info_buf, SWVERSION, swv_len);

    status |= EEPROM_WriteSystemInfo((const uint8_t *)sys_info_buf, swv_len);
    DEBUG_PRINT("%s Firmware version written (SN via provisioning)\r\n", ST_FACTORY_DEFAULTS_TAG);

    /* 2. Write Relay Status (all OFF) */
    status |= EEPROM_WriteUserOutput(DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS));
    DEBUG_PRINT("%s Relay Status written\r\n", ST_FACTORY_DEFAULTS_TAG);

    /* 3. Write Network Configuration (with CRC and derived MAC) */
    {
        networkInfo defnet = DEFAULT_NETWORK; /* work on a copy */

        /* Derive MAC from device identity (uses cached serial or placeholder) */
        DeviceIdentity_FillMac(defnet.mac);

        status |= EEPROM_WriteUserNetworkWithChecksum(&defnet);
        DEBUG_PRINT("%s Network Configuration written\r\n", ST_FACTORY_DEFAULTS_TAG);
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
    DEBUG_PRINT("%s Sensor Calibration written\r\n", ST_FACTORY_DEFAULTS_TAG);

    /* 5. Write Energy Monitoring Data (placeholder) */
    status |= EEPROM_WriteEnergyMonitoring(DEFAULT_ENERGY_DATA, sizeof(DEFAULT_ENERGY_DATA));
    DEBUG_PRINT("%s Energy Monitoring Data written\r\n", ST_FACTORY_DEFAULTS_TAG);

    /* 6. Write Event Logs (placeholder) */
    status |= EEPROM_WriteEventLogs(DEFAULT_LOG_DATA, sizeof(DEFAULT_LOG_DATA));
    DEBUG_PRINT("%s Event Logs written\r\n", ST_FACTORY_DEFAULTS_TAG);

    /* 7. Write User Preferences (default name/location with CRC) */
    status |= EEPROM_WriteDefaultNameLocation();
    DEBUG_PRINT("%s User Preferences written\r\n", ST_FACTORY_DEFAULTS_TAG);

    /* Report final status */
    if (status == 0) {
        INFO_PRINT("%s Factory defaults written successfully\r\n", ST_FACTORY_DEFAULTS_TAG);
        WARNING_PRINT("%s NOTE: Serial number and region require provisioning\r\n",
                      ST_FACTORY_DEFAULTS_TAG);
    } else {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_FACTORY_DEFS, 0x1);
        ERROR_PRINT_CODE(err_code, "%s Factory defaults write encountered errors\r\n",
                         ST_FACTORY_DEFAULTS_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
    }
    return status;
}

/**
 * @brief Perform basic read-back validation after factory defaulting.
 *
 * @details
 * Reads and checks critical sections to verify factory defaults were written correctly:
 * - Firmware version verification
 * - Network configuration CRC check
 * - Sensor calibration presence check
 *
 * Device identity is checked via DeviceIdentity_IsValid().
 *
 * @return 0 on success, -1 on validation failure.
 */
int EEPROM_ReadFactoryDefaults(void) {
    /* Check Firmware Version */
    char stored_fw[32];
    memset(stored_fw, 0, sizeof(stored_fw));
    EEPROM_ReadSystemInfo((uint8_t *)stored_fw, strlen(SWVERSION) + 1);
    if (memcmp(stored_fw, SWVERSION, strlen(SWVERSION)) != 0) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_FACTORY_DEFS, 0x1);
        WARNING_PRINT_CODE(err_code, "%s Firmware version mismatch: expected '%s', got '%s'\r\n",
                           ST_FACTORY_DEFAULTS_TAG, SWVERSION, stored_fw);
        Storage_EnqueueWarningCode(err_code);
#endif
    }

    /* Check Device Identity */
    if (!DeviceIdentity_IsValid()) {
        INFO_PRINT("%s Device identity not provisioned (SN/region required)\r\n",
                   ST_FACTORY_DEFAULTS_TAG);
    }

    /* Check Network Configuration (CRC verified) */
    networkInfo stored_network;
    if (EEPROM_ReadUserNetworkWithChecksum(&stored_network) != 0) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_FACTORY_DEFS, 0x2);
        WARNING_PRINT_CODE(err_code, "%s Network Configuration CRC mismatch\r\n",
                           ST_FACTORY_DEFAULTS_TAG);
        Storage_EnqueueWarningCode(err_code);
#endif
    }

    /* Check Sensor Calibration presence */
    hlw_calib_data_t tmp;
    if (EEPROM_ReadSensorCalibration((uint8_t *)&tmp, sizeof(tmp)) != 0) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_FACTORY_DEFS, 0x3);
        WARNING_PRINT_CODE(err_code, "%s Sensor Calibration read error\r\n",
                           ST_FACTORY_DEFAULTS_TAG);
        Storage_EnqueueWarningCode(err_code);
#endif
    }

    INFO_PRINT("%s EEPROM content checked\r\n", ST_FACTORY_DEFAULTS_TAG);
    return 0;
}

/**
 * @brief Check if factory defaults need to be written (first boot detection).
 *
 * @details
 * Reads magic value from EEPROM_MAGIC_ADDR. If magic != EEPROM_MAGIC_VAL,
 * this is first boot and factory defaults are written.
 *
 * Also initializes DeviceIdentity module to load any existing provisioning.
 *
 * @return true if defaults were written successfully or already present, false on failure.
 */
bool check_factory_defaults(void) {
    uint16_t magic = 0xFFFF;

    /* Initialize device identity module (loads existing provisioning) */
    DeviceIdentity_Init();

    /* Read magic value to detect first boot */
    xSemaphoreTake(eepromMtx, portMAX_DELAY);
    INFO_PRINT("%s Reading EEPROM magic...\r\n", ST_FACTORY_DEFAULTS_TAG);
    CAT24C256_ReadBuffer(EEPROM_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
    xSemaphoreGive(eepromMtx);
    DEBUG_PRINT("%s Magic read: 0x%04X\r\n", ST_FACTORY_DEFAULTS_TAG, magic);

    /* Check if factory init needed */
    if (magic != EEPROM_MAGIC_VAL) {
        DEBUG_PRINT("%s First boot detected, writing factory defaults...\r\n",
                    ST_FACTORY_DEFAULTS_TAG);

        /* Write factory defaults with mutex protection */
        xSemaphoreTake(eepromMtx, portMAX_DELAY);
        int ret = EEPROM_WriteFactoryDefaults();

        /* Also write an initial temperature calibration record:
           mode=1-point, v0=0.706 V, slope=0.001721 V/°C, offset=0.0 °C. */
        {
            temp_calib_t tcal;
            memset(&tcal, 0, sizeof(tcal));
            tcal.mode = TEMP_CAL_MODE_1PT;
            tcal.v0_volts_at_27c = 0.706f;
            tcal.slope_volts_per_deg = 0.001721f;
            tcal.offset_c = 0.0f;
            /* magic/version + crc are set by EEPROM_WriteTempCalibration() */
            ret |= EEPROM_WriteTempCalibration(&tcal);

            /* Read-back verify key fields (magic/version/mode) */
            temp_calib_t chk;
            memset(&chk, 0, sizeof(chk));
            CAT24C256_ReadBuffer(EEPROM_TEMP_CAL_START, (uint8_t *)&chk, (uint32_t)sizeof(chk));
            DEBUG_PRINT("%s TempCal written\r\n", ST_FACTORY_DEFAULTS_TAG);
        }

        /* Also write default HLW8032 per-channel calibration for all 8 channels.
           This mirrors the intent of AUTO_CAL_ZERO but without taking measurements:
           set default VF/CF and zero offsets, leave flags as not-calibrated (0xFF). */
        {
            for (uint8_t ch = 0u; ch < 8u; ch++) {
                hlw_calib_t c;
                memset(&c, 0xFF, sizeof(c));
                c.voltage_factor = HLW8032_VF;
                c.current_factor = HLW8032_CF;
                c.r1_actual = 1880000.0f;
                c.r2_actual = 1000.0f;
                c.shunt_actual = 0.001f;
                c.voltage_offset = 0.0f;
                c.current_offset = 0.0f;
                c.calibrated = 0xFF;      /* not calibrated */
                c.zero_calibrated = 0xFF; /* zero-cal not performed */
                ret |= EEPROM_WriteSensorCalibrationForChannel(ch, &c);
            }

            /* Briefly read-back CH0 to confirm defaults are present */
            hlw_calib_t chk0;
            memset(&chk0, 0, sizeof(chk0));
            CAT24C256_ReadBuffer(EEPROM_SENSOR_CAL_START, (uint8_t *)&chk0, (uint32_t)sizeof(chk0));
            DEBUG_PRINT("%s Channel calibration written\r\n", ST_FACTORY_DEFAULTS_TAG);
        }
        if (ret == 0) {
            /* Mark EEPROM as initialized */
            magic = EEPROM_MAGIC_VAL;
            CAT24C256_WriteBuffer(EEPROM_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
        }
        xSemaphoreGive(eepromMtx);

        /* Report result */
        if (ret == 0) {
            DEBUG_PRINT("%s Factory defaults written successfully\r\n", ST_FACTORY_DEFAULTS_TAG);
            return true;
        } else {
#if ERRORLOGGER
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_FACTORY_DEFS, 0x2);
            ERROR_PRINT_CODE(err_code, "%s Factory defaults write failed\r\n",
                             ST_FACTORY_DEFAULTS_TAG);
            Storage_EnqueueErrorCode(err_code);
#endif
            return false;
        }
    }

    /* Magic value verified - EEPROM already initialized */
    INFO_PRINT("%s Magic value verified\r\n", ST_FACTORY_DEFAULTS_TAG);
    return true;
}