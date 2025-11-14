/**
 * @file src/tasks/storage_submodule/calibration.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of sensor calibration management for HLW8032 power meters.
 * Stores and retrieves per-channel calibration factors, offsets, and resistor values.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

/**
 * @brief Write entire sensor calibration area to EEPROM.
 *
 * Typically used to write all channel calibration data at once (hlw_calib_data_t).
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Source buffer containing calibration data
 * @param len Number of bytes to write
 * @return 0 on success, -1 on bounds check failure or I2C error
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_SENSOR_CAL_START, data, (uint16_t)len);
}

/**
 * @brief Read entire sensor calibration area from EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len Number of bytes to read
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_SENSOR_CAL_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Write calibration record for a single channel.
 *
 * Calculates EEPROM address for specified channel and writes calibration structure.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param ch Channel index [0..7]
 * @param in Pointer to calibration data structure
 * @return 0 on success, -1 on invalid channel or null pointer
 */
int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in) {
    /* Validate inputs */
    if (ch >= 8 || !in)
        return -1;

    /* Calculate address for this channel */
    uint16_t addr = EEPROM_SENSOR_CAL_START + (ch * sizeof(hlw_calib_t));

    /* Write calibration structure */
    return CAT24C512_WriteBuffer(addr, (const uint8_t *)in, sizeof(hlw_calib_t));
}

/**
 * @brief Read calibration record for a single channel.
 *
 * Reads calibration data from EEPROM. If channel is not calibrated (flag != 0xCA),
 * applies default HLW8032 factors and resistor values.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param ch Channel index [0..7]
 * @param out Pointer to calibration data structure to fill
 * @return 0 on success, -1 on invalid channel or null pointer
 */
int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out) {
    /* Validate inputs */
    if (ch >= 8 || !out)
        return -1;

    /* Calculate address for this channel */
    uint16_t addr = EEPROM_SENSOR_CAL_START + (ch * sizeof(hlw_calib_t));

    /* Read calibration structure from EEPROM */
    CAT24C512_ReadBuffer(addr, (uint8_t *)out, sizeof(hlw_calib_t));

    /* Apply default factors if not calibrated */
    if (out->calibrated != 0xCA) {
        out->voltage_factor = HLW8032_VF;
        out->current_factor = HLW8032_CF;
        out->r1_actual = 1880000.0f;
        out->r2_actual = 1000.0f;
        out->shunt_actual = 0.001f;
    }

    return 0;
}