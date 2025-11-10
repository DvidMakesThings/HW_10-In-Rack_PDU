/**
 * @file MeterTask.h
 * @author DvidMakesThings - David Sipos
 * @brief Power Metering Task Header
 *
 * @version 1.0.0
 * @date 2025-11-08
 *
 * @details MeterTask is the sole owner of the HLW8032 power measurement
 * peripheral. It polls all 8 channels in round-robin fashion, computes
 * rolling averages, tracks uptime, and publishes telemetry data to other
 * tasks via a message queue. Operates at 20-50 Hz polling rate with 1s
 * averaging window.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef METER_TASK_H
#define METER_TASK_H

#include "../CONFIG.h"

/* =====================  Telemetry Data Structure  ===================== */
/**
 * @brief Telemetry sample for a single channel.
 */
typedef struct {
    uint8_t channel;       /**< Channel index 0..7 */
    float voltage;         /**< RMS voltage [V] */
    float current;         /**< RMS current [A] */
    float power;           /**< Active power [W] */
    float power_factor;    /**< Power factor [0..1] */
    uint32_t uptime;       /**< Accumulated ON-time [s] */
    float energy_kwh;      /**< Accumulated energy [kWh] */
    bool relay_state;      /**< Relay ON (true) or OFF (false) */
    uint32_t timestamp_ms; /**< Timestamp of sample [ms since boot] */
    bool valid;            /**< true if measurement successful */
} meter_telemetry_t;

/* =====================  Task Configuration  =========================== */
#define METER_TASK_STACK_SIZE 512
#define METER_POLL_RATE_HZ 25        /**< Polling rate: 25 Hz (40 ms period) */
#define METER_AVERAGE_WINDOW_MS 1000 /**< Rolling average window: 1 second */
#define METER_TELEMETRY_QUEUE_LEN 16 /**< Telemetry queue depth */

/* =====================  Global Queue Handle  ========================== */
/**
 * @brief Telemetry queue handle for publishing measurements.
 * @note NetTask and ConsoleTask read from this queue
 */
extern QueueHandle_t q_meter_telemetry;

/* =====================  Task API  ===================================== */
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
BaseType_t MeterTask_Init(bool enable);

/**
 * @brief Meter subsystem readiness query.
 *
 * @details
 * READY when the Meter task handle exists (robust against latch desync).
 *
 * @return true if meterTaskHandle is non-NULL, else false.
 */
bool Meter_IsReady(void);

/**
 * @brief Get latest telemetry for a specific channel (non-blocking).
 * @param channel Channel index 0..7
 * @param telem Pointer to receive telemetry data
 * @return true if data available, false if no data or invalid channel
 */
bool MeterTask_GetTelemetry(uint8_t channel, meter_telemetry_t *telem);

/**
 * @brief Request immediate refresh of all channels (blocking).
 * @note Sends notification to MeterTask; blocks until complete
 */
void MeterTask_RefreshAll(void);

#endif /* METER_TASK_H */

/** @} */