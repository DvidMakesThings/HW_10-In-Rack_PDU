/**
 * @file HLW8032_driver.c
 * @author DvidMakesThings - David Sipos
 * @defgroup drivers Drivers
 * @brief HAL drivers for the Energis PDU firmware.
 * @{
 *
 * @defgroup driver2 2. HLW8032 Power Measurement Driver
 * @ingroup drivers
 * @brief RTOS-safe driver for HLW8032 power measurement IC
 * @{
 * @version 1.0.4
 * @date 2025-12-10
 *
 * @details RTOS-safe driver providing power measurement via HLW8032 ICs.
 *
 * Features:
 * - Thread-safe UART access via FreeRTOS mutex
 * - Hardware v1.0.0 pin swap compensation
 * - Per-channel calibration from EEPROM
 * - Cached measurements with uptime tracking
 * - Queue-based logging (no direct UART printf)
 *
 * Architecture:
 * - MeterTask is the sole owner of this driver
 * - All blocking operations use vTaskDelay() for RTOS cooperation
 * - Multiplexer control delegates to MCP23017 driver
 * - No ISRs - pure task-based polling
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define HLW8032_TAG "[HLW8032]"

/* =====================  Nominal Component Values  ======================== */
#define NOMINAL_R1 1880000.0f /* 1880 kohm high-side divider */
#define NOMINAL_R2 1000.0f    /* 1 kohm low-side divider */
#define NOMINAL_SHUNT 0.001f  /* 1 mohm shunt resistor */

/* =====================  RTOS Mutex Handle  =============================== */
SemaphoreHandle_t uartHlwMtx = NULL;

/* =====================  Frame & Bookkeeping  ============================= */
static uint8_t frame[HLW8032_FRAME_LENGTH];
static uint8_t rx_buf[MAX_RX_BYTES];
static uint32_t channel_uptime[8] = {0};
static absolute_time_t channel_last_on[8];

/* =====================  Per-Channel Calibration  ========================= */
static hlw_calib_t channel_calib[8];

/* =====================  Last Raw Registers  ============================== */
static uint32_t VolPar, VolData;
static uint32_t CurPar, CurData;
static uint32_t PowPar, PowData;
static uint16_t PF;
static uint32_t PF_Count = 1;
static uint8_t last_channel_read = 0xFF;
static uint8_t last_state_reg = 0x55; /* Track overflow flags */

/* =====================  Last Scaled Readings  ============================ */
static float last_voltage = 0.0f;
static float last_current = 0.0f;
static float last_power = 0.0f;

/* =====================  Cached Measurements  ============================= */
static float cached_voltage[8];
static float cached_current[8];
static float cached_power[8];
static uint32_t cached_uptime[8];
static bool cached_state[8];

/* =====================  Polling Helper  ================================== */
static uint8_t poll_channel = 0;

/* ===================================================================== */
/*                        Internal Helpers (Static)                      */
/* ===================================================================== */

/* =====================  Async Calibration Forward  ======================= */

/**
 * @brief Feed one fresh HLW8032 sample into the async calibration engine.
 *
 * @param ch Channel index [0..7] that has just been read successfully.
 */
static void hlw8032_calibration_consume_sample(uint8_t ch);

/**
 * @brief Select HLW8032 channel via multiplexer.
 *
 * @details
 * - Disables MUX (EN=1) during transition
 * - Sets A/B/C select lines via SwitchTask -> MCP23017 Port B
 * - Hardware v1.0.0: XORs A and B bits when C=1 (upper 4 channels)
 * - Enables MUX (EN=0)
 * - Uses cooperative vTaskDelay() for settling to prevent watchdog starvation
 *
 * @param ch Logical channel index [0..7]
 */
static void mux_select(uint8_t ch) {
    ch &= 0x07;

    uint8_t a = (ch >> 0) & 1u;
    uint8_t b = (ch >> 1) & 1u;
    uint8_t c = (ch >> 2) & 1u;

#if defined(HW_REV) && (HW_REV == 100)
    /* Hardware v1.0.0: XOR A and B bits when C=1 (upper channels 4-7) */
    if (c) {
        a ^= 1u;
        b ^= 1u;
    }
#endif

    /* MUX lines are on Port B bits 0-3 */
    const uint8_t MUX_MASK = 0x0Fu;

    /* Step 1: Disable MUX (EN=1) and set address lines in ONE logical write */
    uint8_t value = (1u << 3) | /* MUX_EN = 1 (disabled, active-low) */
                    (a << 0) |  /* MUX_A */
                    (b << 1) |  /* MUX_B */
                    (c << 2);   /* MUX_C */

    if (!Switch_SetRelayPortBMasked(MUX_MASK, value, 10u)) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0x1);
        ERROR_PRINT_CODE(err_code, "%s Switch_SetRelayPortBMasked failed (EN=1)\r\n", HLW8032_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return;
    }

    /* Allow MUX lines to settle before enabling */
    vTaskDelay(pdMS_TO_TICKS(1));

    /* Step 2: Enable MUX (EN=0), keep A/B/C as-is */
    value &= (uint8_t)~(1u << 3); /* clear EN bit -> enable */

    if (!Switch_SetRelayPortBMasked(MUX_MASK, value, 10u)) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0x2);
        ERROR_PRINT_CODE(err_code, "%s Switch_SetRelayPortBMasked failed (EN=0)\r\n", HLW8032_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
}

