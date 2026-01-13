/**
 * @file src/snmp/snmp_powerMon.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp04 4. SNMP Agent - Power Monitoring
 * @ingroup snmp
 * @brief Per-channel power telemetry and overcurrent protection status via SNMP.
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-08
 *
 * @details
 * This module provides SNMP GET callbacks that expose per-outlet power telemetry
 * and system-wide overcurrent protection status. All telemetry data is retrieved
 * from MeterTask's canonical cache for efficient non-blocking access.
 *
 * Per-Channel Telemetry (8 outlets):
 * - Voltage [V] - RMS voltage measurement (ASCII float, 2 decimals)
 * - Current [A] - RMS current measurement (ASCII float, 3 decimals)
 * - Power [W] - Active power measurement (ASCII float, 1 decimal)
 * - Power Factor - Dimensionless ratio 0..1 (ASCII float, 3 decimals)
 * - Energy [kWh] - Accumulated energy since outlet powered on (ASCII float, 3 decimals)
 * - Uptime [s] - Time since outlet last powered on (ASCII integer)
 *
 * Overcurrent Protection (OCP) Status:
 * - State - NORMAL/WARNING/CRITICAL/LOCKOUT (INTEGER 0..3)
 * - Total Current - Sum of all outlet currents (ASCII float, 3 decimals)
 * - Configured Limit - OCP trip threshold (ASCII float, 2 decimals)
 * - Warning/Critical/Recovery Thresholds (ASCII float, 2 decimals)
 * - Last Tripped Channel - 1-based outlet index or 0 (INTEGER)
 * - Trip Counter - Number of trips since boot (INTEGER)
 * - Last Trip Timestamp - Milliseconds since boot (INTEGER)
 * - Switching Allowed - 0=LOCKOUT, 1=allowed (INTEGER)
 * - Reset Control - Write non-zero to clear lockout (SET-only)
 *
 * All telemetry reads are non-blocking and RTOS-safe. If telemetry is invalid
 * or unavailable, functions return zero values rather than blocking or failing.
 *
 * @note All functions can be safely called from SNMP agent context.
 * @note Telemetry cache is updated by MeterTask at approximately 1 Hz.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_POWER_MON_H
#define SNMP_POWER_MON_H

#include "../CONFIG.h"

/** @name Channel Telemetry
 * @{
 */

/**
 * @brief SNMP getter for outlet 0 voltage [V].
 * @param buf Output buffer (minimum 16 bytes recommended)
 * @param len Pointer to receive string length
 * @return None
 * @note Returns ASCII float with 2 decimals (e.g., "230.15").
 */
