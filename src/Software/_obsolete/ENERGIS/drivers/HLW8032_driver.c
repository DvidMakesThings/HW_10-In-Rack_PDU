/**
 * @file HLW8032_driver.c
 * @author DvidMakesThings - David Sipos
 * @defgroup driver3 3. HLW8032 Energy Measurement Driver
 * @ingroup drivers
 * @brief HLW8032 Power Measurement Driver Source
 * @{
 * @date 2025-08-21
 *
 * Provides API for interfacing with the HLW8032 power measurement chip.
 * Includes functions for initialization, reading measurements, uptime tracking,
 * cached access for multi-core systems, and EEPROM-backed calibration.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "HLW8032_driver.h"
#include "../CONFIG.h" /* HLW8032_UART_ID, HLW8032_BAUDRATE, mcp_relay_write_pin */
#include "pico/stdlib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define HLW_OFFSET 0.0f

/* =====================  Nominal component values  ===================== */
#define NOMINAL_R1 1880000.0f /* 1880 kOhm high side divider */
#define NOMINAL_R2 1000.0f    /* 1 kOhm low side divider */
#define NOMINAL_SHUNT 0.001f  /* 1 mOhm shunt resistor */

/* =====================  Frame & book-keeping  ========================= */
static uint8_t frame[HLW8032_FRAME_LENGTH];
static uint8_t rx_buf[MAX_RX_BYTES];
static uint32_t channel_uptime[8] = {0};
static absolute_time_t channel_last_on[8];

/* =====================  Per-channel calibration  ====================== */
static hlw_calib_t channel_calib[8];

/* =====================  Last raw registers  =========================== */
static uint32_t VolPar, VolData;
static uint32_t CurPar, CurData;
static uint32_t PowPar, PowData;
static uint16_t PF;
static uint32_t PF_Count = 1;
static uint8_t last_channel_read = 0xFF;

/* =====================  Last scaled readings  ========================= */
static float last_voltage = 0.0f;
static float last_current = 0.0f;
static float last_power = 0.0f;

/* =====================  Cached measurements  ========================== */
static float cached_voltage[8];
static float cached_current[8];
static float cached_power[8];
static uint32_t cached_uptime[8];
static bool cached_state[8];

/* =====================  Polling helper  =============================== */
static uint8_t poll_channel = 0;

/* ===================================================================== */
/*                        Internal helpers (static)                       */
/* ===================================================================== */

static inline uint8_t _port_from_pin(uint8_t pin) { return (pin < 8) ? 0u : 1u; }
static inline uint8_t _bit_from_pin(uint8_t pin) { return (uint8_t)(pin & 7u); }

/* Build a mask+value for a given pin/value onto a specific port bucket */
static inline void _accum_mask_for_pin(uint8_t pin, uint8_t val, uint8_t *maskA, uint8_t *valA,
                                       uint8_t *maskB, uint8_t *valB) {
    const uint8_t port = _port_from_pin(pin);
    const uint8_t bit = _bit_from_pin(pin);
    if (port == 0) {
        *maskA |= (uint8_t)(1u << bit);
        if (val)
            *valA |= (uint8_t)(1u << bit);
        else
            *valA &= (uint8_t)~(1u << bit);
    } else {
        *maskB |= (uint8_t)(1u << bit);
        if (val)
            *valB |= (uint8_t)(1u << bit);
        else
            *valB &= (uint8_t)~(1u << bit);
    }
}

static inline float _finite_or(float v, float fb) { return isfinite(v) ? v : fb; }

/**
 * @brief Determine if MUX A/B lines need inversion for upper channels.
 */
static inline bool _mux_invert_needed(void) {
#if defined(SW_REV) && (SW_REV == 100)
    return true;
#else
    return false;
#endif
}

/**
 * @brief Atomically set MUX select lines with MUX_ENABLE held low.
 *
 * This avoids intermediate select codes showing up on the shared MCP port,
 * which could otherwise glitch other outputs on the same port if any writer
 * interleaves a read-modify-write. We only use masked whole-port writes.
 *
 * Pins may be on mixed ports; we build masks for A and B separately.
 */