/**
 * @brief Validate HLW8032 frame checksum.
 *
 * @details Checksum = sum of bytes [2..22] (inclusive).
 * Expected to match byte [23].
 *
 * @param f Pointer to 24-byte frame buffer
 * @return true if checksum valid
 */
static bool checksum_ok(const uint8_t *f) {
    uint8_t sum = 0;
    for (int i = 2; i <= 22; i++)
        sum += f[i];
    return sum == f[23];
}

/**
 * @brief Check if HLW8032 State REG indicates a valid frame.
 *
 * @details Per HLW8032 datasheet (page 12):
 * - 0x55: Normal operation, all registers valid, no overflow
 * - 0xFx (0xF0-0xFF): Normal operation, some registers overflow (value approaches 0)
 * - 0xAA: Chip error correction failed, parameters unusable
 *
 * @param state State REG value (byte 0 of frame)
 * @return true if State REG indicates valid data
 */
static bool state_reg_valid(uint8_t state) {
    /* 0x55 = all normal, 0xFx = normal with overflow flags */
    if (state == 0x55)
        return true;
    if ((state & 0xF0) == 0xF0)
        return true;
    /* 0xAA = error correction failure - reject */
    return false;
}

/**
 * @brief Safely extract uint24_t (big-endian) from frame.
 *
 * @param f Pointer to first byte
 * @return uint32_t 24-bit value as 32-bit integer
 */
static inline uint32_t extract_u24(const uint8_t *f) {
    return ((uint32_t)f[0] << 16) | ((uint32_t)f[1] << 8) | f[2];
}

/**
 * @brief Safely extract uint16_t (big-endian) from frame.
 *
 * @param f Pointer to first byte
 * @return uint16_t 16-bit value
 */
static inline uint16_t extract_u16(const uint8_t *f) { return ((uint16_t)f[0] << 8) | f[1]; }

/**
 * @brief Read one valid 24-byte HLW8032 frame from UART.
 *
 * @details
 * - Reads up to MAX_RX_BYTES to capture at least one complete frame
 * - Searches for valid State REG (0x55 or 0xFx) followed by Check REG 0x5A
 * - Validates frame length and checksum
 * - Stores valid frame in global 'frame' buffer
 * - Uses vTaskDelay() for RTOS cooperation during waits
 *
 * @return true if valid frame received
 * @return false on timeout or validation failure
 *
 * @note Blocks up to HLW_UART_TIMEOUT_US
 *
 * @note FIXED v1.0.2: Frame header search now accepts State REG = 0x55 OR 0xFx.
 *       Previously only 0x55 was accepted, causing frames to be rejected when
 *       overflow flags were set (normal for low/no load conditions).
 */
static bool read_frame(void) {
    // DEBUG_PRINT("%s read_frame() called\r\n", HLW8032_TAG);
    uint64_t start = time_us_64();

    /* Read enough bytes to guarantee capturing one 24-byte frame */
    uint32_t total = 0;
    while (total < MAX_RX_BYTES) {
        while (uart_is_readable(HLW8032_UART_ID) && total < MAX_RX_BYTES) {
            rx_buf[total++] = uart_getc(HLW8032_UART_ID);
        }
        if ((time_us_64() - start) > HLW_UART_TIMEOUT_US)
            break;
        vTaskDelay(pdMS_TO_TICKS(1)); /* Cooperative yield */
    }

    if (total < HLW8032_FRAME_LENGTH) {
        // DEBUG_PRINT("%s Timeout: only %lu bytes received\r\n", HLW8032_TAG, (unsigned
        // long)total);
        return false;
    }

    /*
     * FIXED v1.0.2: Search for valid frame header.
     * Per HLW8032 datasheet:
     * - Byte 0 (State REG): 0x55 (normal) or 0xFx (overflow, still valid)
     * - Byte 1 (Check REG): Always 0x5A
     *
     * Previously the code only accepted State REG = 0x55, which failed for
     * low-load conditions where overflow flags are set (State REG = 0xF4, 0xF6, etc.)
     */
    for (uint32_t i = 0; i <= total - HLW8032_FRAME_LENGTH; i++) {
        uint8_t state = rx_buf[i];
        uint8_t check = rx_buf[i + 1];

        /* Check for valid State REG and Check REG = 0x5A */
        if (state_reg_valid(state) && check == 0x5A) {
            memcpy(frame, &rx_buf[i], HLW8032_FRAME_LENGTH);
            if (checksum_ok(frame)) {
                // DEBUG_PRINT("%s Valid frame found at offset %lu, State=0x%02X\r\n",
                //             HLW8032_TAG, (unsigned long)i, state);
                return true;
            } else {
                // DEBUG_PRINT("%s Frame header found but checksum failed\r\n", HLW8032_TAG);
            }
        }
    }

    // DEBUG_PRINT("%s No valid frame found in %lu bytes\r\n", HLW8032_TAG, (unsigned long)total);
    return false;
}

