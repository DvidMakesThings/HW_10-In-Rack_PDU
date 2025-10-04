/**
 * @file snmp_powerMon.c
 * @author DvidMakesThings - David Sipos
 * @defgroup snmp4 4. SNMP Agent - Power Monitoring
 * @ingroup snmp
 * @brief Custom SNMP agent implementation for ENERGIS PDU power monitoring.
 * @{
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_powerMon.h"
#include "CONFIG.h"

// ####################################################################################
//  Power monitor callbacks
// ####################################################################################

/* --------------------------------------------------------------------------
 * Context instances for all 8 channels × 6 types
 * -------------------------------------------------------------------------- */

#define CTX(CH, M) {(uint8_t)(CH), (uint8_t)(M)}

// clang-format off
meas_context_t g_power_ctxs[8][6] = {
    { {0, MEAS_VOLTAGE}, {0, MEAS_CURRENT}, {0, MEAS_WATT}, {0, MEAS_PF}, {0, MEAS_KWH}, {0, MEAS_UPTIME} },
    { {1, MEAS_VOLTAGE}, {1, MEAS_CURRENT}, {1, MEAS_WATT}, {1, MEAS_PF}, {1, MEAS_KWH}, {1, MEAS_UPTIME} },
    { {2, MEAS_VOLTAGE}, {2, MEAS_CURRENT}, {2, MEAS_WATT}, {2, MEAS_PF}, {2, MEAS_KWH}, {2, MEAS_UPTIME} },
    { {3, MEAS_VOLTAGE}, {3, MEAS_CURRENT}, {3, MEAS_WATT}, {3, MEAS_PF}, {3, MEAS_KWH}, {3, MEAS_UPTIME} },
    { {4, MEAS_VOLTAGE}, {4, MEAS_CURRENT}, {4, MEAS_WATT}, {4, MEAS_PF}, {4, MEAS_KWH}, {4, MEAS_UPTIME} },
    { {5, MEAS_VOLTAGE}, {5, MEAS_CURRENT}, {5, MEAS_WATT}, {5, MEAS_PF}, {5, MEAS_KWH}, {5, MEAS_UPTIME} },
    { {6, MEAS_VOLTAGE}, {6, MEAS_CURRENT}, {6, MEAS_WATT}, {6, MEAS_PF}, {6, MEAS_KWH}, {6, MEAS_UPTIME} },
    { {7, MEAS_VOLTAGE}, {7, MEAS_CURRENT}, {7, MEAS_WATT}, {7, MEAS_PF}, {7, MEAS_KWH}, {7, MEAS_UPTIME} }
};


/**
 * @brief Generic SNMP callback for HLW8032 power measurements (cached mode).
 *
 * This function now uses cached values updated by ::hlw8032_poll_once or
 * ::hlw8032_refresh_all, instead of forcing a fresh HLW8032 read every call.
 *
 * @param buf Pointer to SNMP output buffer (OCTET STRING).
 * @param len Pointer to output length (bytes).
 * @param ctx Context pointer, must point to a ::meas_context_t struct.
 */
void get_power_measurement(void *buf, uint8_t *len, const meas_context_t *ctx) {
    char *dst = (char *)buf;

    switch (ctx->type) {
    case MEAS_VOLTAGE:
        *len = (uint8_t)snprintf(dst, 16, "%.2f", hlw8032_cached_voltage(ctx->channel));
        break;
    case MEAS_CURRENT:
        *len = (uint8_t)snprintf(dst, 16, "%.3f", hlw8032_cached_current(ctx->channel));
        break;
    case MEAS_WATT:
        *len = (uint8_t)snprintf(dst, 16, "%.1f", hlw8032_cached_power(ctx->channel));
        break;
    case MEAS_PF:
        *len = (uint8_t)snprintf(dst, 16, "%.3f", hlw8032_get_power_factor());
        break;
    case MEAS_KWH:
        *len = (uint8_t)snprintf(dst, 16, "%.5f", hlw8032_get_kwh());
        break;
    case MEAS_UPTIME:
        *len = (uint8_t)snprintf(dst, 16, "%u", hlw8032_cached_uptime(ctx->channel));
        break;
    }
}

