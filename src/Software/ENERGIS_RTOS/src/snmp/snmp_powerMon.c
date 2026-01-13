/**
 * @file src/snmp/snmp_powerMon.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-11-08
 *
 * @details
 * Implementation of SNMP power monitoring callbacks. All telemetry data is read
 * from MeterTask's canonical cache using non-blocking operations. If telemetry
 * is invalid or unavailable, functions return zero values to ensure SNMP queries
 * always receive valid responses.
 *
 * The module uses macro-generated functions (GEN_CH) to create 6 telemetry
 * getters per channel (voltage, current, power, power factor, energy, uptime)
 * for all 8 outlets. OCP status functions access the overcurrent protection
 * module directly via Overcurrent_GetStatus().
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_powerMon.h"
#include "../CONFIG.h"
#include "../tasks/OCP.h"

/**
 * @brief Format float to ASCII string buffer.
 *
 * Converts a floating-point value to ASCII representation using the specified
 * printf format string. Used for OCTET_STRING SNMP responses.
 *
 * @param buf Output buffer (minimum 16 bytes required)
 * @param len Pointer to receive string length (excluding null terminator)
 * @param x Float value to format
 * @param fmt Printf format string (e.g., "%.2f" for 2 decimals)
 *
 * @return None
 */
static inline void ftoa(void *buf, uint8_t *len, float x, const char *fmt) {
    *len = (uint8_t)snprintf((char *)buf, 16, fmt, x);
}

/**
 * @brief Format uint32 to ASCII string buffer.
 *
 * Converts an unsigned 32-bit integer to ASCII decimal representation.
 * Used for OCTET_STRING SNMP responses.
 *
 * @param buf Output buffer (minimum 16 bytes required)
 * @param len Pointer to receive string length (excluding null terminator)
 * @param v Unsigned 32-bit value to format
 *
 * @return None
 */
static inline void u32toa(void *buf, uint8_t *len, uint32_t v) {
    *len = (uint8_t)snprintf((char *)buf, 16, "%lu", (unsigned long)v);
}

/**
 * @brief Read telemetry snapshot for a channel from MeterTask cache.
 *
 * Retrieves all telemetry parameters for a specified channel from the MeterTask
 * cache. If telemetry is unavailable or invalid, all output parameters are set
 * to zero to ensure SNMP responses are always well-formed.
 *
 * @param ch Channel index (0..7)
 * @param v Pointer to receive voltage [V]
 * @param a Pointer to receive current [A]
 * @param w Pointer to receive power [W]
 * @param pf Pointer to receive power factor (0..1)
 * @param kwh Pointer to receive accumulated energy [kWh]
 * @param up Pointer to receive uptime [s]
 *
 * @return None
 *
 * @note Non-blocking operation; returns immediately with cached data.
 * @note Returns zeros if channel out of range or telemetry invalid.
 */
static inline void readN(uint8_t ch, float *v, float *a, float *w, float *pf, float *kwh,
                         uint32_t *up) {
    meter_telemetry_t telem;

    /* Attempt to read cached telemetry (non-blocking) */
    if (MeterTask_GetTelemetry(ch, &telem) && telem.valid) {
        *v = telem.voltage;
        *a = telem.current;
        *w = telem.power;
        *pf = telem.power_factor;
        *kwh = telem.energy_kwh;
        *up = telem.uptime;
    } else {
        /* Return zeros if telemetry unavailable or invalid */
        *v = 0.0f;
        *a = 0.0f;
        *w = 0.0f;
        *pf = 0.0f;
        *kwh = 0.0f;
        *up = 0;
    }
}

/**
 * @brief Macro to generate six SNMP telemetry getters for a channel.
 *
 * Generates the following functions for channel N (idx):
 * - get_power_N_MEAS_VOLTAGE: Voltage [V] as ASCII float (2 decimals)
 * - get_power_N_MEAS_CURRENT: Current [A] as ASCII float (3 decimals)
 * - get_power_N_MEAS_WATT: Power [W] as ASCII float (1 decimal)
 * - get_power_N_MEAS_PF: Power factor as ASCII float (3 decimals)
 * - get_power_N_MEAS_KWH: Energy [kWh] as ASCII float (3 decimals)
 * - get_power_N_MEAS_UPTIME: Uptime [s] as ASCII unsigned integer
 *
 * Each generated function reads telemetry from MeterTask cache via readN() and
 * formats the appropriate field using ftoa() or u32toa().
 *
 * @param idx Channel index (0..7)
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

/* Generate SNMP telemetry getter functions for all 8 channels (0..7) */
GEN_CH(0)
GEN_CH(1)
GEN_CH(2)
GEN_CH(3)
GEN_CH(4)
GEN_CH(5)
GEN_CH(6)
GEN_CH(7)

