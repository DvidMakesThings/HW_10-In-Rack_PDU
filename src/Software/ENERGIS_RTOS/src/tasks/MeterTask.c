/**
 * @file src/tasks/MeterTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2.3
 * @date 2025-12-08
 *
 * @details
 * MeterTask implementation for power monitoring and system telemetry collection.
 * This task is the exclusive owner of the HLW8032 power measurement peripheral
 * and the RP2040 ADC subsystem. It polls all 8 power channels in round-robin
 * fashion at 25 Hz and samples system ADC telemetry at 5 Hz.
 *
 * Power Measurement Flow:
 * - Polls HLW8032 once per 40ms task period (25 Hz)
 * - HLW8032 driver handles internal round-robin across 8 channels
 * - Retrieves cached instantaneous values (voltage, current, power) per channel
 * - Computes power factor from active power and apparent power
 * - Integrates energy accumulation using power × time (kWh)
 * - Publishes telemetry to queue every 5th sample (~200ms)
 *
 * System Telemetry Flow:
 * - Samples ADC every 200ms for die temperature, VUSB, and 12V supply
 * - Uses oversampling: 16× for voltage rails, 32× for temperature sensor
 * - Applies calibrated temperature model with per-device parameters
 * - Clamps temperature readings to plausible range (-20 to 120 °C)
 * - Logs critical warnings if voltages or temperature out of safe range
 * - Updates cached snapshot accessible via MeterTask_GetSystemTelemetry()
 *
 * Standby Mode Behavior:
 * - Detects power state via Power_GetState() each iteration
 * - In STANDBY: skips all HLW8032 polling and ADC sampling
 * - Maintains reduced heartbeat (500ms) to keep watchdog satisfied
 * - Uses long delay (300ms) to minimize CPU usage during standby
 * - Automatically resumes normal operation on transition back to RUN
 *
 * Energy Accumulation:
 * - Tracks energy in kWh with millisecond-resolution integration
 * - Formula: delta_kWh = (power_watts / 1000) × (delta_ms / 3600000)
 * - Accumulates only when relay is ON and power > 0
 * - Persists during runtime; resets to zero at boot
 *
 * Temperature Calibration:
 * - Supports three modes: NONE (typicals), 1PT (offset), 2PT (slope+intercept)
 * - Default model uses RP2040 typical values (V0=0.706V, S=0.001721V/°C)
 * - Calibration parameters loaded from EEPROM at task init
 * - Applied in adc_raw_to_die_temp_c() for all temperature conversions
 *
 * Version History:
 * - v1.2.3: Removed rolling filter; now uses instantaneous HLW8032 values
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

/**
 * @brief Convert ADC raw code to RP2040 die temperature using per-device calibration.
 *
 * Applies the calibrated linear temperature model to convert a raw 12-bit ADC
 * reading from the RP2040 internal die sensor (AINSEL=4) to temperature in °C.
 *
 * Calibration Model:
 * T[°C] = 27 - (V - V0) / S + OFFSET
 *
 * Where:
 * - V = raw × (ADC_VREF / ADC_MAX)       [voltage from ADC code]
 * - V0 = s_temp_v0_cal                   [intercept at 27 °C, default 0.706V]
 * - S = s_temp_slope_cal                 [slope, default 0.001721 V/°C]
 * - OFFSET = s_temp_offset_c             [residual offset, default 0 °C]
 *
 * @param[in] raw Raw 12-bit ADC code from die temperature sensor.
 *
 * @return Temperature in degrees Celsius.
 */
static float adc_raw_to_die_temp_c(uint16_t raw) {
    const float v = ((float)raw) * (ADC_VREF / (float)ADC_MAX);
    const float t = 27.0f - (v - s_temp_v0_cal) / s_temp_slope_cal;
    return t + s_temp_offset_c;
}

/**
 * @brief Update energy accumulation for a channel.
 *
 * Integrates power over time to compute accumulated energy in kWh. Uses
 * millisecond-resolution timestamps for accurate integration even at short
 * polling intervals. Energy accumulates only when power is positive.
 *
 * Integration Formula:
 * delta_kWh = (power_watts / 1000) × (delta_ms / 3600000)
 *
 * On first call for a channel (last_energy_update_ms == 0), the timestamp
 * is initialized without accumulating energy.
 *
 * @param[in] ch      Channel index 0..7.
 * @param[in] power   Current active power measurement [W].
 * @param[in] now_ms  Current timestamp [ms since boot].
 */