static void mux_select(uint8_t ch) {
    ch &= 0x07;

    uint8_t a = (ch >> 0) & 1;
    uint8_t b = (ch >> 1) & 1;
    uint8_t c = (ch >> 2) & 1;

    /* If your board uses inverted A/B for upper bank, keep this quirk */
#if defined(SW_REV) && (SW_REV == 100)
    if (c) {
        a ^= 1;
        b ^= 1;
    }
#endif

    /* Build one write per MCP port to avoid interleaved RMW */
    uint8_t maskA = 0, valA = 0;
    uint8_t maskB = 0, valB = 0;

/* helper lambdas */
#define ACCUM(_pin_, _lvl_)                                                                        \
    do {                                                                                           \
        uint8_t _p = ((_pin_) < 8) ? 0 : 1;                                                        \
        uint8_t _b = (uint8_t)((_pin_) & 7);                                                       \
        if (_p == 0) {                                                                             \
            maskA |= (uint8_t)(1u << _b);                                                          \
            if (_lvl_)                                                                             \
                valA |= (uint8_t)(1u << _b);                                                       \
            else                                                                                   \
                valA &= (uint8_t)~(1u << _b);                                                      \
        } else {                                                                                   \
            maskB |= (uint8_t)(1u << _b);                                                          \
            if (_lvl_)                                                                             \
                valB |= (uint8_t)(1u << _b);                                                       \
            else                                                                                   \
                valB &= (uint8_t)~(1u << _b);                                                      \
        }                                                                                          \
    } while (0)

    /* target select lines */
    ACCUM(MUX_SELECT_A, a);
    ACCUM(MUX_SELECT_B, b);
    ACCUM(MUX_SELECT_C, c);

    /* Active-low enable: disable → set A/B/C → enable */
    mcp_relay_write_pin(MUX_ENABLE, 1);
    busy_wait_us(200);

    if (maskA)
        mcp_relay_write_mask(0, maskA, valA);
    if (maskB)
        mcp_relay_write_mask(1, maskB, valB);
    busy_wait_us(500);

    mcp_relay_write_pin(MUX_ENABLE, 0);
    busy_wait_us(500);

#undef ACCUM
}

/**
 * @brief HLW8032 frame checksum validate.
 */
static bool checksum_ok(const uint8_t *f) {
    uint8_t sum = 0;
    for (int i = 2; i <= 22; i++)
        sum += f[i];
    return sum == f[23];
}

/**
 * @brief Read one valid 24-byte frame from UART.
 */
static bool read_frame(void) {
    uint64_t start = time_us_64();
    int cnt = 0;
    while (cnt < MAX_RX_BYTES && (time_us_64() - start) < UART_READ_TIMEOUT_US) {
        if (uart_is_readable(HLW8032_UART_ID)) {
            rx_buf[cnt++] = uart_getc(HLW8032_UART_ID);
        }
    }
    for (int i = 0; i + HLW8032_FRAME_LENGTH <= cnt; i++) {
        if (rx_buf[i + 1] != 0x5A)
            continue;
        memcpy(frame, rx_buf + i, HLW8032_FRAME_LENGTH);
        if (checksum_ok(frame))
            return true;
    }
    return false;
}

/* ===================================================================== */
/*                           Public API (driver)                          */
/* ===================================================================== */

/**
 * @brief Initialize HLW8032 and load EEPROM calibration.
 */
void hlw8032_init(void) {
    uart_init(HLW8032_UART_ID, HLW8032_BAUDRATE);
    uart_set_format(HLW8032_UART_ID, 8, 1, UART_PARITY_EVEN);

    /* Keep mux disabled at boot (active-low) and clear selects */
    mcp_relay_write_pin(MUX_ENABLE, 1);
    mcp_relay_write_pin(MUX_SELECT_A, 0);
    mcp_relay_write_pin(MUX_SELECT_B, 0);
    mcp_relay_write_pin(MUX_SELECT_C, 0);

    for (uint8_t i = 0; i < 8; i++) {
        channel_calib[i].voltage_factor = HLW8032_VF;
        channel_calib[i].current_factor = HLW8032_CF;
        channel_calib[i].voltage_offset = 0.0f;
        channel_calib[i].current_offset = 0.0f;
        channel_calib[i].r1_actual = NOMINAL_R1;
        channel_calib[i].r2_actual = NOMINAL_R2;
        channel_calib[i].shunt_actual = NOMINAL_SHUNT;
        channel_calib[i].calibrated = 0xFF;
        channel_calib[i].zero_calibrated = 0xFF;
    }

    hlw8032_load_calibration();
}