void get_power_0_MEAS_VOLTAGE(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 0 current [A].
 * @param buf Output buffer
 * @param len Pointer to receive string length
 * @return None
 * @note Returns ASCII float with 3 decimals.
 */
void get_power_0_MEAS_CURRENT(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 0 power [W].
 * @param buf Output buffer
 * @param len Pointer to receive string length
 * @return None
 * @note Returns ASCII float with 1 decimal.
 */
void get_power_0_MEAS_WATT(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 0 power factor.
 * @param buf Output buffer
 * @param len Pointer to receive string length
 * @return None
 * @note Returns ASCII float with 3 decimals (0..1 range).
 */
void get_power_0_MEAS_PF(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 0 accumulated energy [kWh].
 * @param buf Output buffer
 * @param len Pointer to receive string length
 * @return None
 * @note Returns ASCII float with 3 decimals.
 */
void get_power_0_MEAS_KWH(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 0 uptime [s].
 * @param buf Output buffer
 * @param len Pointer to receive string length
 * @return None
 * @note Returns ASCII unsigned integer.
 */
void get_power_0_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_1_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_1_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_1_MEAS_WATT(void *buf, uint8_t *len);
void get_power_1_MEAS_PF(void *buf, uint8_t *len);
void get_power_1_MEAS_KWH(void *buf, uint8_t *len);
void get_power_1_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_2_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_2_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_2_MEAS_WATT(void *buf, uint8_t *len);
void get_power_2_MEAS_PF(void *buf, uint8_t *len);
void get_power_2_MEAS_KWH(void *buf, uint8_t *len);
void get_power_2_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_3_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_3_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_3_MEAS_WATT(void *buf, uint8_t *len);
void get_power_3_MEAS_PF(void *buf, uint8_t *len);
void get_power_3_MEAS_KWH(void *buf, uint8_t *len);
void get_power_3_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_4_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_4_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_4_MEAS_WATT(void *buf, uint8_t *len);
void get_power_4_MEAS_PF(void *buf, uint8_t *len);
void get_power_4_MEAS_KWH(void *buf, uint8_t *len);
void get_power_4_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_5_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_5_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_5_MEAS_WATT(void *buf, uint8_t *len);
void get_power_5_MEAS_PF(void *buf, uint8_t *len);
void get_power_5_MEAS_KWH(void *buf, uint8_t *len);
void get_power_5_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_6_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_6_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_6_MEAS_WATT(void *buf, uint8_t *len);
void get_power_6_MEAS_PF(void *buf, uint8_t *len);
void get_power_6_MEAS_KWH(void *buf, uint8_t *len);
void get_power_6_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_7_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_7_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_7_MEAS_WATT(void *buf, uint8_t *len);
void get_power_7_MEAS_PF(void *buf, uint8_t *len);
void get_power_7_MEAS_KWH(void *buf, uint8_t *len);
void get_power_7_MEAS_UPTIME(void *buf, uint8_t *len);

/** @} */

/** @name OCP API
 * @{
 */

/* =====================  Overcurrent Protection (OCP) SNMP  ===================== */

/**
 * @brief SNMP getter for overcurrent protection state.
 *
 * @details
 * Returns an SNMP INTEGER (4 bytes) mapping the internal OCP state:
 * - 0: NORMAL
 * - 1: WARNING
 * - 2: CRITICAL
 * - 3: LOCKOUT
 *
 * @param buf Output buffer; writes a 32-bit little-endian integer.
 * @param len Output length; always set to 4.
 */
void get_ocp_STATE(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for total measured current across all outlets [A].
 *
 * @details Encoded as SNMP OCTET STRING containing ASCII float with 3 decimals.
 *
 * @param buf Output buffer (min 16 bytes recommended).
 * @param len Output length in bytes.
 */
void get_ocp_TOTAL_CURRENT_A(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for configured OCP current limit [A].
 *
 * @details Encoded as SNMP OCTET STRING containing ASCII float with 2 decimals.
 *
 * @param buf Output buffer (min 16 bytes recommended).
 * @param len Output length in bytes.
 */
void get_ocp_LIMIT_A(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for OCP warning threshold [A].
 *
 * @details Encoded as SNMP OCTET STRING containing ASCII float with 2 decimals.
 *
 * @param buf Output buffer (min 16 bytes recommended).
 * @param len Output length in bytes.
 */
void get_ocp_WARNING_THRESHOLD_A(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for OCP critical threshold [A].
 *
 * @details Encoded as SNMP OCTET STRING containing ASCII float with 2 decimals.
 *
 * @param buf Output buffer (min 16 bytes recommended).
 * @param len Output length in bytes.
 */
void get_ocp_CRITICAL_THRESHOLD_A(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for OCP recovery threshold [A].
 *
 * @details Encoded as SNMP OCTET STRING containing ASCII float with 2 decimals.
 *
 * @param buf Output buffer (min 16 bytes recommended).
 * @param len Output length in bytes.
 */
void get_ocp_RECOVERY_THRESHOLD_A(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for last tripped channel.
 *
 * @details
 * Encoded as SNMP INTEGER (4 bytes):
 * - 1..8 : 1-based outlet index that was switched OFF due to a trip
 * - 0    : not available / no targeted channel (fallback)
 *
 * @param buf Output buffer; writes a 32-bit little-endian integer.
 * @param len Output length; always set to 4.
 */
void get_ocp_LAST_TRIPPED_CH(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for OCP trip count since boot.
 *
 * @details Encoded as SNMP INTEGER (4 bytes).
 *
 * @param buf Output buffer; writes a 32-bit little-endian integer.
 * @param len Output length; always set to 4.
 */
void get_ocp_TRIP_COUNT(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for last trip timestamp in milliseconds since boot.
 *
 * @details Encoded as SNMP INTEGER (4 bytes).
 *
 * @param buf Output buffer; writes a 32-bit little-endian integer.
 * @param len Output length; always set to 4.
 */
void get_ocp_LAST_TRIP_MS(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for switching-allowed flag.
 *
 * @details Encoded as SNMP INTEGER (4 bytes):
 * - 0 : switching blocked (LOCKOUT)
 * - 1 : switching allowed
 *
 * @param buf Output buffer; writes a 32-bit little-endian integer.
 * @param len Output length; always set to 4.
 */
void get_ocp_SWITCHING_ALLOWED(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for OCP reset control value.
 *
 * @details
 * This is a write-oriented control OID. The getter returns 0.
 *
 * @param buf Output buffer; writes a 32-bit little-endian integer.
 * @param len Output length; always set to 4.
 */
void get_ocp_RESET(void *buf, uint8_t *len);

/**
 * @brief SNMP setter to clear OCP lockout.
 *
 * @details
 * Any non-zero value triggers Overcurrent_ClearLockout().
 *
 * @param v SNMP INTEGER value provided by SNMP SET.
 */
void set_ocp_RESET(int32_t v);

/** @} */

#endif

/** @} */