/**
 * @brief Parse HLW8032 frame and extract raw register values.
 *
 * @details Frame structure (24 bytes) per HLW8032 datasheet:
 * [0]: State REG - data validity flags
 * [1]: Check REG - always 0x5A
 * [2-4]: Voltage Parameter REG (factory calibration constant)
 * [5-7]: Voltage REG (measurement)
 * [8-10]: Current Parameter REG (factory calibration constant)
 * [11-13]: Current REG (measurement)
 * [14-16]: Power Parameter REG (factory calibration constant)
 * [17-19]: Power REG (measurement)
 * [20]: Data Update REG
 * [21-22]: PF REG
 * [23]: Checksum
 *
 * @param f Pointer to 24-byte frame buffer
 *
 * @note Updates global raw register variables
 * @note Saves State REG for overflow detection
 */
static void parse_frame(const uint8_t *f) {
    /* Save State REG for overflow flag checking */
    last_state_reg = f[0];

    VolPar = extract_u24(&f[2]);
    VolData = extract_u24(&f[5]);
    CurPar = extract_u24(&f[8]);
    CurData = extract_u24(&f[11]);
    PowPar = extract_u24(&f[14]);
    PowData = extract_u24(&f[17]);

    /* Data Update REG at byte 20, PF REG at bytes 21-22 */
    uint8_t data_update = f[20];
    PF = extract_u16(&f[21]);

    /* PF overflow tracking from Data Update REG bit 7 */
    (void)data_update; /* Not currently used */

    // DEBUG_PRINT("%s Parsed: State=0x%02X VolPar=%lu VolData=%lu CurPar=%lu CurData=%lu "
    //            "PowPar=%lu PowData=%lu PF=%u\r\n",
    //            HLW8032_TAG, last_state_reg, (unsigned long)VolPar, (unsigned long)VolData,
    //            (unsigned long)CurPar, (unsigned long)CurData, (unsigned long)PowPar,
    //            (unsigned long)PowData, PF);
}

/**
 * @brief Apply calibration and compute scaled measurements.
 *
 * @details Per HLW8032 datasheet (pages 15-16):
 * - Voltage = (Voltage_Parameter_REG / Voltage_REG) x Voltage_coefficient
 * - Current = (Current_Parameter_REG / Current_REG) x Current_coefficient
 * - Power = (Power_Parameter_REG / Power_REG) x V_coeff x I_coeff
 *
 * Overflow handling per State REG:
 * - Bit 3 = 1: Voltage REG overflow (voltage 0)
 * - Bit 2 = 1: Current REG overflow (current 0)
 * - Bit 1 = 1: Power REG overflow (power 0)
 *
 * @param ch Channel index [0..7] for calibration lookup
 *
 * @note Updates global last_voltage, last_current, last_power
 * @note Clamps invalid results to 0.0
 */
static void apply_calibration(uint8_t ch) {
    if (ch >= 8) {
        last_voltage = last_current = last_power = 0.0f;
        return;
    }

    const hlw_calib_t *cal = &channel_calib[ch];

    /*
     * Check overflow flags in State REG (per datasheet page 12):
     * - Bit 3: Voltage REG overflow
     * - Bit 2: Current REG overflow
     * - Bit 1: Power REG overflow
     * When a register overflows, its value approaches zero.
     */
    bool voltage_overflow = (last_state_reg & 0x08) != 0;
    bool current_overflow = (last_state_reg & 0x04) != 0;
    bool power_overflow = (last_state_reg & 0x02) != 0;

    /* Voltage calculation */
    if (voltage_overflow || VolData == 0) {
        last_voltage = 0.0f;
    } else {
        float v_raw = ((float)VolPar / (float)VolData) * cal->voltage_factor;
        last_voltage = v_raw - cal->voltage_offset;
    }

    /* Current calculation
     *
     * NOTE: We no longer force current to 0.0f when current_overflow is set.
     * The HLW8032 uses this bit to indicate "very small, close to 0" current,
     * but in this design we rely on our own offset calibration instead.
     * We only treat CurData == 0 as invalid.
     */
    if (CurData == 0) {
        last_current = 0.0f;
    } else {
        float i_raw = ((float)CurPar / (float)CurData) * cal->current_factor;
        last_current = i_raw - cal->current_offset;
    }

    /* Sanity checks */
    if (!(last_voltage >= 0.0f) || last_voltage > 400.0f)
        last_voltage = 0.0f;
    if (!(last_current >= 0.0f) || last_current > 100.0f)
        last_current = 0.0f;

    /* Power calculation - can use direct measurement or V*I */
    if (power_overflow || PowData == 0) {
        last_power = last_voltage * last_current;
    } else {
        /* Direct power measurement from HLW8032 */
        float p_raw = ((float)PowPar / (float)PowData) * cal->voltage_factor * cal->current_factor;
        last_power = p_raw;

        /* Cross-check with V*I - use V*I if power measurement seems off */
        float p_vi = last_voltage * last_current;
        if (p_raw < 0.0f || p_raw > 40000.0f ||
            (p_vi > 0.1f && fabsf(p_raw - p_vi) / p_vi > 0.5f)) {
            last_power = p_vi;
        }
    }

    if (!(last_power >= 0.0f) || last_power > 40000.0f)
        last_power = 0.0f;
}

