/**
 * @file src/tasks/MeterTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks07 7. Meter Task
 * @ingroup tasks
 * @brief Power Metering Task Implementation
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-15
 *
 * @details MeterTask is the sole owner of the HLW8032 power measurement
 * peripheral. It polls all 8 channels in round-robin fashion at 25 Hz,
 * computes rolling averages, tracks uptime, and publishes telemetry data
 * to other tasks via q_meter_telemetry queue.
 *
 * Additionally, MeterTask now owns periodic ADC sampling to provide cached
 * system telemetry values (RP2040 die temperature, VUSB rail, and 12V supply).
 * These values are exposed via a read-only snapshot API so HTTP/SNMP/metrics
 * handlers can render without touching ADC drivers.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef METER_TASK_H
#define METER_TASK_H

#include "../CONFIG.h"

/* =====================  Telemetry Data Structures  ==================== */
/**
 * @brief Telemetry sample for a single output channel.
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

/**
 * @brief Snapshot of system-level ADC telemetry.
 */
typedef struct {
    float die_temp_c;      /**< RP2040 internal sensor temperature [°C] */
    float vusb_volts;      /**< USB rail voltage [V] */
    float vsupply_volts;   /**< 12V supply rail voltage [V] */
    uint16_t raw_temp;     /**< Last raw ADC code for temp sensor */
    uint16_t raw_vusb;     /**< Last raw ADC code for VUSB tap */
    uint16_t raw_vsupply;  /**< Last raw ADC code for 12V tap */
    uint32_t timestamp_ms; /**< Timestamp of snapshot [ms since boot] */
    bool valid;            /**< true if values are fresh */
} system_telemetry_t;

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

/**
 * @brief Get the latest system ADC telemetry snapshot (non-blocking).
 *
 * @details
 * Provides cached values for:
 * - RP2040 die temperature [°C]
 * - USB rail voltage [V]
 * - 12V supply rail voltage [V]
 * plus raw ADC codes and the timestamp. Values are updated periodically
 * inside MeterTask; callers must not access ADC directly.
 *
 * @param sys Pointer to receive the snapshot.
 * @return true if snapshot is valid and fresh, false otherwise.
 */
bool MeterTask_GetSystemTelemetry(system_telemetry_t *sys);

/**
 * @brief Compute single-point temperature calibration (offset only).
 *
 * @param ambient_c True ambient temperature in °C from reference thermometer.
 * @param raw_temp  ADC raw code sampled from the RP2040 die sensor (AINSEL=4).
 * @param out_v0    Output: intercept V0 at 27 °C [V]. Returns current calibrated V0.
 * @param out_slope Output: slope S [V/°C] (positive magnitude). Returns current calibrated slope.
 * @param out_offset Output: computed offset [°C] to align T_raw to ambient (ambient - T_raw).
 * @return true on success, false on invalid arguments.
 *
 * @details
 * Uses the current linear model (V0, S) to compute uncalibrated temperature:
 *   T_raw = 27 - (V - V0) / S , where V = raw_temp * (ADC_VREF / ADC_MAX).
 * Then derives the offset to match ambient:
 *   OFFSET = ambient_c - T_raw.
 * Callers should persist (V0, S, OFFSET) to EEPROM and apply later via
 * MeterTask_SetTempCalibration().
 */
bool MeterTask_TempCalibration_SinglePointCompute(float ambient_c, uint16_t raw_temp, float *out_v0,
                                                  float *out_slope, float *out_offset);

/**
 * @brief Compute two-point temperature calibration (slope + intercept, zero offset).
 *
 * @param t1_c   True temperature point #1 in °C (e.g., ambient).
 * @param raw1   ADC raw code measured at point #1 (AINSEL=4).
 * @param t2_c   True temperature point #2 in °C (e.g., warmed MCU).
 * @param raw2   ADC raw code measured at point #2 (AINSEL=4).
 * @param out_v0 Output: calibrated V0 at 27 °C [V].
 * @param out_slope Output: calibrated slope S [V/°C] (positive magnitude).
 * @param out_offset Output: residual °C offset to add after linear model (set to 0 here).
 * @return true on success (valid inputs and sane results), false otherwise.
 *
 * @details
 * From two known points (T1, V1) and (T2, V2), derive:
 *   V1 = raw1 * (ADC_VREF / ADC_MAX)
 *   V2 = raw2 * (ADC_VREF / ADC_MAX)
 *   S  = (V2 - V1) / (T1 - T2)                [V/°C] (positive magnitude)
 *   V0 = V1 - S * (27.0 - T1)                 [V at 27 °C]
 * Residual offset is set to 0; callers may perform a tiny single-point trim later.
 * Results should be persisted to EEPROM and applied via MeterTask_SetTempCalibration().
 */
bool MeterTask_TempCalibration_TwoPointCompute(float t1_c, uint16_t raw1, float t2_c, uint16_t raw2,
                                               float *out_v0, float *out_slope, float *out_offset);

/**
 * @brief Apply per-device temperature calibration parameters.
 *
 * @param v0_volts_at_27c    Calibrated V0 at 27 °C [V], typical ~0.706.
 * @param slope_volts_per_deg Calibrated slope S [V/°C], typical ~0.001721.
 * @param offset_c           Residual °C offset after linear model (can be 0).
 * @return true if parameters are accepted and applied, false otherwise.
 *
 * @details
 * Validates ranges and updates the internal calibration used by adc_raw_to_die_temp_c().
 * Call at boot after loading values from EEPROM. Callers can read back the active
 * values from their persisted storage (this function does not persist by itself).
 */
bool MeterTask_SetTempCalibration(float v0_volts_at_27c, float slope_volts_per_deg, float offset_c);

/**
 * @brief Query active temperature calibration parameters and inferred mode.
 *
 * @param[out] out_mode   Calibration mode: 0=NONE, 1=1PT (offset only), 2=2PT (slope+intercept)
 * @param[out] out_v0     Intercept V0 at 27 °C [V]
 * @param[out] out_slope  Slope S [V/°C] (positive magnitude)
 * @param[out] out_offset Residual °C offset after linear model
 * @return true always (values are taken from the active MeterTask calibration)
 *
 * @details
 * Mode inference avoids extra dependencies:
 *   - Start with RP2040 typicals: V0=0.706, S=0.001721, OFFSET=0.
 *   - If all three match (within tiny epsilon) → NONE (0).
 *   - If V0 & S match typicals but OFFSET ≠ 0 → 1PT (1).
 *   - Otherwise → 2PT (2).
 */
bool MeterTask_GetTempCalibrationInfo(uint8_t *out_mode, float *out_v0, float *out_slope,
                                      float *out_offset);

#endif /* METER_TASK_H */

/** @} */
