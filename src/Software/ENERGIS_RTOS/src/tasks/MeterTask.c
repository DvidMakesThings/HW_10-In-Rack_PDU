/**
 * @file src/tasks/MeterTask.c
 * @author
 *
 * @version 1.1.0
 * @date 2025-11-15
 *
 * @details MeterTask is the sole owner of the HLW8032 power measurement
 * peripheral. It polls all 8 channels in round-robin fashion at 25 Hz,
 * computes rolling averages, tracks uptime, and publishes telemetry data
 * to other tasks via q_meter_telemetry queue.
 *
 * This module also performs periodic ADC sampling for system telemetry
 * (die temperature, VUSB rail, and 12V supply) and exposes a cached
 * snapshot via MeterTask_GetSystemTelemetry(). The HTTP/SNMP/metrics
 * layers must read these cached values instead of touching ADC drivers.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define METER_TASK_TAG "[MeterTask] "

/* =====================  Global Queue Handle  ========================== */
QueueHandle_t q_meter_telemetry = NULL;

/* =====================  Task Handle  ================================== */
static TaskHandle_t meterTaskHandle = NULL;

/* =====================  Internal State  =============================== */
static meter_telemetry_t latest_telemetry[8];
static uint32_t sample_count[8] = {0};

/* Rolling average accumulators (1 second window) */
#define AVG_SAMPLES (METER_POLL_RATE_HZ * METER_AVERAGE_WINDOW_MS / 1000)
static float voltage_sum[8] = {0};
static float current_sum[8] = {0};
static float power_sum[8] = {0};
static uint32_t avg_count[8] = {0};

/* Energy accumulation */
static float energy_accum_kwh[8] = {0};
static uint32_t last_energy_update_ms[8] = {0};

/* System ADC telemetry (owned and updated here) */
static system_telemetry_t s_sys = {0};

/**
 * @brief Calibrated temperature model parameters.
 * @details
 * Linear model:
 *   T[°C] = 27 - (V - s_temp_v0_cal) / s_temp_slope_cal + s_temp_offset_c,
 * where V = raw * (ADC_VREF / ADC_MAX).
 * Defaults reflect RP2040 typical values; callers may override at runtime
 * via MeterTask_SetTempCalibration() after loading from EEPROM.
 */
static float s_temp_v0_cal = 0.706f;       /**< @brief Voltage [V] at 27 °C (intercept). */
static float s_temp_slope_cal = 0.001721f; /**< @brief Sensor slope [V/°C] (positive magnitude). */
static float s_temp_offset_c = 0.0f;       /**< @brief Additional °C offset after linear model. */

/* ##################################################################### */
/*                          Internal Functions                           */
/* ##################################################################### */

/* =====================  ADC Helpers (internal)  ======================= */
/**
 * @brief Read voltage from the selected ADC channel with averaging.
 */
static float adc_read_voltage_avg(uint8_t ch) {
    adc_set_clkdiv(96.0f); /* slow down ADC clock */
    adc_select_input(ch);
    (void)adc_read();             /* throwaway sample */
    vTaskDelay(pdMS_TO_TICKS(1)); /* allow S&H to settle ~0.1 ms rounded up */

    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) {
        acc += adc_read();
    }

    float vtap = (acc / 16.0f) * (ADC_VREF / ADC_MAX);
    return vtap * 1; /* apply ADC_TOL if needed */
}

/**
 * @brief Convert ADC raw code to RP2040 die temperature [°C] using per-device calibration.
 */
static float adc_raw_to_die_temp_c(uint16_t raw) {
    const float v = ((float)raw) * (ADC_VREF / (float)ADC_MAX);
    const float t = 27.0f - (v - s_temp_v0_cal) / s_temp_slope_cal;
    return t + s_temp_offset_c;
}

/**
 * @brief Update rolling averages for a channel.
 */