/* ===================================================================== */
/*                        Public API Implementation                       */
/* ===================================================================== */

void hlw8032_init(void) {
    INFO_PRINT("%s Initializing HLW8032 driver v1.0.2...\r\n", HLW8032_TAG);

    /* Create UART mutex */
    uartHlwMtx = xSemaphoreCreateMutex();
    if (uartHlwMtx == NULL) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0x2);
        ERROR_PRINT_CODE(err_code, "%s Failed to create UART mutex\r\n", HLW8032_TAG);
        Storage_EnqueueErrorCode(err_code);
        return;
    }

    /* Initialize UART0 for HLW8032 */
    uart_init(HLW8032_UART_ID, HLW8032_BAUDRATE);
    gpio_set_function(UART0_TX, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX, GPIO_FUNC_UART);

    /* Configure UART for HLW8032: 8 data bits, even parity, 1 stop bit */
    uart_set_format(HLW8032_UART_ID, 8, 1, UART_PARITY_EVEN);

    /* Initialize all state arrays */
    for (uint8_t i = 0; i < 8; i++) {
        channel_uptime[i] = 0;
        channel_last_on[i] = nil_time;
        cached_state[i] = false;
        cached_voltage[i] = 0.0f;
        cached_current[i] = 0.0f;
        cached_power[i] = 0.0f;
        cached_uptime[i] = 0;
    }

    /* Load calibration from EEPROM */
    hlw8032_load_calibration();

    INFO_PRINT("%s Initialization complete\r\n", HLW8032_TAG);
}

bool hlw8032_read(uint8_t ch) {
    if (ch >= 8) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0x4);
        ERROR_PRINT_CODE(err_code, "%s Invalid channel %u\r\n", HLW8032_TAG, (unsigned)ch);
        Storage_EnqueueErrorCode(err_code);
        return false;
    }

    /* Acquire UART mutex */
    if (xSemaphoreTake(uartHlwMtx, pdMS_TO_TICKS(100)) != pdTRUE) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_HLW8032, 0x4);
        WARNING_PRINT_CODE(err_code, "%s Failed to acquire UART mutex\r\n", HLW8032_TAG);
        return false;
    }

    /* Select channel via multiplexer */
    mux_select(ch);

    /* Drain any stale data */
    while (uart_is_readable(HLW8032_UART_ID))
        uart_getc(HLW8032_UART_ID);

    /* Wait for frame (HLW transmits every ~50ms per datasheet) */
    vTaskDelay(pdMS_TO_TICKS(60));

    /* Read and validate frame */
    bool success = read_frame();

    if (success) {
        parse_frame(frame);
        apply_calibration(ch);
        last_channel_read = ch;

        /* Feed sample into async calibration engine (if active) */
        hlw8032_calibration_consume_sample(ch);

        // DEBUG_PRINT("%s READ CH%u: last_v=%.2f last_i=%.3f last_p=%.2f\r\n", HLW8032_TAG,
        //            (unsigned)ch, last_voltage, last_current, last_power);
    } else {
        last_voltage = last_current = last_power = 0.0f;
        // DEBUG_PRINT("%s Failed to read valid frame from CH%u\r\n", HLW8032_TAG, (unsigned)ch);
    }

    xSemaphoreGive(uartHlwMtx);
    return success;
}

float hlw8032_get_voltage(void) { return last_voltage; }

float hlw8032_get_current(void) { return last_current; }

float hlw8032_get_power(void) { return last_power; }

uint32_t hlw8032_get_uptime(uint8_t ch) { return (ch < 8) ? channel_uptime[ch] : 0u; }

void hlw8032_update_uptime(uint8_t ch, bool state) {
    if (ch >= 8)
        return;

    absolute_time_t now = get_absolute_time();

    if (state) {
        /* ON: accumulate time since last check */
        if (!is_nil_time(channel_last_on[ch])) {
            int64_t elapsed_us = absolute_time_diff_us(channel_last_on[ch], now);
            if (elapsed_us > 0 && elapsed_us < 10000000LL) { /* sanity: <10s */
                channel_uptime[ch] += (uint32_t)(elapsed_us / 1000000LL);
            }
        }
        channel_last_on[ch] = now;
    } else {
        /* OFF: stop accumulating */
        channel_last_on[ch] = nil_time;
    }
}

void hlw8032_poll_once(void) {
    uint8_t ch = poll_channel;

    /* Get relay state from MCP driver */
    bool state = false;
    (void)Switch_GetState(ch, &state);
    cached_state[ch] = state;

    /* Update uptime tracking */
    hlw8032_update_uptime(ch, state);
    cached_uptime[ch] = hlw8032_get_uptime(ch);

    /* Read measurements */
    if (hlw8032_read(ch)) {
        cached_voltage[ch] = last_voltage;
        cached_current[ch] = last_current;
        cached_power[ch] = last_power;
    }

    /* Advance to next channel (round-robin) */
    poll_channel = (poll_channel + 1) & 0x07;
}

