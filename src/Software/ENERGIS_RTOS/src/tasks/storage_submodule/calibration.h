/**
 * @file src/tasks/storage_submodule/calibration.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage01 1. Sensor Calibration
 * @ingroup tasks10
 * @brief HLW8032 power meter calibration per channel
 * @{
 *
 * @version 1.2
 * @date 2025-11-15
 *
 * @details
 * This module manages calibration data for two sensor subsystems: HLW8032 power meters
 * (8 channels) and RP2040 internal temperature sensor. Provides EEPROM persistence,
 * calibration computation helpers, and runtime application to MeterTask.
 *
 * Key Features:
 * - Per-channel HLW8032 calibration with voltage/current factors
 * - RP2040 temperature sensor calibration (1-point or 2-point)
 * - EEPROM persistence with bounds checking
 * - Calibration computation helpers for temperature
 * - Runtime application to MeterTask measurement path
 *
 * HLW8032 Calibration:
 * - 8 independent channels with individual calibration records
 * - Each channel stores:
 *   * Voltage factor (HLW8032_VF default: 0.596)
 *   * Current factor (HLW8032_CF default: 14.871)
 *   * Voltage offset (zero-point compensation)
 *   * Current offset (zero-point compensation)
 *   * Resistor divider values (R1, R2, shunt)
 *   * Calibration status flags
 * - Channel calibration stored as hlw_calib_t structure
 * - EEPROM location: EEPROM_SENSOR_CAL_START + (ch * sizeof(hlw_calib_t))
 * - Bulk read/write for all channels or individual channel access
 *
 * Temperature Calibration:
 * - RP2040 internal temperature sensor compensation
 * - Two calibration modes:
 *   * TEMP_CAL_MODE_1PT: Single-point offset calibration
 *   * TEMP_CAL_MODE_2PT: Two-point slope + intercept calibration
 * - Stored as temp_calib_t structure with magic/version/CRC32
 * - Default model: V0=0.706V@27°C, slope=0.001721V/°C
 * - EEPROM location: EEPROM_TEMP_CAL_START (separate block)
 * - Computation functions for deriving calibration from measurements
 *
 * EEPROM Layout:
 * 1. HLW8032 Calibration Block (EEPROM_SENSOR_CAL_START):
 *    - Size: EEPROM_SENSOR_CAL_SIZE bytes
 *    - Contents: 8 × hlw_calib_t structures (one per channel)
 *    - Layout: Channel 0 @ offset 0, Channel 1 @ offset sizeof(hlw_calib_t), etc.
 *
 * 2. Temperature Calibration Block (EEPROM_TEMP_CAL_START):
 *    - Size: EEPROM_TEMP_CAL_SIZE bytes
 *    - Contents: temp_calib_t structure with magic/version/CRC32
 *    - Magic: 'TC' (0x5443) for validity detection
 *    - Version: TEMP_CAL_VERSION (0x0001)
 *
 * Temperature Calibration Workflow:
 * 1. Measure ADC raw value at known temperature(s)
 * 2. Call TempCalibration_ComputeSinglePoint() or TempCalibration_ComputeTwoPoint()
 * 3. Write computed record to EEPROM via EEPROM_WriteTempCalibration()
 * 4. Apply to MeterTask via TempCalibration_ApplyToMeterTask()
 * 5. On boot, use TempCalibration_LoadAndApply() for automatic load+apply
 *
 * Single-Point Calibration:
 * - Measures offset only, assumes default slope
 * - Requires one known temperature measurement
 * - Suitable for reducing ambient temperature error
 * - Sets offset so T_reported = T_actual at calibration point
 *
 * Two-Point Calibration:
 * - Measures both slope and V0 intercept
 * - Requires two temperature measurements (e.g., ambient + warmed)
 * - Better accuracy across temperature range
 * - Compensates for per-die slope variation
 *
 * Thread Safety:
 * - All EEPROM read/write functions require eepromMtx held by caller
 * - Calibration computation functions are reentrant (no EEPROM access)
 * - TempCalibration_ApplyToMeterTask() sends queue message to MeterTask
 * - TempCalibration_LoadAndApply() manages mutex internally
 *
 * Integration Points:
 * - MeterTask: Consumes calibration data for measurements
 * - StorageTask: Provides eepromMtx for thread-safe access
 * - factory_defaults: Writes default calibration on first boot
 * - Console commands: UART calibration workflow
 *
 * Error Handling:
 * - Bounds checking on all EEPROM operations
 * - Null pointer checks on all function inputs
 * - Invalid channel index rejection
 * - Magic/version validation for temperature calibration
 * - CRC32 validation (optional, currently not enforced)
 * - Error codes logged and enqueued to event log
 *
 * @note Always call EEPROM functions with eepromMtx held.
 * @note Temperature calibration applies globally, not per-channel.
 * @note HLW8032 calibration is per-channel and independent.
 * @note Calibration changes do not take effect until applied to MeterTask.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "../../CONFIG.h"

/** @name Temperature Calibration Constants
 * @{ */
