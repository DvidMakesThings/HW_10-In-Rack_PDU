/**
 * @file src/drivers/HLW8032_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2.0
 * @date 2025-11-08
 *
 * @details Provides RTOS-safe API for interfacing with the HLW8032 power
 * measurement chip. Features mutex-protected UART access, channel polling,
 * cached measurements, uptime tracking, and EEPROM-backed calibration.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define HLW_OFFSET 0.0f

/* =====================  Nominal component values  ===================== */
#define NOMINAL_R1 1880000.0f /* 1880 kOhm high side divider */
#define NOMINAL_R2 1000.0f    /* 1 kOhm low side divider */
#define NOMINAL_SHUNT 0.001f  /* 1 mOhm shunt resistor */

/* =====================  RTOS Mutex Handles  =========================== */
SemaphoreHandle_t uartHlwMtx = NULL;

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

/**
 * @brief Build a mask+value for a given pin/value onto a specific port bucket.
 * @param pin GPIO pin number
 * @param val Desired value (0 or 1)
 * @param maskA Pointer to port A mask accumulator
 * @param valA Pointer to port A value accumulator
 * @param maskB Pointer to port B mask accumulator
 * @param valB Pointer to port B value accumulator
 */
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

/**
 * @brief Return v if finite, else fallback fb.
 * @param v Value to check
 * @param fb Fallback value
 * @return v if finite, fb otherwise
 */
static inline float _finite_or(float v, float fb) { return isfinite(v) ? v : fb; }

/**
 * @brief Determine if MUX A/B lines need inversion for upper channels.
 * @return true if inversion needed (SW_REV == 100)
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
 * @param ch Channel index 0..7
 * @note Uses new MCP23017 driver API with mcp_selection() device
 * @note Uses busy_wait_us for hardware settling (MCP I/O delays)
 */
static void mux_select(uint8_t ch) {
    ch &= 0x07;

    uint8_t a = (ch >> 0) & 1;
    uint8_t b = (ch >> 1) & 1;
    uint8_t c = (ch >> 2) & 1;

#if defined(SW_REV) && (SW_REV == 100)
    if (c) {
        a ^= 1;
        b ^= 1;
    }
#endif

    uint8_t maskA = 0, valA = 0;
    uint8_t maskB = 0, valB = 0;

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

    /* Use CONFIG.h macros: MUX_A, MUX_B, MUX_C */
    ACCUM(MUX_A, a);
    ACCUM(MUX_B, b);
    ACCUM(MUX_C, c);

    /* Get selection MCP device */
    mcp23017_t *mcp_sel = mcp_selection();
    if (mcp_sel == NULL)
        return;

    /* Active-low enable: disable → set A/B/C → enable */
    mcp_write_pin(mcp_sel, MUX_EN, 1);
    busy_wait_us(200); /* Hardware settling time */

    if (maskA)
        mcp_write_mask(mcp_sel, 0, maskA, valA);
    if (maskB)
        mcp_write_mask(mcp_sel, 1, maskB, valB);
    busy_wait_us(500); /* Hardware settling time */

    mcp_write_pin(mcp_sel, MUX_EN, 0);
    busy_wait_us(500); /* Hardware settling time */

#undef ACCUM
}

/**
 * @brief Validate HLW8032 frame checksum.
 * @param f Pointer to 24-byte frame buffer
 * @return true if checksum matches
 */
static bool checksum_ok(const uint8_t *f) {
    uint8_t sum = 0;
    for (int i = 2; i <= 22; i++)
        sum += f[i];
    return sum == f[23];
}

/**
 * @brief Read one valid 24-byte frame from UART with timeout.
 * @return true if valid frame received and stored in `frame`
 * @note RTOS-compatible: uses vTaskDelay for cooperative scheduling
 */