void hlw8032_refresh_all(void) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        bool state = false;
        (void)Switch_GetState(ch, &state);
        cached_state[ch] = state;
        hlw8032_update_uptime(ch, state);
        cached_uptime[ch] = hlw8032_get_uptime(ch);

        if (hlw8032_read(ch)) {
            cached_voltage[ch] = last_voltage;
            cached_current[ch] = last_current;
            cached_power[ch] = last_power;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ===================================================================== */
/*                        Cached Accessors                                */
/* ===================================================================== */

float hlw8032_cached_voltage(uint8_t ch) {
    if (ch >= 8)
        return 0.0f;
    const float v = cached_voltage[ch];
    return (v >= 0.0f && v < 400.0f) ? v : 0.0f;
}

float hlw8032_cached_current(uint8_t ch) {
    if (ch >= 8)
        return 0.0f;
    const float i = cached_current[ch];
    return (i >= 0.0f && i < 100.0f) ? i : 0.0f;
}

float hlw8032_cached_power(uint8_t ch) {
    if (ch >= 8)
        return 0.0f;
    const float v = hlw8032_cached_voltage(ch);
    const float i = hlw8032_cached_current(ch);
    const float p = v * i;
    return (p >= 0.0f && p < 40000.0f) ? p : 0.0f;
}

uint32_t hlw8032_cached_uptime(uint8_t ch) { return (ch < 8) ? cached_uptime[ch] : 0u; }

bool hlw8032_cached_state(uint8_t ch) { return (ch < 8) ? cached_state[ch] : false; }

/* ===================================================================== */
/*                           Calibration Section                         */
/* ===================================================================== */

void hlw8032_load_calibration(void) {
    INFO_PRINT("%s Loading calibration from EEPROM...\r\n", HLW8032_TAG);

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

    INFO_PRINT("%s Calibration load complete\r\n", HLW8032_TAG);
}

/* =====================  Calibration State (Async)  ====================== */

/**
 * @brief Asynchronous HLW8032 auto-calibration mode.
 */
typedef enum {
    HLW_CAL_MODE_IDLE = 0, /**< No calibration in progress. */
    HLW_CAL_MODE_ZERO_ALL, /**< Zero-point auto calibration (0V, 0A). */
    HLW_CAL_MODE_VOLT_ALL, /**< Voltage auto calibration (Vref, 0A). */
    HLW_CAL_MODE_CURR_ALL  /**< Current gain calibration (Iref, with load). */
} hlw_cal_mode_t;

/**
 * @brief Internal state for the asynchronous HLW8032 auto-calibration engine.
 *
 * This state is driven cooperatively from normal polling via hlw8032_read(),
 * so calibration runs without blocking any single task or starving the WD.
 */
typedef struct {
    hlw_cal_mode_t mode;       /**< Current calibration mode. */
    bool running;              /**< True while a calibration sequence is active. */
    float ref_voltage;         /**< Reference voltage in volts (for VOLT mode). */
    float ref_current;         /**< Reference current in amps (for CURR mode). */
    uint8_t current_channel;   /**< Channel currently being calibrated [0..7]. */
    uint8_t total_channels;    /**< Number of channels in sequence (normally 8). */
    uint8_t samples_target;    /**< Required number of samples per channel. */
    uint8_t samples_collected; /**< Total samples collected on current channel. */
    uint8_t valid_samples;     /**< Valid samples counted for current channel. */
    uint32_t vpar_sum;         /**< Accumulated VolPar over valid samples. */
    uint32_t vdat_sum;         /**< Accumulated VolData over valid samples. */
    uint32_t cpar_sum;         /**< Accumulated CurPar over valid samples. */
    uint32_t cdat_sum;         /**< Accumulated CurData over valid samples. */
    uint8_t ok_channels;       /**< Number of channels calibrated successfully. */
    uint8_t failed_channels;   /**< Number of channels that failed calibration. */
} hlw_async_cal_state_t;

/**
 * @brief Global async calibration engine state.
 */
static hlw_async_cal_state_t s_hlw_cal_state = {.mode = HLW_CAL_MODE_IDLE,
                                                .running = false,
                                                .ref_voltage = 0.0f,
                                                .ref_current = 0.0f,
                                                .current_channel = 0u,
                                                .total_channels = 0u,
                                                .samples_target = 0u,
                                                .samples_collected = 0u,
                                                .valid_samples = 0u,
                                                .vpar_sum = 0u,
                                                .vdat_sum = 0u,
                                                .cpar_sum = 0u,
                                                .cdat_sum = 0u,
                                                .ok_channels = 0u,
                                                .failed_channels = 0u};

/**
 * @brief Number of HLW8032 samples per channel used during auto-calibration.
 */
static const uint8_t HLW_CAL_SAMPLES_PER_CH = 10u;

/* ===================================================================== */
/*                        Internal Helpers (Static)                      */
/* ===================================================================== */

/**
 * @brief Reset sample accumulators for the current async calibration channel.
 */
static void hlw8032_calibration_reset_accumulators(void) {
    s_hlw_cal_state.samples_collected = 0u;
    s_hlw_cal_state.valid_samples = 0u;
    s_hlw_cal_state.vpar_sum = 0u;
    s_hlw_cal_state.vdat_sum = 0u;
    s_hlw_cal_state.cpar_sum = 0u;
    s_hlw_cal_state.cdat_sum = 0u;
}

/**
 * @brief Finish calibration for the current channel using accumulated samples.
 *
 * @return true if calibration for the current channel succeeded.
 * @return false if there were insufficient valid samples or EEPROM write failed.
 */
static bool hlw8032_calibration_finish_current_channel(void) {
    uint8_t channel = s_hlw_cal_state.current_channel;
    const int NUM_SAMPLES = (int)s_hlw_cal_state.samples_target;
    const int valid = (int)s_hlw_cal_state.valid_samples;

    if (channel >= 8u) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0x5);
        ERROR_PRINT_CODE(err_code, "%s Async calib invalid channel %u\r\n", HLW8032_TAG,
                         (unsigned)channel);
        return false;
    }

    if (valid < (NUM_SAMPLES / 2)) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_HLW8032, 0x6);
        WARNING_PRINT_CODE(err_code, "%s Async calib insufficient samples for CH%u: %d/%d\r\n",
                           HLW8032_TAG, (unsigned)channel, valid, NUM_SAMPLES);
        return false;
    }

    float vf = (channel_calib[channel].voltage_factor > 0.0f)
                   ? channel_calib[channel].voltage_factor
                   : HLW8032_VF;
    float voff = channel_calib[channel].voltage_offset;

    float cf = (channel_calib[channel].current_factor > 0.0f)
                   ? channel_calib[channel].current_factor
                   : HLW8032_CF;
    float ioff = channel_calib[channel].current_offset;

    const float avgvp = (float)s_hlw_cal_state.vpar_sum / (float)valid;
    const float avgvd = (float)s_hlw_cal_state.vdat_sum / (float)valid;
    const float avgcp = (float)s_hlw_cal_state.cpar_sum / (float)valid;
    const float avgcd = (float)s_hlw_cal_state.cdat_sum / (float)valid;

    bool did_zero = false;
    bool did_v = false;
    bool did_i = false;

    if (s_hlw_cal_state.mode == HLW_CAL_MODE_ZERO_ALL) {
        /* Zero calibration: measure both voltage and current offset at 0V/0A */
        const float v_meas = (avgvd > 0.0f) ? (avgvp / avgvd) * vf : 0.0f;
        const float i_meas = (avgcd > 0.0f) ? (avgcp / avgcd) * cf : 0.0f;

        voff = v_meas;
        ioff = i_meas;
        did_zero = true;

        INFO_PRINT("%s CH%u async zero calibration: Voff=%.3fV Ioff=%.3fA\r\n", HLW8032_TAG,
                   (unsigned)channel, voff, ioff);
    } else if (s_hlw_cal_state.mode == HLW_CAL_MODE_VOLT_ALL) {
        /* Voltage calibration: compute voltage scale factor, keep current as is */
        const float ref_voltage = s_hlw_cal_state.ref_voltage;

        if (ref_voltage > 0.0f && avgvp > 0.0f && avgvd > 0.0f) {
            vf = (ref_voltage + voff) * (avgvd / avgvp);
            did_v = true;
            INFO_PRINT("%s CH%u async voltage calibration: VF=%.3f (ref=%.2fV)\r\n", HLW8032_TAG,
                       (unsigned)channel, vf, ref_voltage);
        } else {
            uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_HLW8032, 0x7);
            WARNING_PRINT_CODE(err_code, "%s CH%u async V-cal failed: invalid readings\r\n",
                               HLW8032_TAG, (unsigned)channel);
            return false;
        }
    } else if (s_hlw_cal_state.mode == HLW_CAL_MODE_CURR_ALL) {
        /* Current calibration: compute current scale factor, keep offsets */
        const float ref_current = s_hlw_cal_state.ref_current;

        if (ref_current <= 0.0f || avgcp <= 0.0f || avgcd <= 0.0f) {
            uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_HLW8032, 0x9);
            WARNING_PRINT_CODE(err_code,
                               "%s CH%u async I-cal failed: invalid readings/ref I=%.3f\r\n",
                               HLW8032_TAG, (unsigned)channel, ref_current);
            return false;
        }

        /* We want: Iref = (CurPar/CurData) * Cf_new - Ioff  => Cf_new = (Iref + Ioff) *
         * (CurData/CurPar) */
        const float ratio = avgcp / avgcd;
        if (ratio <= 0.0f) {
            uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_HLW8032, 0xA);
            WARNING_PRINT_CODE(err_code, "%s CH%u async I-cal failed: invalid ratio\r\n",
                               HLW8032_TAG, (unsigned)channel);
            return false;
        }

        cf = (ref_current + ioff) * (avgcd / avgcp);
        did_i = true;

        INFO_PRINT("%s CH%u async current calibration: CF=%.6f (ref=%.3fA, Ioff=%.3fA)\r\n",
                   HLW8032_TAG, (unsigned)channel, cf, ref_current, ioff);
    } else {
        /* Should not happen */
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0xB);
        ERROR_PRINT_CODE(err_code, "%s Async calib in invalid mode\r\n", HLW8032_TAG);
        return false;
    }

    /* Update calibration */
    channel_calib[channel].voltage_factor = vf;
    channel_calib[channel].voltage_offset = voff;
    channel_calib[channel].current_factor = cf;
    channel_calib[channel].current_offset = ioff;

    /* Update component values for reference (voltage path only) */
    const float v_ratio = vf / HLW8032_VF;
    channel_calib[channel].r1_actual = NOMINAL_R1 * v_ratio;
    channel_calib[channel].r2_actual = NOMINAL_R2 * v_ratio;
    channel_calib[channel].shunt_actual = NOMINAL_SHUNT;

    channel_calib[channel].calibrated =
        (did_v || did_zero || did_i) ? 0xCA : channel_calib[channel].calibrated;
    channel_calib[channel].zero_calibrated =
        did_zero ? 0xCA : channel_calib[channel].zero_calibrated;

    /* Persist to EEPROM */
    if (EEPROM_WriteSensorCalibrationForChannel(channel, &channel_calib[channel]) != 0) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0x8);
        ERROR_PRINT_CODE(err_code, "%s Async calib failed to write EEPROM for CH%u\r\n",
                         HLW8032_TAG, (unsigned)channel);
        return false;
    }

    INFO_PRINT("%s CH%u async calibration complete and saved\r\n", HLW8032_TAG, (unsigned)channel);
    return true;
}