/**
 * @brief Magic value stored in calibration record.
 */
#define TEMP_CAL_MAGIC ((uint16_t)(('T' << 8) | 'C'))

/**
 * @brief Calibration record version.
 */
#define TEMP_CAL_VERSION (0x0001u)
/** @} */

/** @name Calibration Flags
 * @{ */
/**
 * @brief Calibration flags.
 * @details Bitmask stored in @ref temp_calib_t::flags .
 */
enum {
    TEMP_CAL_FLAG_VALID = 1u << 0,  /**< Record validated and CRC-correct. */
    TEMP_CAL_FLAG_TWO_PT = 1u << 1, /**< Derived using two-point computation. */
};
/** @} */

/**
 * @brief Persistent temperature calibration record.
 */
/** @struct temp_calib_t */
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

/** @enum temp_cal_mode_t */
typedef enum {
    TEMP_CAL_MODE_NONE = 0, /**< No calibration data present. */
    TEMP_CAL_MODE_1PT = 1,  /**< Single-point calibration (offset only). */
    TEMP_CAL_MODE_2PT = 2   /**< Two-point calibration (slope + intercept). */
} temp_cal_mode_t;

/* ========================================================================== */
/*                             HLW8032 CALIBRATION                            */
/* ========================================================================== */

/** @name HLW8032 Calibration APIs
 * @{ */
/**
 * @brief Write entire HLW8032 sensor calibration block to EEPROM.
 *
 * Bulk write operation for all 8 channels of HLW8032 calibration data. Typically
 * used to write complete hlw_calib_data_t array containing calibration parameters
 * for all channels at once.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_SENSOR_CAL_START
 * - Maximum size: EEPROM_SENSOR_CAL_SIZE bytes
 * - Contents: Array of hlw_calib_t structures (8 channels)
 *
 * @param[in] data Pointer to source buffer containing calibration data.
 * @param[in] len  Number of bytes to write. Must be <= EEPROM_SENSOR_CAL_SIZE.
 *
 * @return 0 if write successful.
 * @return -1 if length exceeds EEPROM_SENSOR_CAL_SIZE or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Bounds checking performed before write operation.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note For single-channel updates, use EEPROM_WriteSensorCalibrationForChannel().
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len);

/**
 * @brief Read entire HLW8032 sensor calibration block from EEPROM.
 *
 * Bulk read operation for all 8 channels of HLW8032 calibration data. Reads
 * complete calibration block into provided buffer.
 *
 * EEPROM Layout:
 * - Start address: EEPROM_SENSOR_CAL_START
 * - Maximum size: EEPROM_SENSOR_CAL_SIZE bytes
 * - Contents: Array of hlw_calib_t structures (8 channels)
 *
 * @param[out] data Destination buffer for calibration data.
 * @param[in]  len  Number of bytes to read. Must be <= EEPROM_SENSOR_CAL_SIZE.
 *
 * @return 0 if read successful.
 * @return -1 if length exceeds EEPROM_SENSOR_CAL_SIZE.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Bounds checking performed before read operation.
 * @note Read failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note For single-channel reads, use EEPROM_ReadSensorCalibrationForChannel().
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len);

/**
 * @brief Write calibration record for single HLW8032 channel.
 *
 * Per-channel write operation for HLW8032 power meter calibration. Calculates
 * EEPROM address for specified channel and writes hlw_calib_t structure containing
 * voltage/current factors, offsets, and resistor values.
 *
 * Calibration Parameters:
 * - Voltage factor: Scaling coefficient for voltage measurements
 * - Current factor: Scaling coefficient for current measurements
 * - Voltage offset: Zero-point compensation for voltage
 * - Current offset: Zero-point compensation for current
 * - R1 actual: Voltage divider resistor R1 value (typically 1880kΩ)
 * - R2 actual: Voltage divider resistor R2 value (typically 1kΩ)
 * - Shunt actual: Current shunt resistance (typically 0.001Ω)
 * - Calibrated flag: Indicates if channel has been calibrated
 * - Zero calibrated flag: Indicates if zero-point calibration performed
 *
 * EEPROM Address Calculation:
 * - Base: EEPROM_SENSOR_CAL_START
 * - Offset: ch × sizeof(hlw_calib_t)
 * - Channel 0: EEPROM_SENSOR_CAL_START + 0
 * - Channel 7: EEPROM_SENSOR_CAL_START + (7 × sizeof(hlw_calib_t))
 *
 * @param[in] ch Channel index [0..7].
 * @param[in] in Pointer to hlw_calib_t structure containing calibration data.
 *
 * @return 0 if write successful.
 * @return -1 if ch >= 8 or in is NULL.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Channel index validated before write operation.
 * @note Null pointer check performed on input structure.
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 */
int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in);