static bool read_frame(void) {
    uint64_t start = time_us_64();
    int cnt = 0;

    /* Read with timeout, yield to other tasks between checks */
    while (cnt < MAX_RX_BYTES && (time_us_64() - start) < UART_READ_TIMEOUT_US) {
        if (uart_is_readable(HLW8032_UART_ID)) {
            rx_buf[cnt++] = uart_getc(HLW8032_UART_ID);
        } else {
            vTaskDelay(pdMS_TO_TICKS(1)); /* Yield CPU instead of busy-wait */
        }
    }

    if (cnt < HLW8032_FRAME_LENGTH)
        return false;

    /* Scan for header (0x55 0x5A) and validate checksum */
    for (int i = 0; i <= cnt - HLW8032_FRAME_LENGTH; i++) {
        if (rx_buf[i] == 0x55 && rx_buf[i + 1] == 0x5A) {
            memcpy(frame, &rx_buf[i], HLW8032_FRAME_LENGTH);
            if (checksum_ok(frame))
                return true;
        }
    }
    return false;
}

/**
 * @brief Extract 24-bit big-endian value from frame.
 * @param idx Starting index in frame
 * @return 24-bit value
 */
static inline uint32_t extract24(int idx) {
    return ((uint32_t)frame[idx] << 16) | ((uint32_t)frame[idx + 1] << 8) | frame[idx + 2];
}

/**
 * @brief Parse raw 24-byte frame into internal registers.
 * @return true on successful parse
 */
static bool parse_frame(void) {
    if (frame[0] != 0x55 || frame[1] != 0x5A || !checksum_ok(frame))
        return false;

    VolPar = extract24(2);
    VolData = extract24(5);
    CurPar = extract24(8);
    CurData = extract24(11);
    PowPar = extract24(14);
    PowData = extract24(17);
    PF = ((uint16_t)frame[20] << 8) | frame[21];
    uint8_t pf_cnt = frame[22];

    if (pf_cnt > 0)
        PF_Count = pf_cnt;

    return true;
}

/**
 * @brief Calculate calibrated voltage [V] from last read.
 * @param calib Pointer to channel calibration data
 * @return Voltage in volts
 */
static float calc_voltage(const hlw_calib_t *calib) {
    if (VolData == 0)
        return 0.0f;
    float vf = (calib->voltage_factor > 0.0f) ? calib->voltage_factor : HLW8032_VF;
    float voff = calib->voltage_offset;
    float v = ((float)VolPar / (float)VolData) * vf - voff;
    return _finite_or(v, 0.0f);
}

/**
 * @brief Calculate calibrated current [A] from last read.
 * @param calib Pointer to channel calibration data
 * @return Current in amps
 */
static float calc_current(const hlw_calib_t *calib) {
    if (CurData == 0)
        return 0.0f;
    float cf = (calib->current_factor > 0.0f) ? calib->current_factor : HLW8032_CF;
    float coff = calib->current_offset;
    float i = ((float)CurPar / (float)CurData) * cf - coff;
    return _finite_or(i, 0.0f);
}

/**
 * @brief Calculate calibrated active power [W] from last read.
 * @param calib Pointer to channel calibration data
 * @return Active power in watts
 */
static float calc_power(const hlw_calib_t *calib) {
    if (PowData == 0)
        return 0.0f;
    float vf = (calib->voltage_factor > 0.0f) ? calib->voltage_factor : HLW8032_VF;
    float cf = (calib->current_factor > 0.0f) ? calib->current_factor : HLW8032_CF;
    float p = ((float)PowPar / (float)PowData) * vf * cf;
    return _finite_or(p, 0.0f);
}

/* ===================================================================== */
/*                            Public API                                  */
/* ===================================================================== */

void hlw8032_init(void) {
    /* Initialize UART for HLW8032 communication */
    uart_init(HLW8032_UART_ID, HLW8032_BAUDRATE);
    gpio_set_function(UART0_TX, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX, GPIO_FUNC_UART);
    uart_set_format(HLW8032_UART_ID, 8, 1, UART_PARITY_EVEN);

    /* Load calibration from EEPROM */
    hlw8032_load_calibration();

    /* Initialize uptime tracking */
    for (uint8_t i = 0; i < 8; i++) {
        channel_last_on[i] = get_absolute_time();
        cached_state[i] = false;
    }
}