/**
 * @brief Feed one fresh HLW8032 sample into the async calibration engine.
 *
 * @param ch Channel index [0..7] that has just been read successfully.
 */
static void hlw8032_calibration_consume_sample(uint8_t ch) {
    if (!s_hlw_cal_state.running)
        return;
    if (ch != s_hlw_cal_state.current_channel)
        return;

    s_hlw_cal_state.samples_collected++;
    s_hlw_cal_state.vpar_sum += VolPar;
    s_hlw_cal_state.vdat_sum += VolData;
    s_hlw_cal_state.cpar_sum += CurPar;
    s_hlw_cal_state.cdat_sum += CurData;
    s_hlw_cal_state.valid_samples++;

    if (s_hlw_cal_state.samples_collected < s_hlw_cal_state.samples_target) {
        return;
    }

    /* Enough samples for this channel, compute and store calibration */
    bool ok = hlw8032_calibration_finish_current_channel();
    if (ok) {
        s_hlw_cal_state.ok_channels++;
    } else {
        s_hlw_cal_state.failed_channels++;
    }

    /* Move to next channel or finish sequence */
    if ((uint8_t)(s_hlw_cal_state.current_channel + 1u) < s_hlw_cal_state.total_channels) {
        s_hlw_cal_state.current_channel++;
        hlw8032_calibration_reset_accumulators();
        INFO_PRINT("%s Async calib: moving to CH%u\r\n", HLW8032_TAG,
                   (unsigned)s_hlw_cal_state.current_channel);
    } else {
        const char *mode_str = "unknown";
        if (s_hlw_cal_state.mode == HLW_CAL_MODE_ZERO_ALL)
            mode_str = "zero";
        else if (s_hlw_cal_state.mode == HLW_CAL_MODE_VOLT_ALL)
            mode_str = "voltage";
        else if (s_hlw_cal_state.mode == HLW_CAL_MODE_CURR_ALL)
            mode_str = "current";

        INFO_PRINT("%s Async %s calibration complete: %u ok, %u failed\r\n", HLW8032_TAG, mode_str,
                   (unsigned)s_hlw_cal_state.ok_channels,
                   (unsigned)s_hlw_cal_state.failed_channels);

        s_hlw_cal_state.mode = HLW_CAL_MODE_IDLE;
        s_hlw_cal_state.running = false;
    }
}