static void update_energy(uint8_t ch, float power, uint32_t now_ms) {
    /* Initialize timestamp on first call */
    if (last_energy_update_ms[ch] == 0) {
        last_energy_update_ms[ch] = now_ms;
        return;
    }

    uint32_t delta_ms = now_ms - last_energy_update_ms[ch];
    /* Accumulate energy only if time has passed and power is positive */
    if (delta_ms > 0 && power > 0.0f) {
        float delta_hours = (float)delta_ms / (1000.0f * 3600.0f);
        energy_accum_kwh[ch] += (power / 1000.0f) * delta_hours;
    }

    last_energy_update_ms[ch] = now_ms;
}

/**
 * @brief Publish telemetry sample to queue (non-blocking).
 *
 * Sends a telemetry sample to the global queue for consumption by NetTask,
 * ConsoleTask, or other interested consumers. Uses non-blocking send with
 * zero timeout; if queue is full, the sample is dropped to prevent blocking
 * the measurement loop.
 *
 * @param[in] telem Pointer to telemetry sample to publish.
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
 *
 * @details
 * In normal RUN mode, polls HLW8032 at 25 Hz, updates energy accumulation,
 * samples ADC telemetry every 200ms, and publishes data to the queue.
 *
 * In STANDBY mode, skips all hardware operations and only feeds heartbeat
 * at a reduced rate with long delays to minimize CPU usage.
 *
 * @param pvParameters Unused.
 * @return None
 */