static void update_averages(uint8_t ch, float voltage, float current, float power) {
    voltage_sum[ch] += voltage;
    current_sum[ch] += current;
    power_sum[ch] += power;
    avg_count[ch]++;

    /* Reset accumulator after reaching window size */
    if (avg_count[ch] >= AVG_SAMPLES) {
        avg_count[ch] = 0;
        voltage_sum[ch] = 0.0f;
        current_sum[ch] = 0.0f;
        power_sum[ch] = 0.0f;
    }
}

/**
 * @brief Get averaged values for a channel.
 */
static void get_averages(uint8_t ch, float *avg_voltage, float *avg_current, float *avg_power) {
    if (avg_count[ch] > 0) {
        *avg_voltage = voltage_sum[ch] / avg_count[ch];
        *avg_current = current_sum[ch] / avg_count[ch];
        *avg_power = power_sum[ch] / avg_count[ch];
    } else {
        *avg_voltage = 0.0f;
        *avg_current = 0.0f;
        *avg_power = 0.0f;
    }
}

/**
 * @brief Update energy accumulation for a channel.
 */
static void update_energy(uint8_t ch, float power, uint32_t now_ms) {
    if (last_energy_update_ms[ch] == 0) {
        last_energy_update_ms[ch] = now_ms;
        return;
    }

    uint32_t delta_ms = now_ms - last_energy_update_ms[ch];
    if (delta_ms > 0 && power > 0.0f) {
        /* Energy = Power × Time; kWh = W × hours */
        float delta_hours = (float)delta_ms / (1000.0f * 3600.0f);
        energy_accum_kwh[ch] += (power / 1000.0f) * delta_hours;
    }

    last_energy_update_ms[ch] = now_ms;
}

/**
 * @brief Publish telemetry sample to queue (non-blocking).
 */
static void publish_telemetry(const meter_telemetry_t *telem) {
    /* Send to queue with no wait - drop if full */
    xQueueSend(q_meter_telemetry, telem, 0);
}

/* ===================================================================== */
/*                          Task Implementation                          */
/* ===================================================================== */

/**
 * @brief Main Meter Task loop.
 */
