/**
 * @file src/tasks/storage_submodule/calibration.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage05 5. Sensor Calibration
 * @ingroup storage
 * @brief HLW8032 power meter calibration per channel
 * @{
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Manages HLW8032 power meter calibration data for all 8 channels.
 * Stores voltage/current factors, offsets, and resistor values for accurate
 * power measurement.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "../../CONFIG.h"

/**
 * @brief Magic value stored in calibration record.
 */
#define TEMP_CAL_MAGIC ((uint16_t)(('T' << 8) | 'C'))

/**
 * @brief Calibration record version.
 */
#define TEMP_CAL_VERSION (0x0001u)

/**
 * @brief Calibration flags.
 * @details Bitmask stored in @ref temp_calib_t::flags .
 */
enum {
    TEMP_CAL_FLAG_VALID = 1u << 0,  /**< Record validated and CRC-correct. */
    TEMP_CAL_FLAG_TWO_PT = 1u << 1, /**< Derived using two-point computation. */
};

/**
 * @brief Persistent temperature calibration record.
 */
typedef struct {
    uint16_t magic;            /**< Magic 'TC' to identify struct. */
    uint8_t version;           /**< Structure version, see @ref TEMP_CAL_VERSION. */
    uint8_t mode;              /**< @ref temp_cal_mode_t. */
    float v0_volts_at_27c;     /**< V0 at 27 °C [V] (intercept). */
    float slope_volts_per_deg; /**< S in V/°C (positive magnitude). */
    float offset_c;            /**< Residual °C offset after linear model. */
    uint32_t reserved0;        /**< Reserved for future use. */
    uint32_t reserved1;        /**< Reserved for future use. */
    uint32_t reserved2;        /**< Reserved for future use. */
    uint32_t crc32;            /**< CRC32 over all fields except crc32 (0 if unused). */
} temp_calib_t;

typedef enum {
    TEMP_CAL_MODE_NONE = 0, /**< No calibration data present. */
    TEMP_CAL_MODE_1PT = 1,  /**< Single-point calibration (offset only). */
    TEMP_CAL_MODE_2PT = 2   /**< Two-point calibration (slope + intercept). */
} temp_cal_mode_t;

/* ========================================================================== */
/*                             HLW8032 CALIBRATION                            */
/* ========================================================================== */

/**
 * @brief Write the entire sensor calibration area.
 * @param data Pointer to source buffer (typically hlw_calib_data_t)
 * @param len Must be <= EEPROM_SENSOR_CAL_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds or write error
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len);

/**
 * @brief Read the entire sensor calibration area.
 * @param data Destination buffer
 * @param len Must be <= EEPROM_SENSOR_CAL_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds error
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len);

/**
 * @brief Write calibration record for one channel.
 *
 * Stores calibration factors and offsets for a single HLW8032 channel:
 * - Voltage/current factors
 * - Zero-point offsets
 * - Resistor values (R1, R2, shunt)
 * - Calibration status flags
 *
 * @param ch Channel index [0..7]
 * @param in Pointer to calibration struct
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null input
 */
int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in);

/**
 * @brief Read calibration record for one channel.
 *
 * Reads calibration data and applies sane defaults if channel not calibrated.
 *
 * @param ch Channel index [0..7]
 * @param out Destination calibration struct
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null pointer
 */
int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out);

/* ========================================================================== */
/*                         RP2040 TEMP CALIBRATION BLOCK                      */
/* ========================================================================== */

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
int EEPROM_WriteTempCalibration(const temp_calib_t *cal);

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
int EEPROM_ReadTempCalibration(temp_calib_t *out);

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
int TempCalibration_ComputeSinglePoint(float ambient_c, uint16_t raw_temp, temp_calib_t *out);

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
                                    temp_calib_t *out);

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
int TempCalibration_ApplyToMeterTask(const temp_calib_t *cal);

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
int TempCalibration_LoadAndApply(temp_calib_t *out);

#endif /* CALIBRATION_H */

/** @} */