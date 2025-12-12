/**
 * @file src/snmp/snmp_powerMon.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-11-08
 *
 * @details Queries the canonical telemetry cache owned by MeterTask.
 *          Uses MeterTask_GetTelemetry() for non-blocking cached reads.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_powerMon.h"
#include "../CONFIG.h"
#include "../tasks/OCP.h"

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
 * @brief Read a full overcurrent status snapshot.
 *
 * @details
 * Provides a best-effort atomic snapshot using Overcurrent_GetStatus(). If the
 * module is not initialized or the snapshot cannot be obtained, this function
 * returns false.
 *
 * @param st Destination for the status snapshot.
 * @return true if a valid snapshot was written, false otherwise.
 */
static inline bool ocp_read_status(overcurrent_status_t *st) {
    if (st == NULL) {
        return false;
    }

    if (!Overcurrent_GetStatus(st)) {
        return false;
    }

    return true;
}

/**
 * @brief SNMP getter: OCP state (NORMAL/WARNING/CRITICAL/LOCKOUT).
 *
 * @param buf Output buffer; writes a 32-bit little-endian INTEGER.
 * @param len Output length; always set to 4.
 */
void get_ocp_STATE(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        i32le(buf, len, (int32_t)OC_STATE_NORMAL);
        return;
    }

    i32le(buf, len, (int32_t)st.state);
}

/**
 * @brief SNMP getter: OCP total current (A).
 *
 * @details Encoded as an ASCII string with 3 decimals to match other telemetry.
 *
 * @param buf Output buffer (min 16 bytes recommended by table entries).
 * @param len Output length in bytes.
 */
void get_ocp_TOTAL_CURRENT_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.3f");
        return;
    }

    ftoa(buf, len, st.total_current_a, "%.3f");
}

/**
 * @brief SNMP getter: OCP configured current limit (A).
 *
 * @param buf Output buffer (min 16 bytes recommended by table entries).
 * @param len Output length in bytes.
 */
void get_ocp_LIMIT_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.limit_a, "%.2f");
}

/**
 * @brief SNMP getter: OCP warning threshold (A).
 *
 * @param buf Output buffer (min 16 bytes recommended by table entries).
 * @param len Output length in bytes.
 */
void get_ocp_WARNING_THRESHOLD_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.warning_threshold_a, "%.2f");
}

/**
 * @brief SNMP getter: OCP critical threshold (A).
 *
 * @param buf Output buffer (min 16 bytes recommended by table entries).
 * @param len Output length in bytes.
 */
void get_ocp_CRITICAL_THRESHOLD_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.critical_threshold_a, "%.2f");
}

/**
 * @brief SNMP getter: OCP recovery threshold (A).
 *
 * @param buf Output buffer (min 16 bytes recommended by table entries).
 * @param len Output length in bytes.
 */
void get_ocp_RECOVERY_THRESHOLD_A(void *buf, uint8_t *len) {
    overcurrent_status_t st;

    if (!ocp_read_status(&st)) {
        ftoa(buf, len, 0.0f, "%.2f");
        return;
    }

    ftoa(buf, len, st.recovery_threshold_a, "%.2f");
}

/**
 * @brief SNMP getter: OCP last tripped channel (1..8), or 0 if none.
 *
 * @details
 * The core module stores the channel as 0-based (0..7) or 0xFF when not known.
 * This getter converts the output to a user-facing 1-based value (1..8), or 0
 * when no channel is available.
 *
 * @param buf Output buffer; writes a 32-bit little-endian INTEGER.
 * @param len Output length; always set to 4.
 */
void get_ocp_LAST_TRIPPED_CH(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    int32_t out = 0;

    if (ocp_read_status(&st)) {
        if (st.last_tripped_channel < 8u) {
            out = (int32_t)st.last_tripped_channel + 1;
        } else {
            out = 0;
        }
    }

    i32le(buf, len, out);
}

/**
 * @brief SNMP getter: OCP trip counter since boot.
 *
 * @param buf Output buffer; writes a 32-bit little-endian INTEGER.
 * @param len Output length; always set to 4.
 */
void get_ocp_TRIP_COUNT(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    uint32_t out = 0;

    if (ocp_read_status(&st)) {
        out = st.trip_count;
    }

    u32le(buf, len, out);
}

/**
 * @brief SNMP getter: Timestamp of last trip (ms since boot).
 *
 * @param buf Output buffer; writes a 32-bit little-endian INTEGER.
 * @param len Output length; always set to 4.
 */
void get_ocp_LAST_TRIP_MS(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    uint32_t out = 0;

    if (ocp_read_status(&st)) {
        out = st.last_trip_timestamp_ms;
    }

    u32le(buf, len, out);
}

/**
 * @brief SNMP getter: Switching allowed flag.
 *
 * @details Encoded as INTEGER (0 = not allowed, 1 = allowed).
 *
 * @param buf Output buffer; writes a 32-bit little-endian INTEGER.
 * @param len Output length; always set to 4.
 */
void get_ocp_SWITCHING_ALLOWED(void *buf, uint8_t *len) {
    overcurrent_status_t st;
    int32_t out = 1;

    if (ocp_read_status(&st)) {
        out = st.switching_allowed ? 1 : 0;
    }

    i32le(buf, len, out);
}

/**
 * @brief SNMP getter: OCP reset control value.
 *
 * @details
 * This is a write-oriented control OID. The getter returns 0.
 *
 * @param buf Output buffer; writes a 32-bit little-endian INTEGER.
 * @param len Output length; always set to 4.
 */
void get_ocp_RESET(void *buf, uint8_t *len) { i32le(buf, len, 0); }

/**
 * @brief SNMP setter: Clear OCP lockout state.
 *
 * @details
 * A non-zero value triggers Overcurrent_ClearLockout(). This operation does not
 * validate that the overload condition is resolved and should only be used
 * after reducing load.
 *
 * @param v SNMP INTEGER value written by the client.
 */
void set_ocp_RESET(int32_t v) {
    if (v != 0) {
        (void)Overcurrent_ClearLockout();
    }
}