static void MeterTask_Loop(void *pvParameters) {
    (void)pvParameters;

    const TickType_t pollPeriod = pdMS_TO_TICKS(1000 / METER_POLL_RATE_HZ);
    TickType_t lastWakeTime = xTaskGetTickCount();

    /* Initialize energy tracking */
    uint32_t boot_ms = to_ms_since_boot(get_absolute_time());
    for (uint8_t ch = 0; ch < 8; ch++) {
        last_energy_update_ms[ch] = boot_ms;
    }

    /* Heartbeat and ADC sampling cadence */
    static uint32_t hb_meter_ms = 0;
    uint32_t last_adc_ms = boot_ms;

    while (1) {
        uint32_t now_ms_ = to_ms_since_boot(get_absolute_time());
        if ((now_ms_ - hb_meter_ms) >= METERTASKBEAT_MS) {
            hb_meter_ms = now_ms_;
            Health_Heartbeat(HEALTH_ID_METER);
        }

        /* Poll next HLW8032 slice */
        hlw8032_poll_once();

        /* Process channels and publish telemetry */
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        for (uint8_t ch = 0; ch < 8; ch++) {
            float v = hlw8032_cached_voltage(ch);
            float i = hlw8032_cached_current(ch);
            float p = hlw8032_cached_power(ch);
            uint32_t uptime = hlw8032_cached_uptime(ch);
            bool state = hlw8032_cached_state(ch);

            update_averages(ch, v, i, p);
            update_energy(ch, p, now_ms);

            float avg_v, avg_i, avg_p;
            get_averages(ch, &avg_v, &avg_i, &avg_p);

            float pf = 0.0f;
            float apparent = avg_v * avg_i;
            if (apparent > 0.01f) {
                pf = avg_p / apparent;
                if (pf < 0.0f)
                    pf = 0.0f;
                if (pf > 1.0f)
                    pf = 1.0f;
            }

            meter_telemetry_t telem = {.channel = ch,
                                       .voltage = avg_v,
                                       .current = avg_i,
                                       .power = avg_p,
                                       .power_factor = pf,
                                       .uptime = uptime,
                                       .energy_kwh = energy_accum_kwh[ch],
                                       .relay_state = state,
                                       .timestamp_ms = now_ms,
                                       .valid = true};

            latest_telemetry[ch] = telem;
            sample_count[ch]++;

            if ((sample_count[ch] % 5) == 0) {
                publish_telemetry(&telem);
            }
        }

        /* Periodic ADC snapshot (own the ADC here; no other task should touch it) */
        if ((now_ms - last_adc_ms) >= 200) {
            last_adc_ms = now_ms;

            /* --- VUSB (GPIO26, channel 0) --- */
            adc_select_input(V_USB);
            (void)adc_read(); /* throwaway after mux switch */
            (void)adc_read(); /* second throwaway for stability */
            vTaskDelay(pdMS_TO_TICKS(1));
            uint32_t acc_vusb = 0;
            for (int i = 0; i < 16; i++)
                acc_vusb += adc_read();
            uint16_t raw_vusb = (uint16_t)(acc_vusb / 16);
            float v_usb_tap = ((float)raw_vusb) * (ADC_VREF / (float)ADC_MAX);
            float v_usb = v_usb_tap * VBUS_DIVIDER;

            /* --- 12V supply (GPIO29, channel 3) --- */
            adc_select_input(V_SUPPLY);
            (void)adc_read(); /* throwaway */
            (void)adc_read(); /* throwaway */
            vTaskDelay(pdMS_TO_TICKS(1));
            uint32_t acc_12v = 0;
            for (int i = 0; i < 16; i++)
                acc_12v += adc_read();
            uint16_t raw_12v = (uint16_t)(acc_12v / 16);
            float v_12_tap = ((float)raw_12v) * (ADC_VREF / (float)ADC_MAX);
            float v_12v = v_12_tap * SUPPLY_DIVIDER;

            /* --- Die temperature (channel 4) --- */
            adc_set_temp_sensor_enabled(true); /* force-enable each time */
            adc_select_input(4);
            (void)adc_read(); /* throwaway */
            (void)adc_read(); /* throwaway */
            vTaskDelay(pdMS_TO_TICKS(1));
            uint32_t acc_t = 0;
            for (int i = 0; i < 32; i++)
                acc_t += adc_read();
            uint16_t raw_temp = (uint16_t)(acc_t / 32);
            float temp_c = adc_raw_to_die_temp_c(raw_temp);

            /* Plausibility clamp: ignore junk (e.g., if another task stomped ADC) */
            if (temp_c >= -20.0f && temp_c <= 120.0f) {
                s_sys.raw_temp = raw_temp;
                s_sys.die_temp_c = temp_c;
            }

            s_sys.raw_vusb = raw_vusb;
            s_sys.vusb_volts = v_usb;
            s_sys.raw_vsupply = raw_12v;
            s_sys.vsupply_volts = v_12v;
            s_sys.timestamp_ms = now_ms;
            s_sys.valid = true;
        }

        vTaskDelayUntil(&lastWakeTime, pollPeriod);
    }
}

/* ##################################################################### */
/*                             Public API                                */
/* ##################################################################### */
/**
 * @brief Create and start the Meter Task with a deterministic enable gate.
 */