/**
 * @brief Read one measurement frame from the selected channel and apply calibration.
 *        NOTE: VF/CF are defined to already include the board’s divider/shunt scaling.
 *        Do NOT multiply by external R1/R2 or shunt again (that caused 1.5 kV / 1e8 A).
 * @param channel Channel index (0..7).
 * @return true if a valid, parsed frame is available; false otherwise.
 */
bool hlw8032_read(uint8_t channel) {
    mux_select(channel);
    sleep_ms(10);

    while (uart_is_readable(HLW8032_UART_ID))
        (void)uart_getc(HLW8032_UART_ID);
    (void)read_frame();
    if (!read_frame())
        return false;

    VolPar = ((uint32_t)frame[2] << 16) | (frame[3] << 8) | frame[4];
    VolData = (frame[20] & 0x40) ? (((uint32_t)frame[5] << 16) | (frame[6] << 8) | frame[7]) : 0;
    CurPar = ((uint32_t)frame[8] << 16) | (frame[9] << 8) | frame[10];
    CurData = (frame[20] & 0x20) ? (((uint32_t)frame[11] << 16) | (frame[12] << 8) | frame[13]) : 0;
    PowPar = ((uint32_t)frame[14] << 16) | (frame[15] << 8) | frame[16];
    PowData = (frame[20] & 0x10) ? (((uint32_t)frame[17] << 16) | (frame[18] << 8) | frame[19]) : 0;
    PF = ((uint16_t)frame[21] << 8) | frame[22];
    if (frame[20] & 0x80)
        PF_Count++;

    last_channel_read = channel;

    const float vf = (channel_calib[channel].voltage_factor > 0.0f)
                         ? channel_calib[channel].voltage_factor
                         : HLW8032_VF;
    const float cf = (channel_calib[channel].current_factor > 0.0f)
                         ? channel_calib[channel].current_factor
                         : HLW8032_CF;
    const float v_off = channel_calib[channel].voltage_offset;
    const float c_off = channel_calib[channel].current_offset;

    const float v_raw = (VolData > 0) ? ((float)VolPar / (float)VolData) * vf : 0.0f;
    const float i_raw = (CurData > 0) ? ((float)CurPar / (float)CurData) * cf : 0.0f;

    last_voltage = v_raw - v_off;
    last_current = i_raw - c_off;

    if (!(last_voltage >= 0.0f) || last_voltage > 400.0f)
        last_voltage = 0.0f;
    if (!(last_current >= 0.0f) || last_current > 100.0f)
        last_current = 0.0f;

    last_power = last_voltage * last_current;
    return true;
}

float hlw8032_get_voltage(void) { return last_voltage; }
float hlw8032_get_current(void) { return last_current; }
float hlw8032_get_power(void) { return last_power; }
float hlw8032_get_power_inspect(void) { return last_voltage * last_current; }

float hlw8032_get_power_factor(void) {
    const float app = last_voltage * last_current;
    return app > 0.0f ? last_power / app : 0.0f;
}

float hlw8032_get_kwh(void) {
    const float app = last_voltage * last_current;
    if (app <= 0.0f || PowPar == 0)
        return 0.0f;
    const float imp_per_wh = ((float)PowPar / app) / 1e9f;
    return ((float)PF * PF_Count) / (imp_per_wh * 3600.0f);
}

void hlw8032_update_uptime(uint8_t ch, bool state) {
    if (state) {
        if (is_nil_time(channel_last_on[ch])) {
            channel_last_on[ch] = get_absolute_time();
        } else {
            absolute_time_t now = get_absolute_time();
            int64_t diff_us = absolute_time_diff_us(channel_last_on[ch], now);
            channel_uptime[ch] += (uint32_t)(diff_us / 1000000);
            channel_last_on[ch] = now;
        }
    } else {
        channel_last_on[ch] = nil_time;
    }
}

uint32_t hlw8032_get_uptime(uint8_t ch) { return channel_uptime[ch]; }

/**
 * @brief Poll exactly one channel round-robin and update calibrated caches.
 *        Caches always store CALIBRATED values as produced by hlw8032_read().
 */
void hlw8032_poll_once(void) {
    const uint8_t ch = poll_channel;

    const bool state = mcp_relay_read_pin(ch);
    cached_state[ch] = state;
    hlw8032_update_uptime(ch, state);

    const bool ok = hlw8032_read(ch);
    cached_voltage[ch] = ok ? hlw8032_get_voltage() : 0.0f;
    cached_current[ch] = ok ? hlw8032_get_current() : 0.0f;
    cached_power[ch] = ok ? hlw8032_get_power() : 0.0f;
    cached_uptime[ch] = hlw8032_get_uptime(ch);

    poll_channel = (uint8_t)((ch + 1) % 8);
}