/**
 * @brief Read calibration record for single HLW8032 channel.
 *
 * Per-channel read operation for HLW8032 power meter calibration. Reads
 * hlw_calib_t structure from calculated EEPROM address for specified channel.
 *
 * EEPROM Address Calculation:
 * - Base: EEPROM_SENSOR_CAL_START
 * - Offset: ch × sizeof(hlw_calib_t)
 * - Channel 0: EEPROM_SENSOR_CAL_START + 0
 * - Channel 7: EEPROM_SENSOR_CAL_START + (7 × sizeof(hlw_calib_t))
 *
 * @param[in]  ch  Channel index [0..7].
 * @param[out] out Pointer to hlw_calib_t structure to receive calibration data.
 *
 * @return 0 if read successful.
 * @return -1 if ch >= 8 or out is NULL.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Channel index validated before read operation.
 * @note Null pointer check performed on output structure.
 * @note Read failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 * @note Caller responsible for interpreting calibration flags.
 */
int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out);
/** @} */

/* ========================================================================== */
/*                         RP2040 TEMP CALIBRATION BLOCK                      */
/* ========================================================================== */

/** @name RP2040 Temperature Calibration APIs
 * @{ */
/**
 * @brief Write RP2040 temperature calibration record to EEPROM.
 *
 * Writes temp_calib_t structure containing temperature sensor calibration parameters
 * to dedicated EEPROM block. Automatically sets magic value and version fields.
 * Supports both 1-point (offset only) and 2-point (slope + intercept) calibration.
 *
 * Calibration Record Contents:
 * - Magic: TEMP_CAL_MAGIC ('TC' = 0x5443) for validity detection
 * - Version: TEMP_CAL_VERSION (0x0001) for structure compatibility
 * - Mode: TEMP_CAL_MODE_1PT or TEMP_CAL_MODE_2PT
 * - V0: Voltage at 27°C reference point [V]
 * - Slope: Temperature coefficient [V/°C]
 * - Offset: Residual temperature offset [°C]
 * - Reserved: 3× uint32_t for future use
 * - CRC32: Checksum (currently 0, not enforced)
 *
 * EEPROM Layout:
 * - Start address: EEPROM_TEMP_CAL_START
 * - Size: EEPROM_TEMP_CAL_SIZE bytes (sizeof(temp_calib_t))
 *
 * @param[in] cal Pointer to temp_calib_t structure containing calibration data.
 *
 * @return 0 if write successful.
 * @return -1 if cal is NULL or I2C write fails.
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Magic and version fields set automatically if not already set.
 * @note CRC32 field currently not enforced (set to 0).
 * @note Write failures logged as ERR_SEV_ERROR if ERRORLOGGER enabled.
 */
int EEPROM_WriteTempCalibration(const temp_calib_t *cal);