bool hlw8032_read(uint8_t channel) {
    if (channel >= 8)
        return false;

    /* Select channel via MUX (MCP access is mutex-protected internally) */
    mux_select(channel);
    vTaskDelay(pdMS_TO_TICKS(50)); /* Allow HLW to stabilize on new channel */

    /* Acquire UART mutex for exclusive access */
    if (uartHlwMtx != NULL) {
        xSemaphoreTake(uartHlwMtx, portMAX_DELAY);
    }

    /* Flush UART RX buffer */
    while (uart_is_readable(HLW8032_UART_ID))
        uart_getc(HLW8032_UART_ID);

    /* Read and parse frame */
    bool ok = read_frame() && parse_frame();

    if (uartHlwMtx != NULL) {
        xSemaphoreGive(uartHlwMtx);
    }

    if (ok) {
        last_channel_read = channel;
        last_voltage = calc_voltage(&channel_calib[channel]);
        last_current = calc_current(&channel_calib[channel]);
        last_power = calc_power(&channel_calib[channel]);
    }

    return ok;
}

float hlw8032_get_voltage(void) { return last_voltage; }

float hlw8032_get_current(void) { return last_current; }

float hlw8032_get_power(void) { return last_power; }

float hlw8032_get_power_inspect(void) { return last_voltage * last_current; }

void hlw8032_update_uptime(uint8_t ch, bool state) {
    if (ch >= 8)
        return;

    if (state && !cached_state[ch]) {
        /* Transition OFF -> ON: remember start of active period */
        channel_last_on[ch] = get_absolute_time();
    } else if (!state && cached_state[ch]) {
        /* Transition ON -> OFF: accumulate elapsed time into history */
        uint64_t elapsed_us = absolute_time_diff_us(channel_last_on[ch], get_absolute_time());
        channel_uptime[ch] += (uint32_t)(elapsed_us / 1000000ULL);
    }

    cached_state[ch] = state;
}

uint32_t hlw8032_get_uptime(uint8_t ch) {
    if (ch >= 8)
        return 0;

    uint32_t total = channel_uptime[ch];
    uint32_t current = 0;

    if (cached_state[ch]) {
        /* While ON, compute seconds since last ON */
        uint64_t elapsed_us = absolute_time_diff_us(channel_last_on[ch], get_absolute_time());
        current = (uint32_t)(elapsed_us / 1000000ULL);
    }

#ifdef USE_ACCUMULATED_UPTIME
    /* Lifetime uptime: accumulated history + current active slice */
    return total + current;
#else
    /* Session uptime: only the current active period */
    return cached_state[ch] ? current : 0u;
#endif
}

/* ===================================================================== */
/*                     Producer / Consumer API                            */
/* ===================================================================== */

void hlw8032_poll_once(void) {
    uint8_t ch = poll_channel;

    /* Query relay state from MCP driver */
    bool state = mcp_get_channel_state(ch);
    hlw8032_update_uptime(ch, state);

    /* Read measurement */
    bool ok = hlw8032_read(ch);
    cached_voltage[ch] = ok ? hlw8032_get_voltage() : 0.0f;
    cached_current[ch] = ok ? hlw8032_get_current() : 0.0f;
    cached_power[ch] = ok ? hlw8032_get_power() : 0.0f;
    cached_uptime[ch] = hlw8032_get_uptime(ch);

    /* Advance to next channel */
    poll_channel = (uint8_t)((ch + 1) % 8);
}

void hlw8032_refresh_all(void) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        bool state = mcp_get_channel_state(ch);
        hlw8032_update_uptime(ch, state);

        bool ok = hlw8032_read(ch);
        cached_voltage[ch] = ok ? hlw8032_get_voltage() : 0.0f;
        cached_current[ch] = ok ? hlw8032_get_current() : 0.0f;
        cached_power[ch] = ok ? hlw8032_get_power() : 0.0f;
        cached_uptime[ch] = hlw8032_get_uptime(ch);
    }
}

float hlw8032_cached_voltage(uint8_t ch) {
    float v = (ch < 8) ? cached_voltage[ch] : 0.0f;
    if (!(v >= 0.0f) || v > 400.0f)
        v = 0.0f;
    return v;
}

