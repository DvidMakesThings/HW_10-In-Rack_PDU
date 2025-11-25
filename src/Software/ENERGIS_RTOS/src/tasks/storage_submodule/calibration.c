/**
 * @file src/tasks/storage_submodule/calibration.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2
 * @date 2025-11-15
 *
 * @brief Sensor calibration storage for ENERGIS.
 *
 * @details
 * - HLW8032 (per-channel) calibration read/write helpers.
 * - RP2040 die temperature calibration storage (single-point or two-point).
 * - Application helper to push temp calibration into MeterTask.
 *
 * CRITICAL: All EEPROM read/write functions here must be called with eepromMtx held.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github  https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define ST_CAL_TAG "[ST-CAL]"

/* ========================================================================== */
/*                             HLW8032 CALIBRATION                            */
/* ========================================================================== */

/**
 * @brief Write entire sensor calibration area to EEPROM.
 *
 * @param data Source buffer containing calibration data
 * @param len  Number of bytes to write (must be <= EEPROM_SENSOR_CAL_SIZE)
 * @return 0 on success, -1 on bounds check failure or I2C error
 *
 * @details Typically used to write all channel calibration data at once
 *          (array of @ref hlw_calib_t). Must be called with eepromMtx held.
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x1);
        ERROR_PRINT_CODE(err_code, "%s  Write Sensor Calibration: Length %d exceeds max %d\r\n",
                         ST_CAL_TAG, len, EEPROM_SENSOR_CAL_SIZE);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }
    return CAT24C256_WriteBuffer(EEPROM_SENSOR_CAL_START, data, (uint16_t)len);
}

/**
 * @brief Read entire sensor calibration area from EEPROM.
 *
 * @param data Destination buffer
 * @param len  Number of bytes to read (must be <= EEPROM_SENSOR_CAL_SIZE)
 * @return 0 on success, -1 on bounds check failure
 *
 * @details Reads the contiguous HLW8032 calibration block. Must be called
 *          with eepromMtx held.
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x2);
        ERROR_PRINT_CODE(err_code, "%s  Read Sensor Calibration: Length %d exceeds max %d\r\n",
                         ST_CAL_TAG, len, EEPROM_SENSOR_CAL_SIZE);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }
    CAT24C256_ReadBuffer(EEPROM_SENSOR_CAL_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Write calibration record for a single HLW8032 channel.
 *
 * @param ch  Channel index [0..7]
 * @param in  Pointer to calibration data structure
 * @return 0 on success, -1 on invalid channel or null pointer
 *
 * @details Calculates the per-channel address and writes @ref hlw_calib_t.
 *          Must be called with eepromMtx held.
 */
int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in) {
    if (ch >= 8 || !in) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x3);
        ERROR_PRINT_CODE(err_code, "%s Write Sensor Calibration: Invalid channel %d\r\n",
                         ST_CAL_TAG, ch);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }
    uint16_t addr = EEPROM_SENSOR_CAL_START + (ch * sizeof(hlw_calib_t));
    return CAT24C256_WriteBuffer(addr, (const uint8_t *)in, sizeof(hlw_calib_t));
}

/**
 * @brief Read calibration record for a single HLW8032 channel.
 *
 * @param ch  Channel index [0..7]
 * @param out Pointer to calibration data structure to fill
 * @return 0 on success, -1 on invalid channel or null pointer
 *
 * @details If record is not marked calibrated (flag != 0xCA), default HLW8032
 *          factors and component values are populated. Must be called with
 *          eepromMtx held.
 */
int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out) {
    if (ch >= 8 || !out) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x3);
        ERROR_PRINT_CODE(err_code, "%s Read Sensor Calibration: Invalid channel %d\r\n", ST_CAL_TAG,
                         ch);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint16_t addr = EEPROM_SENSOR_CAL_START + (ch * sizeof(hlw_calib_t));
    CAT24C256_ReadBuffer(addr, (uint8_t *)out, sizeof(hlw_calib_t));

    if (out->calibrated != 0xCA) {
        out->voltage_factor = HLW8032_VF;
        out->current_factor = HLW8032_CF;
        out->r1_actual = 1880000.0f;
        out->r2_actual = 1000.0f;
        out->shunt_actual = 0.001f;
    }
    return 0;
}

/* ========================================================================== */
/*                         RP2040 TEMP CALIBRATION BLOCK                      */
/* ========================================================================== */

#ifndef TEMP_CAL_MAGIC
#define TEMP_CAL_MAGIC 0x5443u /* 'T' 'C' */
#endif

#ifndef TEMP_CAL_VERSION
#define TEMP_CAL_VERSION 1u
#endif

/**
 * @brief Compute a simple CRC32 (poly 0xEDB88320) over a buffer.
 *
 * @param data Pointer to buffer
 * @param len  Length in bytes
 * @return CRC32 value
 *
 * @note Tiny software CRC32 for integrity. If code size is critical, you can
 *       replace with 0 and skip verification (leave crc32=0).
 */