/* ===================================================================== */
/*                    Overcurrent Protection (OCP) Status                 */
/* ===================================================================== */

/**
 * @brief Write a signed 32-bit integer into SNMP response buffer.
 *
 * @details
 * The SNMP agent uses fixed-length 4-byte INTEGER storage for SNMPDTYPE_INTEGER
 * entries. This helper writes the value as a 32-bit little-endian integer and
 * reports a fixed length of 4 bytes.
 *
 * @param buf  Destination buffer (must be at least 4 bytes).
 * @param len  Output length; always set to 4.
 * @param v    Signed 32-bit value to encode.
 */
static inline void i32le(void *buf, uint8_t *len, int32_t v) {
    memcpy(buf, &v, 4);
    *len = 4;
}

/**
 * @brief Write an unsigned 32-bit integer into SNMP response buffer.
 *
 * @details
 * The SNMP agent uses fixed-length 4-byte INTEGER storage for SNMPDTYPE_INTEGER
 * entries. This helper writes the value as a 32-bit little-endian integer and
 * reports a fixed length of 4 bytes.
 *
 * @param buf  Destination buffer (must be at least 4 bytes).
 * @param len  Output length; always set to 4.
 * @param v    Unsigned 32-bit value to encode.
 */
static inline void u32le(void *buf, uint8_t *len, uint32_t v) {
    memcpy(buf, &v, 4);
    *len = 4;
}

/**
 * @brief Read overcurrent protection status snapshot.
 *
 * Attempts to obtain an atomic snapshot of the overcurrent protection state
 * from the OCP module via Overcurrent_GetStatus(). If the module is not
 * initialized or the snapshot cannot be obtained, returns false.
 *
 * @param st Pointer to destination structure for status snapshot
 *
 * @return true if valid snapshot obtained, false otherwise
 *
 * @note Returns false if st is NULL or OCP module not initialized.
 */
static inline bool ocp_read_status(overcurrent_status_t *st) {
    /* Validate destination pointer */
    if (st == NULL) {
        return false;
    }

    /* Attempt to read OCP status snapshot */
    if (!Overcurrent_GetStatus(st)) {
        return false;
    }

    return true;
}

void get_ocp_STATE(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    /* Return NORMAL if status read fails */
    if (!ocp_read_status(&st)) {
        i32le(buf, len, (int32_t)OC_STATE_NORMAL);
        return;
    }

    i32le(buf, len, (int32_t)st.state);
}

void get_ocp_TOTAL_CURRENT_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    /* Return zero if status read fails */
    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.3f");
        return;
    }

    ftoa(buf, len, st.total_current_a, "%.3f");
}

void get_ocp_LIMIT_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.limit_a, "%.2f");
}

void get_ocp_WARNING_THRESHOLD_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.warning_threshold_a, "%.2f");
}

void get_ocp_CRITICAL_THRESHOLD_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.critical_threshold_a, "%.2f");
}

void get_ocp_RECOVERY_THRESHOLD_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.recovery_threshold_a, "%.2f");
}

void get_ocp_LAST_TRIPPED_CH(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    int32_t out = 0;

    /* Convert 0-based channel index to 1-based for SNMP */
    if (ocp_read_status(&st)) {
        if (st.last_tripped_channel < 8u) {
            out = (int32_t)st.last_tripped_channel + 1;
        } else {
            out = 0;
        }
    }

    i32le(buf, len, out);
}

void get_ocp_TRIP_COUNT(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    uint32_t out = 0;

    if (ocp_read_status(&st)) {
        out = st.trip_count;
    }

    u32le(buf, len, out);
}

void get_ocp_LAST_TRIP_MS(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    uint32_t out = 0;

    if (ocp_read_status(&st)) {
        out = st.last_trip_timestamp_ms;
    }

    u32le(buf, len, out);
}

void get_ocp_SWITCHING_ALLOWED(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    int32_t out = 1;

    if (ocp_read_status(&st)) {
        out = st.switching_allowed ? 1 : 0;
    }

    i32le(buf, len, out);
}

void get_ocp_RESET(void *buf, uint8_t *len) { i32le(buf, len, 0); }

void set_ocp_RESET(int32_t v) {
    /* Trigger OCP lockout clear if non-zero */
    if (v != 0) {
        (void)Overcurrent_ClearLockout();
    }
}
