/**
 * @file src/snmp/snmp_voltageMon.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp05 5. SNMP Agent - Voltage Monitoring
 * @ingroup snmp
 * @brief RP2040 internal rails, die temperature, and project voltage monitoring.
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details
 * This module provides SNMP GET callbacks that expose RP2040 internal voltage
 * rails, on-die temperature sensor, and project-specific power supply voltages.
 * All measurements are retrieved from MeterTask's system telemetry cache.
 *
 * Monitored Parameters:
 *
 * RP2040 Internal Measurements:
 * - Die Temperature Sensor Voltage - Raw ADC voltage from internal temperature sensor
 * - Die Temperature - Converted temperature in Celsius
 * - Core VREG Voltage - Adjustable core voltage regulator setting (0.85V + 0.05V * VSEL)
 * - Core VREG Status - Regulator status (OK/Overload/Hi-Z)
 * - Bandgap Reference - Fixed 1.10V reference
 * - USB PHY Rail - Fixed 1.80V USB PHY supply
 * - IO Rail - Fixed 3.30V IO supply
 *
 * Project-Specific Measurements:
 * - V_SUPPLY - Main power supply voltage (measured via voltage divider)
 * - V_USB - USB power supply voltage (measured via voltage divider)
 * - V_SUPPLY Divider Tap - Pre-divider voltage at ADC input
 * - V_USB Divider Tap - Pre-divider voltage at ADC input
 *
 * All voltage measurements are returned as ASCII float strings with appropriate
 * precision (typically 2-3 decimals). Temperature is reported in degrees Celsius.
 * Fixed voltages return constant string values for efficiency.
 *
 * Telemetry Source:
 * - Internal sensors: RP2040 ADC channel 4 (temperature sensor)
 * - External voltages: RP2040 ADC channels with resistive dividers
 * - All readings cached by MeterTask at approximately 1 Hz
 *
 * @note All functions are RTOS-safe and non-blocking.
 * @note Returns zeros if telemetry unavailable or invalid.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_VOLTAGE_MON_H
#define SNMP_VOLTAGE_MON_H

#include "../CONFIG.h"

/** @name Public API
 * @{
 */

/**
 * @brief SNMP getter for on-die temperature sensor voltage [V].
 *
 * Returns the raw ADC voltage from the RP2040 internal temperature sensor.
 * Calculated as: V = raw_adc_count * (ADC_VREF / ADC_MAX).
 *
 * @param buf Output buffer (minimum 16 bytes recommended)
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII float with 5 decimals (e.g., "0.72345").
 * @note Returns "0.00000" if telemetry unavailable.
 */
void get_tempSensorVoltage(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for on-die temperature [°C].
 *
 * Returns the RP2040 internal die temperature converted from the ADC reading
 * using the chip-specific calibration formula.
 *
 * @param buf Output buffer (minimum 16 bytes recommended)
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII float with 3 decimals (e.g., "42.150").
 * @note Returns "0.000" if telemetry unavailable.
 */
void get_tempSensorTemperature(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for V_SUPPLY rail voltage [V].
 *
 * Returns the main power supply voltage measured via voltage divider and ADC.
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII float with 3 decimals.
 * @note Returns "0.000" if telemetry unavailable.
 */
void get_VSUPPLY(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for V_USB rail voltage [V].
 *
 * Returns the USB power supply voltage measured via voltage divider and ADC.
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII float with 3 decimals.
 * @note Returns "0.000" if telemetry unavailable.
 */
void get_VUSB(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for V_SUPPLY divider tap voltage [V].
 *
 * Returns the pre-divider voltage at the ADC input for V_SUPPLY measurement.
 * Useful for diagnostics and divider calibration.
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII float with 3 decimals.
 * @note Returns "0.000" if telemetry unavailable.
 */
void get_VSUPPLY_divider(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for V_USB divider tap voltage [V].
 *
 * Returns the pre-divider voltage at the ADC input for V_USB measurement.
 * Useful for diagnostics and divider calibration.
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII float with 3 decimals.
 * @note Returns "0.000" if telemetry unavailable.
 */
void get_VUSB_divider(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for core VREG voltage [V].
 *
 * Returns the RP2040 core voltage regulator output voltage. Calculated from
 * VREG register: V = 0.85 + 0.05 * VSEL (where VSEL = bits[3:0]).
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII float with 2 decimals (e.g., "1.10").
 * @note Typical range: 0.85V to 1.30V.
 */
void get_coreVREG(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for core VREG status.
 *
 * Returns the RP2040 core voltage regulator status as a text string.
 * Possible values: "OK", "Overload", "Hi-Z", "Unknown".
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Returns ASCII string (not numeric).
 */
void get_coreVREG_status(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for bandgap reference voltage [V].
 *
 * Returns the RP2040 internal bandgap reference voltage (fixed at 1.10V).
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Always returns "1.10" (constant).
 */
void get_bandgapRef(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for USB PHY rail voltage [V].
 *
 * Returns the RP2040 USB PHY supply voltage (fixed at 1.80V).
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Always returns "1.80" (constant).
 */
void get_usbPHYrail(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for IO rail voltage [V].
 *
 * Returns the RP2040 IO supply voltage (fixed at 3.30V).
 *
 * @param buf Output buffer
 * @param len Pointer to receive string length
 *
 * @return None
 *
 * @note Always returns "3.30" (constant).
 */
void get_ioRail(void *buf, uint8_t *len);

/** @} */

#endif

/** @} */