static void MeterTask_Loop(void *pvParameters) {
    (void)pvParameters;

    const TickType_t pollPeriod = pdMS_TO_TICKS(1000 / METER_POLL_RATE_HZ);
    TickType_t lastWakeTime = xTaskGetTickCount();

    ECHO("%s Task started\r\n", METER_TASK_TAG);

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

        /* Query current power state to determine operation mode */
        power_state_t pwr_state = Power_GetState();

        /* Standby mode: skip all measurements to reduce power consumption */
        if (pwr_state == PWR_STATE_STANDBY) {
            /* Send heartbeat at reduced rate to satisfy watchdog */
            if ((now_ms_ - hb_meter_ms) >= 500U) {
                hb_meter_ms = now_ms_;
                Health_Heartbeat(HEALTH_ID_METER);
            }
            /* Long delay minimizes CPU usage while in standby */
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        /* ===== Normal RUN mode operation from here ===== */

        /* Send periodic heartbeat to health monitor */
        if ((now_ms_ - hb_meter_ms) >= METERTASKBEAT_MS) {
            hb_meter_ms = now_ms_;
            Health_Heartbeat(HEALTH_ID_METER);
        }

        /* Poll HLW8032 for next measurement slice */
        hlw8032_poll_once();

        /* Update overcurrent protection when full 8-channel cycle completes */
        if (hlw8032_cycle_complete()) {
            float total_current = hlw8032_get_total_current();
            (void)Overcurrent_Update(total_current);
        }

        /* Process all channels: retrieve measurements, compute energy, publish telemetry */
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        for (uint8_t ch = 0; ch < 8; ch++) {
            /* Retrieve cached instantaneous measurements from HLW8032 driver */
            float v = hlw8032_cached_voltage(ch);
            float i = hlw8032_cached_current(ch);
            float p = hlw8032_cached_power(ch);
            uint32_t uptime = hlw8032_cached_uptime(ch);
            bool state = hlw8032_cached_state(ch);

            /* Integrate power over time to accumulate energy */
            update_energy(ch, p, now_ms);

            /* Compute power factor from active and apparent power */
            float pf = 0.0f;
            float apparent = v * i;
            if (apparent > 0.01f) {
                pf = p / apparent;
                /* Clamp power factor to valid range [0, 1] */
                if (pf < 0.0f)
                    pf = 0.0f;
                if (pf > 1.0f)
                    pf = 1.0f;
            }

            /* Populate telemetry structure with all measurements */
            meter_telemetry_t telem = {.channel = ch,
                                       .voltage = v,
                                       .current = i,
                                       .power = p,
                                       .power_factor = pf,
                                       .uptime = uptime,
                                       .energy_kwh = energy_accum_kwh[ch],
                                       .relay_state = state,
                                       .timestamp_ms = now_ms,
                                       .valid = true};

            /* Cache telemetry for getter API */
            latest_telemetry[ch] = telem;
            sample_count[ch]++;

            /* Publish to queue every 5th sample to reduce queue traffic */
            if ((sample_count[ch] % 5) == 0) {
                publish_telemetry(&telem);
            }
        }

        /* Sample system ADC telemetry every 200ms for temperature and voltages */
        if ((now_ms - last_adc_ms) >= 200) {
            last_adc_ms = now_ms;

            /* Sample VUSB rail (GPIO26, ADC channel 0) with 16× oversampling */
            adc_select_input(V_USB);
            (void)adc_read(); /* Discard first reading after mux switch */
            (void)adc_read(); /* Discard second reading for ADC stabilization */
            vTaskDelay(pdMS_TO_TICKS(1));
            uint32_t acc_vusb = 0;
            /* Oversample 16× to reduce noise */
            for (int i = 0; i < 16; i++)
                acc_vusb += adc_read();
            uint16_t raw_vusb = (uint16_t)(acc_vusb / 16);
            float v_usb_tap = ((float)raw_vusb) * (ADC_VREF / (float)ADC_MAX);
            float v_usb = v_usb_tap * VBUS_DIVIDER * ADC_TOL;

            /* Log warning if USB supply voltage is critically low */
            if (v_usb < 4.5f) {
#if ERRORLOGGER
                // Leaving as warning. If USB is not connected, it's a nice to have
                // info, but not that critical to log error.
                uint16_t err_code =
                    ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_METERTASK, 0x0);
                WARNING_PRINT_CODE(err_code, "%s CRITICAL: USB supply low: %.2f V\r\n",
                                   METER_TASK_TAG, v_usb);
                // Storage_EnqueueErrorCode(err_code);
#endif
            }

            /* Sample 12V supply rail (GPIO29, ADC channel 3) with 16× oversampling */
            adc_select_input(V_SUPPLY);
            (void)adc_read(); /* Discard first reading after mux switch */
            (void)adc_read(); /* Discard second reading for ADC stabilization */
            vTaskDelay(pdMS_TO_TICKS(1));
            uint32_t acc_12v = 0;
            /* Oversample 16× to reduce noise */
            for (int i = 0; i < 16; i++)
                acc_12v += adc_read();
            uint16_t raw_12v = (uint16_t)(acc_12v / 16);
            float v_12_tap = ((float)raw_12v) * (ADC_VREF / (float)ADC_MAX);
            float v_12v = v_12_tap * SUPPLY_DIVIDER * ADC_TOL;

            /* Log error if 12V supply voltage is critically low */
            if (v_12v < 10.0f) {
#if ERRORLOGGER
                uint16_t err_code =
                    ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_METERTASK, 0x3);
                ERROR_PRINT_CODE(err_code, "%s CRITICAL: 12V supply low: %.2f V\r\n",
                                 METER_TASK_TAG, v_12v);
                Storage_EnqueueErrorCode(err_code);
#endif
            }

            /* Sample die temperature (ADC channel 4) with 32× oversampling */
            adc_set_temp_sensor_enabled(true); /* Force enable sensor each iteration */
            adc_select_input(4);
            (void)adc_read(); /* Discard first reading after mux switch */
            (void)adc_read(); /* Discard second reading for ADC stabilization */
            vTaskDelay(pdMS_TO_TICKS(1));
            uint32_t acc_t = 0;
            /* Oversample 32× for higher precision temperature reading */
            for (int i = 0; i < 32; i++)
                acc_t += adc_read();
            uint16_t raw_temp = (uint16_t)(acc_t / 32);
            float temp_c = adc_raw_to_die_temp_c(raw_temp);

            /* Log error if die temperature exceeds safe operating range */
            if (temp_c > 60.0f) {
#if ERRORLOGGER
                uint16_t err_code =
                    ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_METERTASK, 0x4);
                ERROR_PRINT_CODE(err_code, "%s CRITICAL: Die temperature high: %.2f C\r\n",
                                 METER_TASK_TAG, temp_c);
                Storage_EnqueueErrorCode(err_code);
#endif
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            Health_Heartbeat(HEALTH_ID_METER);
            if (temp_c < 0.0f) {
#if ERRORLOGGER
                uint16_t err_code =
                    ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_METERTASK, 0x5);
                ERROR_PRINT_CODE(err_code, "%s CRITICAL: Die temperature low: %.2f C\r\n",
                                 METER_TASK_TAG, temp_c);
                Storage_EnqueueErrorCode(err_code);
#endif
            }
            Health_Heartbeat(HEALTH_ID_METER);
            vTaskDelay(pdMS_TO_TICKS(5));

            /* Apply plausibility range check to reject obviously corrupt readings */
            if (temp_c >= -20.0f && temp_c <= 120.0f) {
                s_sys.raw_temp = raw_temp;
                s_sys.die_temp_c = temp_c;
            }

            /* Update cached system telemetry snapshot */
            s_sys.raw_vusb = raw_vusb;
            s_sys.vusb_volts = v_usb;
            s_sys.raw_vsupply = raw_12v;
            s_sys.vsupply_volts = v_12v;
            s_sys.timestamp_ms = now_ms;
            s_sys.valid = true;
        }

        /* Delay until next polling period (40ms nominal for 25 Hz rate) */
        vTaskDelayUntil(&lastWakeTime, pollPeriod);
    }
}

