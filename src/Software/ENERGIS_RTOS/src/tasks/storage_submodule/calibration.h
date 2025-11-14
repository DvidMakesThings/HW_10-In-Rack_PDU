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

#endif /* CALIBRATION_H */

/** @} */