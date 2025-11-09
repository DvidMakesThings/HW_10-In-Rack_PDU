/**
 * @file snmp_powerMon.h
 * @author DvidMakesThings - David Sipos
 * @brief Custom SNMP functions for the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-05-19
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef _SNMP_POWERMON_H_
#define _SNMP_POWERMON_H_

#include "CONFIG.h"
#include "snmp.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Measurement type selector for HLW8032 SNMP callbacks.
 */
typedef enum {
    MEAS_VOLTAGE, /**< Voltage in V (RMS, float, 2 decimals) */
    MEAS_CURRENT, /**< Current in A (RMS, float, 3 decimals) */
    MEAS_WATT,    /**< Active power in W (float, 1 decimal) */
    MEAS_PF,      /**< Power factor (float, 3 decimals) */
    MEAS_KWH,     /**< Accumulated energy in kWh (float, 5 decimals) */
    MEAS_UPTIME   /**< Uptime in seconds (uint32 ASCII) */
} meas_type_t;

/**
 * @brief Context structure used to bind channel + measurement type to OID entries.
 */
typedef struct {
    uint8_t channel;  /**< HLW8032 channel index (0–7) */
    meas_type_t type; /**< Measurement type selector */
} meas_context_t;

/**
 * @brief Generic SNMP callback for HLW8032 power measurements.
 *
 * This function supports all measurement types (voltage, current, power, PF, kWh, uptime).
 * It uses a context structure passed from the SNMP OID table to determine channel and type.
 * For all except uptime, it forces a blocking HLW8032 read to ensure fresh values.
 *
 * @param buf Pointer to SNMP output buffer (OCTET STRING).
 * @param len Pointer to output length (bytes).
 * @param ctx Context pointer, must point to a ::meas_context_t struct.
 * @return None. On success buf contains ASCII-encoded value, otherwise "ERR".
 */
void get_power_measurement(void *buf, uint8_t *len, const meas_context_t *ctx);

/**
 * @brief Macro to define trampoline functions for each channel/metric.
 */
#define DECLARE_TRAMPOLINE_PROTO(CH, MTYPE) void get_power_##CH##_##MTYPE(void *buf, uint8_t *len)

/* Channel 0 */
DECLARE_TRAMPOLINE_PROTO(0, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(0, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(0, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(0, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(0, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(0, MEAS_UPTIME);

/* Channel 1 */
DECLARE_TRAMPOLINE_PROTO(1, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(1, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(1, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(1, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(1, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(1, MEAS_UPTIME);

/* Channel 2 */
DECLARE_TRAMPOLINE_PROTO(2, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(2, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(2, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(2, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(2, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(2, MEAS_UPTIME);

/* Channel 3 */
DECLARE_TRAMPOLINE_PROTO(3, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(3, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(3, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(3, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(3, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(3, MEAS_UPTIME);

/* Channel 4 */
DECLARE_TRAMPOLINE_PROTO(4, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(4, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(4, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(4, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(4, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(4, MEAS_UPTIME);

/* Channel 5 */
DECLARE_TRAMPOLINE_PROTO(5, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(5, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(5, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(5, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(5, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(5, MEAS_UPTIME);

/* Channel 6 */
DECLARE_TRAMPOLINE_PROTO(6, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(6, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(6, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(6, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(6, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(6, MEAS_UPTIME);

/* Channel 7 */
DECLARE_TRAMPOLINE_PROTO(7, MEAS_VOLTAGE);
DECLARE_TRAMPOLINE_PROTO(7, MEAS_CURRENT);
DECLARE_TRAMPOLINE_PROTO(7, MEAS_WATT);
DECLARE_TRAMPOLINE_PROTO(7, MEAS_PF);
DECLARE_TRAMPOLINE_PROTO(7, MEAS_KWH);
DECLARE_TRAMPOLINE_PROTO(7, MEAS_UPTIME);

#endif