/* ===================================================================== */
/*                        Public Calibration API                         */
/* ===================================================================== */

/**
 * @brief Start asynchronous zero calibration (0V/0A) for all channels.
 *
 * @return true if calibration sequence successfully started
 * @return false if another calibration is already running
 */
bool hlw8032_calibration_start_zero_all(void) {
    if (s_hlw_cal_state.running) {
        WARNING_PRINT("%s Async calibration already in progress\r\n", HLW8032_TAG);
        return false;
    }

    s_hlw_cal_state.mode = HLW_CAL_MODE_ZERO_ALL;
    s_hlw_cal_state.running = true;
    s_hlw_cal_state.ref_voltage = 0.0f;
    s_hlw_cal_state.ref_current = 0.0f;
    s_hlw_cal_state.current_channel = 0u;
    s_hlw_cal_state.total_channels = 8u;
    s_hlw_cal_state.samples_target = HLW_CAL_SAMPLES_PER_CH;
    s_hlw_cal_state.ok_channels = 0u;
    s_hlw_cal_state.failed_channels = 0u;
    hlw8032_calibration_reset_accumulators();

    INFO_PRINT("%s Async zero calibration started for all channels (0V/0A)\r\n", HLW8032_TAG);
    INFO_PRINT("%s Ensure all relays OFF and no load\r\n", HLW8032_TAG);

    return true;
}