/* ##################################################################### */
/*                             Public API                                */
/* ##################################################################### */

/**
 * @brief Create and start the Meter Task with a deterministic enable gate.
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Waits up to 5 seconds for NetTask readiness before proceeding
 * - Creates 16-deep telemetry queue for meter_telemetry_t samples
 * - Initializes HLW8032 driver and overcurrent protection module
 * - Takes ownership of ADC hardware (adc_init, temperature sensor enable)
 * - Attempts to load temperature calibration from EEPROM with mutex protection
 * - Initializes all 8 channel accumulators and system telemetry cache
 * - Spawns task with 512-word stack at METERTASK_PRIORITY
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
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_METERTASK, 0x0);
        ERROR_PRINT_CODE(err_code, "%s Failed to create telemetry queue\r\n", METER_TASK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return pdFAIL;
    }

    /* Initialize HLW8032 driver */
    hlw8032_init();

    /* Initialize overcurrent protection module */
    Overcurrent_Init();

    /* Initialize ADC ownership here (idempotent) */
    adc_init();
    adc_set_temp_sensor_enabled(true);

    /* Try to load and apply per-device temperature calibration from EEPROM. */
    extern SemaphoreHandle_t eepromMtx;
    if (eepromMtx && xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(250)) == pdTRUE) {
        (void)TempCalibration_LoadAndApply(NULL);
        xSemaphoreGive(eepromMtx);
    } else {
        (void)TempCalibration_LoadAndApply(NULL);
    }

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
 * @brief Query meter subsystem readiness status.
 *
 * See metertask.h for full API documentation.
 */
bool Meter_IsReady(void) { return (meterTaskHandle != NULL); }

/**
 * @brief Get latest cached telemetry for a specific channel (non-blocking).
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Returns cached data from latest_telemetry[] array
 * - Validates channel range and pointer before access
 * - Returns valid flag from telemetry structure
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
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Calls hlw8032_refresh_all() to poll all 8 channels synchronously
 * - Updates latest_telemetry[] cache with fresh instantaneous values
 * - Does not update energy accumulation (timestamps not advanced)
 * - Power factor set to 0 in cached data (not recomputed)
 * - May block for up to 40ms during polling
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
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Returns cached snapshot from s_sys static structure
 * - Validates pointer and checks s_sys.valid flag
 * - Logs error code if NULL pointer provided
 */
bool MeterTask_GetSystemTelemetry(system_telemetry_t *sys) {
    if (sys == NULL) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_METERTASK, 0x1);
        ERROR_PRINT_CODE(err_code, "%s NULL pointer in MeterTask_GetSystemTelemetry\r\n",
                         METER_TASK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return false;
    }
    *sys = s_sys;
    return s_sys.valid;
}

/**
 * @brief Compute single-point temperature calibration (offset only).
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Uses current s_temp_v0_cal and s_temp_slope_cal values
 * - Computes uncalibrated temperature from raw ADC code
 * - Derives offset as difference between reference and uncalibrated value
 * - Returns current V0, S, and computed OFFSET for persistence
 * - Logs error code if NULL pointers provided
 */
