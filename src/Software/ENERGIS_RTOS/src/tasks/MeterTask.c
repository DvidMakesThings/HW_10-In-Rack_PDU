/**
 * @file MeterTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks07 7. Meter Task
 * @ingroup tasks
 * @brief Power Metering Task Implementation
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-08
 *
 * @details MeterTask is the sole owner of the HLW8032 power measurement
 * peripheral. It polls all 8 channels in round-robin fashion at 25 Hz,
 * computes rolling averages, tracks uptime, and publishes telemetry data
 * to other tasks via q_meter_telemetry queue.
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

/* ===================================================================== */
/*                          Internal Functions                            */
/* ===================================================================== */

/**
 * @brief Update rolling averages for a channel.
 * @param ch Channel index 0..7
 * @param voltage Instantaneous voltage [V]
 * @param current Instantaneous current [A]
 * @param power Instantaneous power [W]
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
 * @param ch Channel index 0..7
 * @param avg_voltage Pointer to receive average voltage [V]
 * @param avg_current Pointer to receive average current [A]
 * @param avg_power Pointer to receive average power [W]
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
 * @param ch Channel index 0..7
 * @param power Current power reading [W]
 * @param now_ms Current timestamp [ms]
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
 * @param telem Pointer to telemetry sample
 */
static void publish_telemetry(const meter_telemetry_t *telem) {
    /* Send to queue with no wait - drop if full */
    xQueueSend(q_meter_telemetry, telem, 0);
}

/* ===================================================================== */
/*                          Task Implementation                           */
/* ===================================================================== */

/**
 * @brief Main Meter Task loop.
 * @param pvParameters Unused task parameters
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

    static uint32_t hb_meter_ms = 0;

    /* Main polling loop */
    while (1) {
        uint32_t now_ms_ = to_ms_since_boot(get_absolute_time());
        if ((now_ms_ - hb_meter_ms) >= METERTASKBEAT_MS) {
            hb_meter_ms = now_ms_;
            Health_Heartbeat(HEALTH_ID_METER);
        }

        /* Poll next channel in round-robin */
        hlw8032_poll_once();

        /* Get current timestamp */
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        /* Process all channels and publish telemetry */
        for (uint8_t ch = 0; ch < 8; ch++) {
            /* Get cached measurements */
            float v = hlw8032_cached_voltage(ch);
            float i = hlw8032_cached_current(ch);
            float p = hlw8032_cached_power(ch);
            uint32_t uptime = hlw8032_cached_uptime(ch);
            bool state = hlw8032_cached_state(ch);

            /* Update rolling averages */
            update_averages(ch, v, i, p);

            /* Update energy accumulation */
            update_energy(ch, p, now_ms);

            /* Get averaged values */
            float avg_v, avg_i, avg_p;
            get_averages(ch, &avg_v, &avg_i, &avg_p);

            /* Calculate power factor */
            float pf = 0.0f;
            float apparent = avg_v * avg_i;
            if (apparent > 0.01f) {
                pf = avg_p / apparent;
                if (pf < 0.0f)
                    pf = 0.0f;
                if (pf > 1.0f)
                    pf = 1.0f;
            }

            /* Build telemetry sample */
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

            /* Store as latest */
            latest_telemetry[ch] = telem;
            sample_count[ch]++;

            /* Publish to queue (every Nth sample to reduce queue traffic) */
            if ((sample_count[ch] % 5) == 0) {
                publish_telemetry(&telem);
            }
        }

        /* Wait for next poll period */
        vTaskDelayUntil(&lastWakeTime, pollPeriod);
    }
}

/* ===================================================================== */
/*                             Public API                                 */
/* ===================================================================== */

/**
 * @brief Create and start the Meter Task with a deterministic enable gate.
 *
 * @details
 * - Deterministic boot order step 6/6. Waits for Network to be READY.
 * - If @p enable is false, meter is skipped and marked NOT ready.
 * - On success, READY latch is set after task creation.
 *
 * @instructions
 * Call after NetTask_Init(true):
 *   MeterTask_Init(true);
 * Query readiness via Meter_IsReady().
 *
 * @param enable Gate that allows or skips starting this subsystem.
 * @return pdPASS on success, pdFAIL on failure.
 */
BaseType_t MeterTask_Init(bool enable) {
    /* TU-local READY flag accessor (no file-scope globals added). */
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

    /* Original body preserved */
    q_meter_telemetry = xQueueCreate(METER_TELEMETRY_QUEUE_LEN, sizeof(meter_telemetry_t));
    if (q_meter_telemetry == NULL) {
        return pdFAIL;
    }

    hlw8032_init();

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
 *
 * @details
 * READY when the Meter task handle exists (robust against latch desync).
 *
 * @return true if meterTaskHandle is non-NULL, else false.
 */
bool Meter_IsReady(void) { return (meterTaskHandle != NULL); }

/**
 * @brief Get latest telemetry for a specific channel (non-blocking).
 * @param channel Channel index 0..7
 * @param telem Pointer to receive telemetry data
 * @return true if data available, false otherwise
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
 * @note Calls hlw8032_refresh_all() which takes ~2s
 */
void MeterTask_RefreshAll(void) {
    /* Direct call to driver - blocks for ~2 seconds */
    hlw8032_refresh_all();

    /* Update latest telemetry from cached values */
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

/** @} */