float hlw8032_cached_current(uint8_t ch) {
    float i = (ch < 8) ? cached_current[ch] : 0.0f;
    if (!(i >= 0.0f) || i > 100.0f)
        i = 0.0f;
    return i;
}

float hlw8032_cached_power(uint8_t ch) {
    if (ch >= 8)
        return 0.0f;
    const float v = hlw8032_cached_voltage(ch);
    const float i = hlw8032_cached_current(ch);
    const float p = v * i;
    return (p >= 0.0f && p < 400.0f * 100.0f) ? p : 0.0f;
}

uint32_t hlw8032_cached_uptime(uint8_t ch) { return (ch < 8) ? cached_uptime[ch] : 0u; }

bool hlw8032_cached_state(uint8_t ch) { return (ch < 8) ? cached_state[ch] : false; }

/* ===================================================================== */
/*                           Calibration section                          */
/* ===================================================================== */

void hlw8032_load_calibration(void) {
    for (uint8_t i = 0; i < 8; i++) {
        hlw_calib_t tmp;
        if (EEPROM_ReadSensorCalibrationForChannel(i, &tmp) == 0) {
            /* Sanitize loaded values */
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
            /* EEPROM read failed - use defaults */
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

bool hlw8032_calibrate_channel(uint8_t channel, float ref_voltage, float ref_current) {
    (void)ref_current; /* Current calibration not implemented */
    if (channel >= 8)
        return false;

    const int NUM_SAMPLES = 10;
    uint32_t vpar_sum = 0, vdat_sum = 0;
    int valid = 0;

    /* Collect samples */
    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (hlw8032_read(channel)) {
            vpar_sum += VolPar;
            vdat_sum += VolData;
            valid++;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); /* RTOS-compatible delay */
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
        /* Zero calibration: measure offset */
        const float v_meas = (avgvd > 0.f) ? (avgvp / avgvd) * vf : 0.0f;
        voff = v_meas;
        did_zero = true;
    } else {
        /* Voltage calibration: compute scale factor */
        if (ref_voltage > 0.0f && avgvp > 0.0f && avgvd > 0.0f) {
            vf = (ref_voltage + voff) * (avgvd / avgvp);
            did_v = true;
        } else {
            return false;
        }
    }

    /* Update calibration */
    channel_calib[channel].voltage_factor = vf;
    channel_calib[channel].voltage_offset = voff;
    channel_calib[channel].current_factor = HLW8032_CF;
    channel_calib[channel].current_offset = 0.0f;

    /* Update component values for reference */
    const float v_ratio = vf / HLW8032_VF;
    channel_calib[channel].r1_actual = NOMINAL_R1 * v_ratio;
    channel_calib[channel].r2_actual = NOMINAL_R2 * v_ratio;
    channel_calib[channel].shunt_actual = NOMINAL_SHUNT;

    channel_calib[channel].calibrated =
        (did_v || did_zero) ? 0xCA : channel_calib[channel].calibrated;
    channel_calib[channel].zero_calibrated =
        did_zero ? 0xCA : channel_calib[channel].zero_calibrated;

    /* Persist to EEPROM */
    if (EEPROM_WriteSensorCalibrationForChannel(channel, &channel_calib[channel]) != 0)
        return false;

    /* Note: ECHO macro removed for RTOS - logging via queue recommended */
    return true;
}

bool hlw8032_zero_calibrate_all(void) {
    uint8_t ok = 0;
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (hlw8032_calibrate_channel(ch, 0.0f, 0.0f))
            ok++;
    }
    return ok == 8;
}

bool hlw8032_get_calibration(uint8_t channel, hlw_calib_t *calib) {
    if (channel >= 8 || calib == NULL)
        return false;
    *calib = channel_calib[channel];
    return (calib->calibrated == 0xCA);
}

void hlw8032_print_calibration(uint8_t channel) {
    if (channel >= 8)
        return;
    /* Logging via queue recommended instead of direct log_printf */
    /* const hlw_calib_t *c = &channel_calib[channel]; */
    /* Send to logger queue instead of ECHO */
}