static uint32_t tempcal_crc32(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/**
 * @brief Fill @ref temp_calib_t with safe defaults (no calibration).
 *
 * @param out Pointer to structure to initialize
 * @return None
 */
static void tempcal_defaults(temp_calib_t *out) {
    memset(out, 0, sizeof(*out));
    out->magic = TEMP_CAL_MAGIC;
    out->version = TEMP_CAL_VERSION;
    out->mode = TEMP_CAL_MODE_NONE;
    out->v0_volts_at_27c = 0.706f;
    out->slope_volts_per_deg = 0.001721f;
    out->offset_c = 0.0f;
    out->crc32 = 0u;
}

/**
 * @brief Validate a temp calibration record (magic/version/ranges/CRC).
 *
 * @param in Pointer to record to validate
 * @return 1 if valid, 0 otherwise
 */
static int tempcal_is_valid(const temp_calib_t *in) {
    if (!in)
        return 0;
    if (in->magic != TEMP_CAL_MAGIC)
        return 0;
    if (in->version != TEMP_CAL_VERSION)
        return 0;
    if (in->mode > TEMP_CAL_MODE_2PT)
        return 0;
    if (!(in->v0_volts_at_27c > 0.60f && in->v0_volts_at_27c < 0.85f))
        return 0;
    if (!(in->slope_volts_per_deg > 0.0005f && in->slope_volts_per_deg < 0.005f))
        return 0;
    if (in->crc32 != 0u) {
        temp_calib_t tmp = *in;
        tmp.crc32 = 0u;
        uint32_t calc = tempcal_crc32(&tmp, sizeof(tmp));
        if (calc != in->crc32)
            return 0;
    }
    return 1;
}

/**
 * @brief Compute and set CRC32 field for a record (over all but crc32).
 *
 * @param rec Pointer to record to update
 * @return None
 */
static void tempcal_finalize_crc(temp_calib_t *rec) {
    rec->crc32 = 0u;
    rec->crc32 = tempcal_crc32(rec, sizeof(*rec));
}

/**
 * @brief Write RP2040 temperature calibration block to EEPROM.
 *
 * @param cal Pointer to calibration data to store
 * @return 0 on success, -1 on invalid input or I2C error
 *
 * @details
 * Writes @ref temp_calib_t into the block defined by @ref EEPROM_TEMP_CAL_START
 * and @ref EEPROM_TEMP_CAL_SIZE. Must be called with eepromMtx held.
 */
int EEPROM_WriteTempCalibration(const temp_calib_t *cal) {
    if (!cal) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x4);
        ERROR_PRINT_CODE(err_code, "%s Null pointer was passed to write\r\n", ST_CAL_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif

        return -1;
    }

    temp_calib_t copy = *cal;
    copy.magic = TEMP_CAL_MAGIC;
    copy.version = TEMP_CAL_VERSION;
    tempcal_finalize_crc(&copy);

    if (sizeof(copy) > EEPROM_TEMP_CAL_SIZE) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x5);
        ERROR_PRINT_CODE(err_code, "%s  Calibration size %d exceeds max %d\r\n", ST_CAL_TAG,
                         (int)sizeof(copy), EEPROM_TEMP_CAL_SIZE);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    return CAT24C256_WriteBuffer(EEPROM_TEMP_CAL_START, (const uint8_t *)&copy,
                                 (uint16_t)sizeof(copy));
}

/**
 * @brief Read RP2040 temperature calibration block from EEPROM.
 *
 * @param out Pointer to struct to fill
 * @return 0 if valid data loaded, -1 if not present/invalid (defaults returned)
 *
 * @details
 * On failure, @p out is filled with safe defaults (mode NONE).
 * Must be called with eepromMtx held.
 */