void hlw8032_refresh_all(void) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        bool state = mcp_relay_read_pin(ch);
        cached_state[ch] = state;
        hlw8032_update_uptime(ch, state);

        bool ok = hlw8032_read(ch);
        cached_voltage[ch] = ok ? hlw8032_get_voltage() : 0.0f;
        cached_current[ch] = ok ? hlw8032_get_current() : 0.0f;
        cached_power[ch] = ok ? hlw8032_get_power() : 0.0f;
        cached_uptime[ch] = hlw8032_get_uptime(ch);
    }
}

/**
 * @brief Return cached calibrated voltage. Guarantees sane, non-negative value.
 */
float hlw8032_cached_voltage(uint8_t ch) {
    float v = (ch < 8) ? cached_voltage[ch] : 0.0f;
    if (!(v >= 0.0f) || v > 400.0f)
        v = 0.0f;
    return v;
}

/**
 * @brief Return cached calibrated current. Guarantees sane, non-negative value.
 */
float hlw8032_cached_current(uint8_t ch) {
    float i = (ch < 8) ? cached_current[ch] : 0.0f;
    if (!(i >= 0.0f) || i > 100.0f)
        i = 0.0f;
    return i;
}

/**
 * @brief Return cached calibrated power computed at read time.
 *        If voltage/current were clamped later, recompute a safe product.
 */
float hlw8032_cached_power(uint8_t ch) {
    if (ch >= 8)
        return 0.0f;
    /* recompute to stay consistent with clamped getters */
    const float v = hlw8032_cached_voltage(ch);
    const float i = hlw8032_cached_current(ch);
    const float p = v * i;
    return (p >= 0.0f && p < 400.0f * 100.0f) ? p : 0.0f;
}

/**
 * @brief Return cached uptime seconds for a channel.
 */
uint32_t hlw8032_cached_uptime(uint8_t ch) { return (ch < 8) ? cached_uptime[ch] : 0u; }

/**
 * @brief Return cached switch state for a channel.
 */
bool hlw8032_cached_state(uint8_t ch) { return (ch < 8) ? cached_state[ch] : false; }

/* ===================================================================== */
/*                           Calibration section                          */
/* ===================================================================== */

/**
 * @brief Load calibration for all channels; keep EEPROM values and only sanitize invalid fields.
 */
void hlw8032_load_calibration(void) {
    for (uint8_t i = 0; i < 8; i++) {
        hlw_calib_t tmp;
        if (EEPROM_ReadSensorCalibrationForChannel(i, &tmp) == 0) {
            if (!(tmp.voltage_factor > 0.0f))
                tmp.voltage_factor = HLW8032_VF;
            if (!(tmp.current_factor > 0.0f))
                tmp.current_factor = HLW8032_CF;
            if (!(tmp.voltage_offset > -1000.0f && tmp.voltage_offset < 1000.0f))
                tmp.voltage_offset = 0.0f;
            if (!(tmp.current_offset > -1000.0f && tmp.current_offset < 1000.0f))
                tmp.current_offset = 0.0f;
            if (!(tmp.r1_actual > 0.0f))
                tmp.r1_actual = NOMINAL_R1;
            if (!(tmp.r2_actual > 0.0f))
                tmp.r2_actual = NOMINAL_R2;
            if (!(tmp.shunt_actual > 0.0f))
                tmp.shunt_actual = NOMINAL_SHUNT;

            channel_calib[i] = tmp;
        } else {
            channel_calib[i].voltage_factor = HLW8032_VF;
            channel_calib[i].current_factor = HLW8032_CF;
            channel_calib[i].voltage_offset = 0.0f;
            channel_calib[i].current_offset = 0.0f;
            channel_calib[i].r1_actual = NOMINAL_R1;
            channel_calib[i].r2_actual = NOMINAL_R2;
            channel_calib[i].shunt_actual = NOMINAL_SHUNT;
            channel_calib[i].calibrated = 0xFF;
            channel_calib[i].zero_calibrated = 0xFF;
        }
    }
}

