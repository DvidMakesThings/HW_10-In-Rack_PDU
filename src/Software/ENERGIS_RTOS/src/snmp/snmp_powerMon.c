/**
 * @file snmp_powerMon.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp04 4. SNMP Agent - Power monitoring (RTOS)
 * @ingroup snmp
 * @brief Per-channel power telemetry GET callbacks using MeterTask API.
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-08
 * @details Queries the canonical telemetry cache owned by MeterTask.
 *          Uses MeterTask_GetTelemetry() for non-blocking cached reads.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_powerMon.h"
#include "../CONFIG.h"

/**
 * @brief Format float to string buffer with specified format
 * @param buf Output buffer (min 16 bytes)
 * @param len Pointer to receive string length
 * @param x Float value to format
 * @param fmt Printf format string (e.g., "%.2f")
 */
static inline void ftoa(void *buf, uint8_t *len, float x, const char *fmt) {
    *len = (uint8_t)snprintf((char *)buf, 16, fmt, x);
}

/**
 * @brief Format uint32 to string buffer
 * @param buf Output buffer (min 16 bytes)
 * @param len Pointer to receive string length
 * @param v Unsigned 32-bit value to format
 */
static inline void u32toa(void *buf, uint8_t *len, uint32_t v) {
    *len = (uint8_t)snprintf((char *)buf, 16, "%lu", (unsigned long)v);
}

/**
 * @brief Read telemetry data for a channel from MeterTask cache
 * @param ch Channel index 0..7
 * @param v Pointer to receive voltage [V]
 * @param a Pointer to receive current [A]
 * @param w Pointer to receive power [W]
 * @param pf Pointer to receive power factor [0..1]
 * @param kwh Pointer to receive energy [kWh]
 * @param up Pointer to receive uptime [s]
 * @note Returns zeros if no valid telemetry available
 */
static inline void readN(uint8_t ch, float *v, float *a, float *w, float *pf, float *kwh,
                         uint32_t *up) {
    meter_telemetry_t telem;

    /* Get cached telemetry from MeterTask (non-blocking) */
    if (MeterTask_GetTelemetry(ch, &telem) && telem.valid) {
        *v = telem.voltage;
        *a = telem.current;
        *w = telem.power;
        *pf = telem.power_factor;
        *kwh = telem.energy_kwh;
        *up = telem.uptime;
    } else {
        /* No valid data - return zeros */
        *v = 0.0f;
        *a = 0.0f;
        *w = 0.0f;
        *pf = 0.0f;
        *kwh = 0.0f;
        *up = 0;
    }
}

/**
 * @brief Generate SNMP getter functions for a channel
 * @param idx Channel index 0..7
 *
 * Generates 6 functions per channel:
 * - get_power_N_MEAS_VOLTAGE(buf, len) -> "%.2f"V
 * - get_power_N_MEAS_CURRENT(buf, len) -> "%.3f"A
 * - get_power_N_MEAS_WATT(buf, len)    -> "%.1f"W
 * - get_power_N_MEAS_PF(buf, len)      -> "%.3f" (power factor)
 * - get_power_N_MEAS_KWH(buf, len)     -> "%.3f"kWh
 * - get_power_N_MEAS_UPTIME(buf, len)  -> "%lu"s
 */
#define GEN_CH(idx)                                                                                \
    void get_power_##idx##_MEAS_VOLTAGE(void *b, uint8_t *l) {                                     \
        float V = 0, I = 0, W = 0, PF = 0, K = 0;                                                  \
        uint32_t U = 0;                                                                            \
        readN(idx, &V, &I, &W, &PF, &K, &U);                                                       \
        ftoa(b, l, V, "%.2f");                                                                     \
    }                                                                                              \
    void get_power_##idx##_MEAS_CURRENT(void *b, uint8_t *l) {                                     \
        float V = 0, I = 0, W = 0, PF = 0, K = 0;                                                  \
        uint32_t U = 0;                                                                            \
        readN(idx, &V, &I, &W, &PF, &K, &U);                                                       \
        ftoa(b, l, I, "%.3f");                                                                     \
    }                                                                                              \
    void get_power_##idx##_MEAS_WATT(void *b, uint8_t *l) {                                        \
        float V = 0, I = 0, W = 0, PF = 0, K = 0;                                                  \
        uint32_t U = 0;                                                                            \
        readN(idx, &V, &I, &W, &PF, &K, &U);                                                       \
        ftoa(b, l, W, "%.1f");                                                                     \
    }                                                                                              \
    void get_power_##idx##_MEAS_PF(void *b, uint8_t *l) {                                          \
        float V = 0, I = 0, W = 0, PF = 0, K = 0;                                                  \
        uint32_t U = 0;                                                                            \
        readN(idx, &V, &I, &W, &PF, &K, &U);                                                       \
        ftoa(b, l, PF, "%.3f");                                                                    \
    }                                                                                              \
    void get_power_##idx##_MEAS_KWH(void *b, uint8_t *l) {                                         \
        float V = 0, I = 0, W = 0, PF = 0, K = 0;                                                  \
        uint32_t U = 0;                                                                            \
        readN(idx, &V, &I, &W, &PF, &K, &U);                                                       \
        ftoa(b, l, K, "%.3f");                                                                     \
    }                                                                                              \
    void get_power_##idx##_MEAS_UPTIME(void *b, uint8_t *l) {                                      \
        float V = 0, I = 0, W = 0, PF = 0, K = 0;                                                  \
        uint32_t U = 0;                                                                            \
        readN(idx, &V, &I, &W, &PF, &K, &U);                                                       \
        u32toa(b, l, U);                                                                           \
    }

/* Generate SNMP getter functions for all 8 channels */
GEN_CH(0)
GEN_CH(1)
GEN_CH(2)
GEN_CH(3)
GEN_CH(4)
GEN_CH(5)
GEN_CH(6)
GEN_CH(7)

/** @} */