int EEPROM_ReadTempCalibration(temp_calib_t *out) {
    if (!out) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x6);
        ERROR_PRINT_CODE(err_code, "%s Null pointer was passed to read\r\n", ST_CAL_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    temp_calib_t tmp;
    memset(&tmp, 0, sizeof(tmp));

    CAT24C256_ReadBuffer(EEPROM_TEMP_CAL_START, (uint8_t *)&tmp, (uint32_t)sizeof(tmp));

    if (!tempcal_is_valid(&tmp)) {
        tempcal_defaults(out);
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x7);
        ERROR_PRINT_CODE(
            err_code, "%s Read Temperature calibration: Invalid calibration data\r\n, " ST_CAL_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    *out = tmp;
    return 0;
}

/**
 * @brief Compute a single-point temp calibration (offset only) from a reference.
 *
 * @param ambient_c True ambient temperature [°C]
 * @param raw_temp  ADC raw code measured at ambient (AINSEL=4)
 * @param out       Output structure to populate (mode=TEMP_CAL_MODE_1PT)
 * @return 0 on success, -1 on invalid arguments
 *
 * @details
 * Uses current model defaults (0.706 V @27°C, 1.721 mV/°C) to compute T_raw then
 * sets offset so that reported temperature matches @p ambient_c.
 * The resulting @ref temp_calib_t can be written to EEPROM with
 * @ref EEPROM_WriteTempCalibration() and applied at boot.
 */
int TempCalibration_ComputeSinglePoint(float ambient_c, uint16_t raw_temp, temp_calib_t *out) {
    if (!out) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x8);
        ERROR_PRINT_CODE(err_code, "%s Compute Single Point: Null pointer\r\n", ST_CAL_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    tempcal_defaults(out);
    out->mode = TEMP_CAL_MODE_1PT;

    const float ADC_MAX_F = (float)ADC_MAX;
    const float v = ((float)raw_temp) * (ADC_VREF / ADC_MAX_F);
    const float t_raw = 27.0f - (v - out->v0_volts_at_27c) / out->slope_volts_per_deg;

    out->offset_c = (ambient_c - t_raw);
    tempcal_finalize_crc(out);
    return 0;
}

/**
 * @brief Compute a two-point temp calibration (slope + intercept).
 *
 * @param t1_c  True temperature point #1 [°C] (e.g., ambient)
 * @param raw1  ADC raw at point #1
 * @param t2_c  True temperature point #2 [°C] (e.g., warmed)
 * @param raw2  ADC raw at point #2
 * @param out   Output structure to populate (mode=TEMP_CAL_MODE_2PT, offset=0)
 * @return 0 on success, -1 on invalid inputs or out-of-range results
 *
 * @details
 * Derives slope and V0 from the two points, sets residual offset to 0.
 * Callers can optionally do a tiny single-point trim afterwards to reduce
 * residuals at ambient.
 */
int TempCalibration_ComputeTwoPoint(float t1_c, uint16_t raw1, float t2_c, uint16_t raw2,
                                    temp_calib_t *out) {
    if (!out) {

#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0x9);
        ERROR_PRINT_CODE(err_code, "%s Compute Two Point: Null pointer\r\n", ST_CAL_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }
    if (t1_c == t2_c) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0xA);
        ERROR_PRINT_CODE(err_code, "%s Compute Two Point: Identical temperature points %.2f°C\r\n",
                         ST_CAL_TAG, t1_c);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    tempcal_defaults(out);
    out->mode = TEMP_CAL_MODE_2PT;

    const float v1 = ((float)raw1) * (ADC_VREF / (float)ADC_MAX);
    const float v2 = ((float)raw2) * (ADC_VREF / (float)ADC_MAX);
    const float dt = (t1_c - t2_c);

    const float s_cal = (v2 - v1) / dt; /* V/°C */
    const float v0_cal = v1 - s_cal * (27.0f - t1_c);

    if (!(s_cal > 0.0005f && s_cal < 0.005f)) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0xB);
        ERROR_PRINT_CODE(err_code, "%s Computed two point slope %.6f V/°C out of range\r\n",
                         ST_CAL_TAG, s_cal);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }
    if (!(v0_cal > 0.60f && v0_cal < 0.85f)) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0xC);
        ERROR_PRINT_CODE(err_code, "%s Computed two point V0 %.6f V out of range\r\n", ST_CAL_TAG,
                         v0_cal);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    out->slope_volts_per_deg = s_cal;
    out->v0_volts_at_27c = v0_cal;
    out->offset_c = 0.0f;

    tempcal_finalize_crc(out);
    return 0;
}

/**
 * @brief Apply a temperature calibration record to MeterTask.
 *
 * @param cal Pointer to temp calibration record (already validated or read)
 * @return 0 on success, -1 on invalid input or rejection by MeterTask
 *
 * @details
 * Pushes (V0, slope, offset) into the MeterTask conversion path so that all
 * consumers (CLI, /api/status, /metrics) see calibrated °C. This does not
 * persist any data; callers should write @p cal to EEPROM separately.
 */
int TempCalibration_ApplyToMeterTask(const temp_calib_t *cal) {
    if (!cal) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0xD);
        ERROR_PRINT_CODE(err_code, "%s Apply To MeterTask: Null pointer\r\n", ST_CAL_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }
    if (!tempcal_is_valid(cal)) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CALIBRATION, 0xE);
        ERROR_PRINT_CODE(err_code, "%s Apply To MeterTask: Invalid calibration data\r\n",
                         ST_CAL_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    bool ok =
        MeterTask_SetTempCalibration(cal->v0_volts_at_27c, cal->slope_volts_per_deg, cal->offset_c);
    return ok ? 0 : -1;
}

/**
 * @brief Load temp calibration from EEPROM and apply to MeterTask.
 *
 * @param out Optional pointer to receive the loaded (or default) record
 * @return 0 if valid calibration was applied, -1 if defaults applied
 *
 * @details
 * Convenience helper for boot: read record, validate, apply. If invalid,
 * defaults are applied (mode NONE) and -1 is returned.
 * Must be called with eepromMtx held for the EEPROM read.
 */
int TempCalibration_LoadAndApply(temp_calib_t *out) {
    temp_calib_t rec;
    int rc = EEPROM_ReadTempCalibration(&rec);
    (void)TempCalibration_ApplyToMeterTask(&rec);
    if (out)
        *out = rec;
    return rc;
}
