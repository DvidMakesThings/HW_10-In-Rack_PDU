/**
 * @file HLW8032_driver.h
 * @author DvidMakesThings - David Sipos
 * @brief Driver for HLW8032 energy measurement IC.
 * @version 1.0
 * @date 2025-05-24
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HLW8032_DRIVER_H
#define HLW8032_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

// Hardware Calibration Constants
#define HLW8032_VF (1880000.0f / 1000.0f) // Voltage divider factor (e.g., 1880K:1K)
#define HLW8032_CF (1.0f / 0.001f)        // Current factor for 1 mΩ shunt
#define UART_READ_TIMEOUT_US 200000
#define MAX_RX_BYTES 64

/**
 * @brief Initialize the HLW8032 interface.
 * This function sets up the UART for HLW8032 communication and configures the MUX.
 */
void hlw8032_init(void);

/** * @brief Read and parse measurement frame for a given channel (0–7).
 * This function reads the HLW8032 data for the specified channel and parses the frame.
 * It reads 64 bytes maximum or until a valid frame is found.
 * If a valid frame is found, it extracts the voltage, current, power, and power factor values.
 * @param channel Channel index (0–7)
 * @return true if frame read and parsed successfully, false otherwise
 */
bool hlw8032_read(uint8_t channel);

/**
 * @brief Get last parsed voltage reading (in volts).
 * This function returns the last measured voltage value.
 * @return Last voltage reading in volts
 */
float hlw8032_get_voltage(void);

/**
 * @brief Get last parsed current reading (in amps).
 * This function returns the last measured current value.
 * @return Last current reading in amps
 */
float hlw8032_get_current(void);

/**
 * @brief Get last parsed active power (in watts).
 * This function returns the last measured power value.
 * @return Last power reading in watts
 */
float hlw8032_get_power(void);

/**
 * @brief Get estimated power using V × I (not factory-calibrated).
 * This function calculates the estimated power by multiplying voltage and current.
 * @return Estimated power in watts
 */
float hlw8032_get_power_inspect(void);

/**
 * @brief Get estimated power factor (Active / Apparent).
 * This function calculates the power factor based on the last measured power and apparent power.
 * @return Power factor as a float
 */
float hlw8032_get_power_factor(void);

/**
 * @brief Get estimated energy in kilowatt-hours.
 * This function calculates the total energy consumed in kWh based on the power factor and count.
 * It uses the formula: kWh = (PF * PF_Count) / (imp_per_wh * 3600)
 * where imp_per_wh is derived from the power parameters.
 * @return Estimated energy in kilowatt-hours
 */
float hlw8032_get_kwh(void);

#endif // HLW8032_DRIVER_H