bool MeterTask_TempCalibration_SinglePointCompute(float ambient_c, uint16_t raw_temp, float *out_v0,
                                                  float *out_slope, float *out_offset) {
    if (!out_v0 || !out_slope || !out_offset) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_METERTASK, 0x2);
        ERROR_PRINT_CODE(err_code, "%s NULL pointer in Single Point Compute\r\n", METER_TASK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
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
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Converts raw ADC codes to voltages
 * - Computes slope from (V2-V1)/(T1-T2) with sign handling
 * - Derives intercept V0 at 27 °C reference point
 * - Validates slope: 0.0005 to 0.005 V/°C
 * - Validates V0: 0.60 to 0.85 V
 * - Sets OFFSET to 0 (linear fit doesn't require additive offset)
 * - Logs warning codes if parameters out of plausible range
 */
bool MeterTask_TempCalibration_TwoPointCompute(float t1_c, uint16_t raw1, float t2_c, uint16_t raw2,
                                               float *out_v0, float *out_slope, float *out_offset) {
    if (!out_v0 || !out_slope || !out_offset) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_ERROR, ERR_FID_METERTASK, 0x6);
        ERROR_PRINT_CODE(err_code, "%s NULL pointer in Two Point Compute\r\n", METER_TASK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
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
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_METERTASK, 0x1);
        WARNING_PRINT_CODE(err_code, "%s Two Point Compute: computed slope out of range\r\n",
                           METER_TASK_TAG);
        Storage_EnqueueWarningCode(err_code);
#endif
        return false;
    }
    if (v0_cal <= 0.60f || v0_cal >= 0.85f) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_METERTASK, 0x2);
        WARNING_PRINT_CODE(err_code, "%s Two Point Compute: computed slope out of range\r\n",
                           METER_TASK_TAG);
        Storage_EnqueueWarningCode(err_code);
#endif
        return false;
    }

    *out_v0 = s_cal > 0 ? v0_cal : s_temp_v0_cal;
    *out_slope = s_cal > 0 ? s_cal : s_temp_slope_cal;
    *out_offset = 0.0f; /* two-point sets the line; do optional single-point trim later */
    return true;
}

/**
 * @brief Apply per-device temperature calibration parameters.
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Validates slope: 0.0005 to 0.005 V/°C
 * - Validates V0: 0.60 to 0.85 V
 * - Updates static calibration variables used by adc_raw_to_die_temp_c()
 * - Does not persist to EEPROM (caller's responsibility)
 * - Logs warning codes if parameters out of range
 */
bool MeterTask_SetTempCalibration(float v0_volts_at_27c, float slope_volts_per_deg,
                                  float offset_c) {
    if (slope_volts_per_deg <= 0.0005f || slope_volts_per_deg >= 0.005f) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_METERTASK, 0x3);
        WARNING_PRINT_CODE(err_code, "%s SetTempCalibration: slope out of range\r\n",
                           METER_TASK_TAG);
        Storage_EnqueueWarningCode(err_code);
#endif
        return false;
    }
    if (v0_volts_at_27c <= 0.60f || v0_volts_at_27c >= 0.85f) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_METER, ERR_SEV_WARNING, ERR_FID_METERTASK, 0x4);
        WARNING_PRINT_CODE(err_code, "%s SetTempCalibration: V0 out of range\r\n", METER_TASK_TAG);
        Storage_EnqueueWarningCode(err_code);
#endif
        return false;
    }

    s_temp_v0_cal = v0_volts_at_27c;
    s_temp_slope_cal = slope_volts_per_deg;
    s_temp_offset_c = offset_c;
    return true;
}

/**
 * @brief Query active temperature calibration parameters and inferred mode.
 *
 * See metertask.h for full API documentation.
 *
 * Implementation notes:
 * - Returns current values from static calibration variables
 * - Infers mode by comparing with RP2040 typical values
 * - Mode 0: All parameters match typicals (V0=0.706, S=0.001721, OFFSET=0)
 * - Mode 1: V0 and S match typicals, but OFFSET differs (1-point calibration)
 * - Mode 2: V0 or S differ from typicals (2-point calibration)
 * - Uses small epsilons for comparison to handle floating-point precision
 * - All output pointers are optional (may be NULL)
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

    /* Tiny epsilons to classify "equal" without noise sensitivity */
    const float EPS_V0 = 0.0005f;     /* 0.5 mV */
    const float EPS_SLOPE = 0.00002f; /* 20 µV/°C */
    const float EPS_OFFSET = 0.005f;  /* 0.005 °C */

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