/**
 * @brief Start asynchronous voltage calibration (Vref/0A) for all channels.
 *
 * @param ref_voltage Reference voltage in volts (must be > 0.0f)
 * @return true if calibration sequence successfully started
 * @return false if another calibration is running or ref_voltage invalid
 */
bool hlw8032_calibration_start_voltage_all(float ref_voltage) {
    if (ref_voltage <= 0.0f) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0xA);
        ERROR_PRINT_CODE(err_code, "%s Invalid reference voltage: %.3f\r\n", HLW8032_TAG,
                         ref_voltage);
        return false;
    }

    if (s_hlw_cal_state.running) {
        WARNING_PRINT("%s Async calibration already in progress\r\n", HLW8032_TAG);
        return false;
    }

    s_hlw_cal_state.mode = HLW_CAL_MODE_VOLT_ALL;
    s_hlw_cal_state.running = true;
    s_hlw_cal_state.ref_voltage = ref_voltage;
    s_hlw_cal_state.ref_current = 0.0f;
    s_hlw_cal_state.current_channel = 0u;
    s_hlw_cal_state.total_channels = 8u;
    s_hlw_cal_state.samples_target = HLW_CAL_SAMPLES_PER_CH;
    s_hlw_cal_state.ok_channels = 0u;
    s_hlw_cal_state.failed_channels = 0u;
    hlw8032_calibration_reset_accumulators();

    INFO_PRINT("%s Async voltage calibration started for all channels (%.1fV, 0A)\r\n", HLW8032_TAG,
               ref_voltage);
    INFO_PRINT("%s Ensure all channels see same stable mains voltage\r\n", HLW8032_TAG);

    return true;
}

/**
 * @brief Start asynchronous current calibration (Iref) for a single channel.
 *
 * @param channel     Channel index [0..7] to be calibrated.
 * @param ref_current Reference current in amps (must be > 0.0f)
 *
 * @return true if calibration sequence successfully started
 * @return false if another calibration is running, channel invalid, or ref_current invalid
 *
 * @note Requires that the selected channel carries the known current (use a DMM).
 * @note Assumes voltage & zero calibration have already been run.
 */
bool hlw8032_calibration_start_current_all(uint8_t channel, float ref_current) {
    if (channel >= 8u) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0xC);
        ERROR_PRINT_CODE(err_code, "%s Invalid channel for current calibration: %u\r\n",
                         HLW8032_TAG, (unsigned)channel);
        return false;
    }

    if (ref_current <= 0.0f) {
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_HLW8032, 0xD);
        ERROR_PRINT_CODE(err_code, "%s Invalid reference current: %.3f\r\n", HLW8032_TAG,
                         ref_current);
        return false;
    }

    if (s_hlw_cal_state.running) {
        WARNING_PRINT("%s Async calibration already in progress\r\n", HLW8032_TAG);
        return false;
    }

    s_hlw_cal_state.mode = HLW_CAL_MODE_CURR_ALL;
    s_hlw_cal_state.running = true;
    s_hlw_cal_state.ref_voltage = 0.0f;
    s_hlw_cal_state.ref_current = ref_current;
    s_hlw_cal_state.current_channel = channel;
    s_hlw_cal_state.total_channels = (uint8_t)(channel + 1u); /* single-channel window */
    s_hlw_cal_state.samples_target = HLW_CAL_SAMPLES_PER_CH;
    s_hlw_cal_state.ok_channels = 0u;
    s_hlw_cal_state.failed_channels = 0u;
    hlw8032_calibration_reset_accumulators();

    INFO_PRINT("%s Async current calibration started for channel %u (Iref=%.3fA)\r\n", HLW8032_TAG,
               (unsigned)channel, ref_current);
    INFO_PRINT("%s Ensure channel %u has the measured load current\r\n", HLW8032_TAG,
               (unsigned)channel);

    return true;
}

/**
 * @brief Query whether an asynchronous calibration sequence is currently running.
 *
 * @return true if a calibration is in progress
 * @return false otherwise
 */
bool hlw8032_calibration_is_running(void) { return s_hlw_cal_state.running; }

bool hlw8032_get_calibration(uint8_t channel, hlw_calib_t *calib) {
    if (channel >= 8 || calib == NULL) {
        return false;
    }
    *calib = channel_calib[channel];
    return (calib->calibrated == 0xCA);
}

void hlw8032_print_calibration(uint8_t channel) {
    if (channel >= 8)
        return;

    const hlw_calib_t *c = &channel_calib[channel];
    log_printf("%s CH%u Calibration:\r\n", HLW8032_TAG, (unsigned)channel);
    log_printf("  VF=%.3f CF=%.6f\r\n", c->voltage_factor, c->current_factor);
    log_printf("  Voff=%.3f Ioff=%.3f\r\n", c->voltage_offset, c->current_offset);
    log_printf("  R1=%.0f R2=%.0f Rshunt=%.6f\r\n", c->r1_actual, c->r2_actual, c->shunt_actual);
    log_printf("  Calibrated=%s ZeroCal=%s\r\n", (c->calibrated == 0xCA) ? "YES" : "NO",
               (c->zero_calibrated == 0xCA) ? "YES" : "NO");
}

/** @} */
/** @} */