// clang-format on

/**
 * @brief Macro to define trampoline functions for each channel/metric.
 * @param CH Channel number (0–7)
 * @param MTYPE Measurement type (meas_type_t)
 * @returns None. Defines a function get_power_CH_MTYPE()
 */
#define DECLARE_TRAMPOLINE(CH, MTYPE)                                                              \
    void get_power_##CH##_##MTYPE(void *buf, uint8_t *len) {                                       \
        const meas_context_t ctx = {(uint8_t)(CH), (meas_type_t)(MTYPE)};                          \
        get_power_measurement(buf, len, &ctx);                                                     \
    }

/* Channel 0 */
DECLARE_TRAMPOLINE(0, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(0, MEAS_CURRENT)
DECLARE_TRAMPOLINE(0, MEAS_WATT)
DECLARE_TRAMPOLINE(0, MEAS_PF)
DECLARE_TRAMPOLINE(0, MEAS_KWH)
DECLARE_TRAMPOLINE(0, MEAS_UPTIME)

/* Channel 1 */
DECLARE_TRAMPOLINE(1, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(1, MEAS_CURRENT)
DECLARE_TRAMPOLINE(1, MEAS_WATT)
DECLARE_TRAMPOLINE(1, MEAS_PF)
DECLARE_TRAMPOLINE(1, MEAS_KWH)
DECLARE_TRAMPOLINE(1, MEAS_UPTIME)

/* Channel 2 */
DECLARE_TRAMPOLINE(2, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(2, MEAS_CURRENT)
DECLARE_TRAMPOLINE(2, MEAS_WATT)
DECLARE_TRAMPOLINE(2, MEAS_PF)
DECLARE_TRAMPOLINE(2, MEAS_KWH)
DECLARE_TRAMPOLINE(2, MEAS_UPTIME)

/* Channel 3 */
DECLARE_TRAMPOLINE(3, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(3, MEAS_CURRENT)
DECLARE_TRAMPOLINE(3, MEAS_WATT)
DECLARE_TRAMPOLINE(3, MEAS_PF)
DECLARE_TRAMPOLINE(3, MEAS_KWH)
DECLARE_TRAMPOLINE(3, MEAS_UPTIME)

/* Channel 4 */
DECLARE_TRAMPOLINE(4, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(4, MEAS_CURRENT)
DECLARE_TRAMPOLINE(4, MEAS_WATT)
DECLARE_TRAMPOLINE(4, MEAS_PF)
DECLARE_TRAMPOLINE(4, MEAS_KWH)
DECLARE_TRAMPOLINE(4, MEAS_UPTIME)

/* Channel 5 */
DECLARE_TRAMPOLINE(5, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(5, MEAS_CURRENT)
DECLARE_TRAMPOLINE(5, MEAS_WATT)
DECLARE_TRAMPOLINE(5, MEAS_PF)
DECLARE_TRAMPOLINE(5, MEAS_KWH)
DECLARE_TRAMPOLINE(5, MEAS_UPTIME)

/* Channel 6 */
DECLARE_TRAMPOLINE(6, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(6, MEAS_CURRENT)
DECLARE_TRAMPOLINE(6, MEAS_WATT)
DECLARE_TRAMPOLINE(6, MEAS_PF)
DECLARE_TRAMPOLINE(6, MEAS_KWH)
DECLARE_TRAMPOLINE(6, MEAS_UPTIME)

/* Channel 7 */
DECLARE_TRAMPOLINE(7, MEAS_VOLTAGE)
DECLARE_TRAMPOLINE(7, MEAS_CURRENT)
DECLARE_TRAMPOLINE(7, MEAS_WATT)
DECLARE_TRAMPOLINE(7, MEAS_PF)
DECLARE_TRAMPOLINE(7, MEAS_KWH)
DECLARE_TRAMPOLINE(7, MEAS_UPTIME)

/** @} */ // End of snmp4