/**
 * @brief Read RP2040 temperature calibration record from EEPROM.
 *
 * Reads temp_calib_t structure from dedicated EEPROM block and validates magic
 * value and version. Returns safe defaults if calibration record invalid or not
 * present.
 *
 * Validation Checks:
 * - Magic value matches TEMP_CAL_MAGIC ('TC' = 0x5443)
 * - Version matches TEMP_CAL_VERSION (0x0001)
 * - Mode is valid (TEMP_CAL_MODE_1PT or TEMP_CAL_MODE_2PT)
 *
 * Default Calibration (if invalid):
 * - Mode: TEMP_CAL_MODE_NONE
 * - V0: 0.706V (RP2040 typical at 27°C)
 * - Slope: 0.001721 V/°C (RP2040 typical)
 * - Offset: 0.0°C
 *
 * EEPROM Layout:
 * - Start address: EEPROM_TEMP_CAL_START
 * - Size: EEPROM_TEMP_CAL_SIZE bytes (sizeof(temp_calib_t))
 *
 * @param[out] out Pointer to temp_calib_t structure to receive calibration data.
 *
 * @return 0 if valid calibration record loaded.
 * @return -1 if magic/version invalid or not present (defaults returned).
 *
 * @warning Caller MUST hold eepromMtx before calling this function.
 * @note Null pointer check performed on output structure.
 * @note Always fills output structure with either valid data or defaults.
 * @note CRC32 validation not currently enforced.
 */
int EEPROM_ReadTempCalibration(temp_calib_t *out);

/**
 * @brief Compute single-point temperature calibration from reference measurement.
 *
 * Calculates offset-only calibration from a single known temperature measurement.
 * Uses default RP2040 temperature model (V0=0.706V@27°C, slope=0.001721V/°C) to
 * compute raw temperature, then determines offset needed to match actual temperature.
 *
 * Calibration Algorithm:
 * 1. Convert raw_temp ADC value to voltage
 * 2. Calculate uncalibrated temperature using default model
 * 3. Compute offset: offset_c = ambient_c - T_uncalibrated
 * 4. Populate output structure with mode=TEMP_CAL_MODE_1PT
 *
 * Use Case:
 * - Quick calibration with single reference thermometer
 * - Reduces ambient temperature measurement error
 * - Does not compensate for slope variation
 *
 * Typical Workflow:
 * 1. Measure ambient temperature with reference thermometer
 * 2. Read ADC raw value from RP2040 temperature sensor
 * 3. Call this function to compute calibration
 * 4. Write result to EEPROM via EEPROM_WriteTempCalibration()
 * 5. Apply to MeterTask via TempCalibration_ApplyToMeterTask()
 *
 * @param[in]  ambient_c True ambient temperature [°C] from reference.
 * @param[in]  raw_temp  ADC raw code from RP2040 temp sensor (AINSEL=4).
 * @param[out] out       Output temp_calib_t structure (mode set to TEMP_CAL_MODE_1PT).
 *
 * @return 0 if computation successful.
 * @return -1 if out is NULL or inputs out of valid range.
 *
 * @note Does not access EEPROM; pure computation function.
 * @note Output structure ready for EEPROM_WriteTempCalibration().
 * @note Assumes default RP2040 slope (0.001721 V/°C).
 */
int TempCalibration_ComputeSinglePoint(float ambient_c, uint16_t raw_temp, temp_calib_t *out);