BaseType_t MeterTask_Init(bool enable) {
    static volatile bool ready_val = false;
#define METER_READY() (ready_val)

    METER_READY() = false;

    if (!enable) {
        return pdPASS;
    }

    /* Gate on Network readiness deterministically */
    extern bool Net_IsReady(void);
    TickType_t const t0 = xTaskGetTickCount();
    TickType_t const deadline = t0 + pdMS_TO_TICKS(5000);
    while (!Net_IsReady() && xTaskGetTickCount() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Create queue */
    q_meter_telemetry = xQueueCreate(METER_TELEMETRY_QUEUE_LEN, sizeof(meter_telemetry_t));
    if (q_meter_telemetry == NULL) {
        return pdFAIL;
    }

    /* Initialize HLW8032 driver */
    hlw8032_init();

    /* Initialize ADC ownership here (idempotent) */
    adc_init();
    adc_set_temp_sensor_enabled(true);

    /* Try to load and apply per-device temperature calibration from EEPROM. */
    (void)TempCalibration_LoadAndApply(NULL);

    /* Init per-channel accumulators and telemetry */
    for (uint8_t ch = 0; ch < 8; ch++) {
        latest_telemetry[ch] = (meter_telemetry_t){.channel = ch,
                                                   .voltage = 0.0f,
                                                   .current = 0.0f,
                                                   .power = 0.0f,
                                                   .power_factor = 0.0f,
                                                   .uptime = 0,
                                                   .energy_kwh = 0.0f,
                                                   .relay_state = false,
                                                   .timestamp_ms = 0,
                                                   .valid = false};
        sample_count[ch] = 0;
        avg_count[ch] = 0;
        voltage_sum[ch] = 0.0f;
        current_sum[ch] = 0.0f;
        power_sum[ch] = 0.0f;
        energy_accum_kwh[ch] = 0.0f;
    }

    /* Init system telemetry */
    s_sys = (system_telemetry_t){.die_temp_c = 0.0f,
                                 .vusb_volts = 0.0f,
                                 .vsupply_volts = 0.0f,
                                 .raw_temp = 0,
                                 .raw_vusb = 0,
                                 .raw_vsupply = 0,
                                 .timestamp_ms = 0,
                                 .valid = false};

    INFO_PRINT("%s MeterTask started\r\n", METER_TASK_TAG);

    BaseType_t result = xTaskCreate(MeterTask_Loop, "MeterTask", METER_TASK_STACK_SIZE, NULL,
                                    METERTASK_PRIORITY, &meterTaskHandle);

    if (result == pdPASS) {
        METER_READY() = true;
    }
    return result;
}

/**
 * @brief Meter subsystem readiness query.
 */
bool Meter_IsReady(void) { return (meterTaskHandle != NULL); }

/**
 * @brief Get latest telemetry for a specific channel (non-blocking).
 */
bool MeterTask_GetTelemetry(uint8_t channel, meter_telemetry_t *telem) {
    if (channel >= 8 || telem == NULL) {
        return false;
    }

    *telem = latest_telemetry[channel];
    return latest_telemetry[channel].valid;
}

/**
 * @brief Request immediate refresh of all channels (blocking).
 */
void MeterTask_RefreshAll(void) {
    hlw8032_refresh_all();

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    for (uint8_t ch = 0; ch < 8; ch++) {
        latest_telemetry[ch] = (meter_telemetry_t){.channel = ch,
                                                   .voltage = hlw8032_cached_voltage(ch),
                                                   .current = hlw8032_cached_current(ch),
                                                   .power = hlw8032_cached_power(ch),
                                                   .power_factor = 0.0f,
                                                   .uptime = hlw8032_cached_uptime(ch),
                                                   .energy_kwh = energy_accum_kwh[ch],
                                                   .relay_state = hlw8032_cached_state(ch),
                                                   .timestamp_ms = now_ms,
                                                   .valid = true};
    }
}

/**
 * @brief Get the latest system ADC telemetry snapshot (non-blocking).
 */
bool MeterTask_GetSystemTelemetry(system_telemetry_t *sys) {
    if (sys == NULL) {
        return false;
    }
    *sys = s_sys;
    return s_sys.valid;
}

/**
 * @brief Compute single-point temperature calibration (offset only).
 */
bool MeterTask_TempCalibration_SinglePointCompute(float ambient_c, uint16_t raw_temp, float *out_v0,
                                                  float *out_slope, float *out_offset) {
    if (!out_v0 || !out_slope || !out_offset) {
        return false;
    }

    const float v = ((float)raw_temp) * (ADC_VREF / (float)ADC_MAX);
    const float t_raw = 27.0f - (v - s_temp_v0_cal) / s_temp_slope_cal;

    *out_v0 = s_temp_v0_cal;
    *out_slope = s_temp_slope_cal;
    *out_offset = (ambient_c - t_raw);
    return true;
}

/**
 * @brief Compute two-point temperature calibration (slope + intercept, zero offset).
 */
bool MeterTask_TempCalibration_TwoPointCompute(float t1_c, uint16_t raw1, float t2_c, uint16_t raw2,
                                               float *out_v0, float *out_slope, float *out_offset) {
    if (!out_v0 || !out_slope || !out_offset) {
        return false;
    }
    const float v1 = ((float)raw1) * (ADC_VREF / (float)ADC_MAX);
    const float v2 = ((float)raw2) * (ADC_VREF / (float)ADC_MAX);
    const float dt = (t1_c - t2_c);
    if (dt == 0.0f) {
        return false;
    }

    const float s_cal = (v2 - v1) / dt;               /* V/°C */
    const float v0_cal = v1 - s_cal * (27.0f - t1_c); /* V @ 27 °C */

    /* Sanity ranges to guard bad inputs */
    if (s_cal <= 0.0005f || s_cal >= 0.005f) {
        return false;
    }
    if (v0_cal <= 0.60f || v0_cal >= 0.85f) {
        return false;
    }

    *out_v0 = s_cal > 0 ? v0_cal : s_temp_v0_cal;
    *out_slope = s_cal > 0 ? s_cal : s_temp_slope_cal;
    *out_offset = 0.0f; /* two-point sets the line; do optional single-point trim later */
    return true;
}

/**
 * @brief Apply per-device temperature calibration parameters.
 */
bool MeterTask_SetTempCalibration(float v0_volts_at_27c, float slope_volts_per_deg,
                                  float offset_c) {
    if (slope_volts_per_deg <= 0.0005f || slope_volts_per_deg >= 0.005f) {
        return false;
    }
    if (v0_volts_at_27c <= 0.60f || v0_volts_at_27c >= 0.85f) {
        return false;
    }

    s_temp_v0_cal = v0_volts_at_27c;
    s_temp_slope_cal = slope_volts_per_deg;
    s_temp_offset_c = offset_c;
    return true;
}

/**
 * @brief Query active temperature calibration parameters and inferred mode.
 */
bool MeterTask_GetTempCalibrationInfo(uint8_t *out_mode, float *out_v0, float *out_slope,
                                      float *out_offset) {
    if (out_v0)
        *out_v0 = s_temp_v0_cal;
    if (out_slope)
        *out_slope = s_temp_slope_cal;
    if (out_offset)
        *out_offset = s_temp_offset_c;

    /* RP2040 typicals used by the uncalibrated model */
    const float V0_TYP = 0.706f;
    const float S_TYP = 0.001721f;
    const float OFFSET_TYP = 0.0f;

    /* Tiny epsilons to classify “equal” without noise sensitivity */
    const float EPS_V0 = 0.0005f;     /* 0.5 mV */
    const float EPS_SLOPE = 0.00002f; /* 20 µV/°C */
    const float EPS_OFFSET = 0.05f;   /* 0.05 °C */

    uint8_t mode = 2; /* default: looks like 2-point */

    const bool v0_is_typ = (fabsf(s_temp_v0_cal - V0_TYP) < EPS_V0);
    const bool slope_is_typ = (fabsf(s_temp_slope_cal - S_TYP) < EPS_SLOPE);
    const bool offset_is_typ = (fabsf(s_temp_offset_c - OFFSET_TYP) < EPS_OFFSET);

    if (v0_is_typ && slope_is_typ) {
        mode = offset_is_typ ? 0 : 1; /* NONE or 1PT depending on offset */
    } else {
        mode = 2; /* custom slope and/or V0 -> 2PT */
    }

    if (out_mode)
        *out_mode = mode;
    return true;
}