/**
 * @brief Calibrate a single channel against known references (non-zero),
 *        recompute derived resistor/shunt values, write to EEPROM immediately.
 * @param channel 0..7
 * @param ref_voltage Reference voltage in volts (>0 to calibrate VF)
 * @param ref_current Reference current in amps  (>0 to calibrate CF)
 * @return true on success
 */
bool hlw8032_calibrate_channel(uint8_t channel, float ref_voltage, float ref_current) {
    (void)ref_current; /* current calibration not used */
    if (channel >= 8)
        return false;

    const int NUM_SAMPLES = 10;
    uint32_t vpar_sum = 0, vdat_sum = 0;
    int valid = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (hlw8032_read(channel)) {
            vpar_sum += VolPar;
            vdat_sum += VolData;
            valid++;
        }
        sleep_ms(100);
    }
    if (valid < (NUM_SAMPLES / 2))
        return false;

    float vf = (channel_calib[channel].voltage_factor > 0.f) ? channel_calib[channel].voltage_factor
                                                             : HLW8032_VF;
    float voff = channel_calib[channel].voltage_offset;

    const float avgvp = (float)vpar_sum / valid;
    const float avgvd = (float)vdat_sum / valid;

    bool did_zero = false, did_v = false;

    if (ref_voltage == 0.0f) {
        const float v_meas = (avgvd > 0.f) ? (avgvp / avgvd) * vf : 0.0f;
        voff = v_meas;
        did_zero = true;
    } else {
        if (ref_voltage > 0.0f && avgvp > 0.0f && avgvd > 0.0f) {
            vf = (ref_voltage + voff) * (avgvd / avgvp);
            did_v = true;
        } else {
            return false;
        }
    }

    /* Update only voltage-related fields */
    channel_calib[channel].voltage_factor = vf;
    channel_calib[channel].voltage_offset = voff;

    /* Keep current calibration unused/default in EEPROM */
    channel_calib[channel].current_factor = HLW8032_CF;
    channel_calib[channel].current_offset = 0.0f;

    /* Fixed shunt, project divider from VF for info */
    const float v_ratio = vf / HLW8032_VF;
    channel_calib[channel].r1_actual = NOMINAL_R1 * v_ratio;
    channel_calib[channel].r2_actual = NOMINAL_R2 * v_ratio;
    channel_calib[channel].shunt_actual = NOMINAL_SHUNT; /* 1 mOhm fixed */

    channel_calib[channel].calibrated =
        (did_v || did_zero) ? 0xCA : channel_calib[channel].calibrated;
    channel_calib[channel].zero_calibrated =
        did_zero ? 0xCA : channel_calib[channel].zero_calibrated;

    /* Persist to EEPROM using existing API */
    if (EEPROM_WriteSensorCalibrationForChannel(channel, &channel_calib[channel]) != 0)
        return false;

    ECHO("HLW8032 CH%u voltage-cal saved: VF=%.5f, Voff=%.5f\n", (unsigned)(channel + 1), vf, voff);
    return true;
}

/**
 * @brief Zero-calibrate all channels at 0V/0A using 25 frames, save to EEPROM, and use immediately.
 *        Sets voltage_offset/current_offset from measured zero, keeps vf/cf as-is.
 * @return true on success (all channels), false on first failure
 */
bool hlw8032_zero_calibrate_all(void) {
    uint8_t ok = 0;
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (hlw8032_calibrate_channel(ch, 0.0f, 0.0f))
            ok++;
    }
    return ok == 8;
}

/**
 * @brief Print calibration for a channel.
 *        Shows offsets when zero-calibrated; VF/CF are listed separately for reference.
 */
void hlw8032_print_calibration(uint8_t channel) {
    if (channel >= 8)
        return;
    const hlw_calib_t *c = &channel_calib[channel];

    ECHO("=== CH%u Calibration ===\n", (unsigned)(channel + 1));
    ECHO("Status: %s\n", (c->calibrated == 0xCA) ? "CALIBRATED" : "DEFAULTS");
    ECHO("VF=%.5f CF=%.5f\n", c->voltage_factor, c->current_factor);
    ECHO("Offsets: Voff=%.5fV Ioff=%.5fA\n", c->voltage_offset, c->current_offset);
    ECHO("R1=%.5f ohm  R2=%.5f ohm  Rshunt=%.6f ohm\n", c->r1_actual, c->r2_actual,
         c->shunt_actual);
}

/** @} */