/**
 * @brief Compute two-point temperature calibration from reference measurements.
 *
 * Calculates slope and intercept calibration from two known temperature measurements.
 * Derives both V0 (voltage at 27°C) and slope (V/°C) from the two data points. More
 * accurate than single-point calibration across wider temperature range.
 *
 * Calibration Algorithm:
 * 1. Convert both raw ADC values to voltages
 * 2. Calculate slope: (V2 - V1) / (T2 - T1)
 * 3. Calculate V0 at 27°C reference point
 * 4. Set offset to 0 (linear model only)
 * 5. Populate output structure with mode=TEMP_CAL_MODE_2PT
 *
 * Use Case:
 * - High-accuracy calibration with two reference temperatures
 * - Compensates for per-die slope variation
 * - Better accuracy across full temperature range
 *
 * Typical Workflow:
 * 1. Measure at ambient temperature (e.g., 25°C)
 * 2. Record ADC raw value at ambient
 * 3. Warm device to higher temperature (e.g., 40°C)
 * 4. Record ADC raw value at elevated temperature
 * 5. Call this function with both measurement pairs
 * 6. Optionally apply single-point trim for residual at ambient
 * 7. Write result to EEPROM and apply to MeterTask
 *
 * @param[in]  t1_c True temperature at point 1 [°C] (e.g., ambient).
 * @param[in]  raw1 ADC raw code at point 1.
 * @param[in]  t2_c True temperature at point 2 [°C] (e.g., warmed).
 * @param[in]  raw2 ADC raw code at point 2.
 * @param[out] out  Output temp_calib_t structure (mode set to TEMP_CAL_MODE_2PT).
 *
 * @return 0 if computation successful.
 * @return -1 if out is NULL, inputs invalid, or computed parameters out of range.
 *
 * @note Does not access EEPROM; pure computation function.
 * @note Temperature points should differ by at least 10°C for accuracy.
 * @note Output offset set to 0; caller may apply single-point trim if desired.
 * @note Validates computed slope and V0 are within reasonable bounds.
 */
int TempCalibration_ComputeTwoPoint(float t1_c, uint16_t raw1, float t2_c, uint16_t raw2,
                                    temp_calib_t *out);

/**
 * @brief Apply temperature calibration to MeterTask measurement path.
 *
 * Sends calibration parameters (V0, slope, offset) to MeterTask via queue message.
 * MeterTask applies these parameters to all temperature conversions, affecting
 * console output, /api/status, and /metrics endpoints. Does not persist to EEPROM.
 *
 * Calibration Parameters Applied:
 * - V0: Voltage at 27°C reference [V]
 * - Slope: Temperature coefficient [V/°C]
 * - Offset: Residual temperature correction [°C]
 *
 * Temperature Conversion Formula (after calibration):
 * T_calibrated = ((V_adc - V0) / slope) + 27.0 + offset
 *
 * @param[in] cal Pointer to temp_calib_t structure with calibration parameters.
 *
 * @return 0 if calibration applied successfully.
 * @return -1 if cal is NULL or MeterTask rejects parameters.
 *
 * @note Does NOT write to EEPROM; caller responsible for persistence.
 * @note Calibration takes effect immediately on next temperature reading.
 * @note MeterTask queue must be available (called after MeterTask initialized).
 * @note Does NOT require eepromMtx (no EEPROM access).
 */
int TempCalibration_ApplyToMeterTask(const temp_calib_t *cal);

/**
 * @brief Load temperature calibration from EEPROM and apply to MeterTask.
 *
 * Convenience function combining EEPROM read, validation, and MeterTask application.
 * Reads calibration record from EEPROM, validates magic/version, and applies to
 * MeterTask. If invalid, applies safe defaults. Typical usage during boot sequence.
 *
 * Operation Flow:
 * 1. Call EEPROM_ReadTempCalibration() with eepromMtx held
 * 2. Validate magic value and version
 * 3. If valid, apply calibration to MeterTask
 * 4. If invalid, apply defaults (mode NONE) to MeterTask
 * 5. Optionally return loaded/default record to caller
 *
 * Default Parameters (if invalid):
 * - Mode: TEMP_CAL_MODE_NONE
 * - V0: 0.706V (RP2040 typical)
 * - Slope: 0.001721 V/°C (RP2040 typical)
 * - Offset: 0.0°C
 *
 * @param[out] out Optional pointer to receive loaded or default calibration.
 *                 Pass NULL if record not needed.
 *
 * @return 0 if valid calibration loaded and applied.
 * @return -1 if invalid/missing calibration (defaults applied instead).
 *
 * @warning Caller MUST hold eepromMtx before calling (for EEPROM read).
 * @note Always applies some calibration (either valid or defaults).
 * @note Called during InitTask boot sequence.
 * @note MeterTask must be initialized before calling.
 */
int TempCalibration_LoadAndApply(temp_calib_t *out);
/** @} */

#endif /* CALIBRATION_H */

/** @} */