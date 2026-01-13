/**
 * @file src/tasks/MeterTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks06 6. Meter Task
 * @ingroup tasks
 * @brief Power metering and system telemetry monitoring task with HLW8032 interface.
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-15
 *
 * @details
 * This module implements a FreeRTOS task for power monitoring and system telemetry
 * collection. It is the exclusive owner of the HLW8032 power measurement peripheral
 * and the RP2040 ADC subsystem, providing centralized measurement data to other tasks.
 *
 * Architecture:
 * - Single-owner model: MeterTask has exclusive access to HLW8032 and ADC hardware
 * - Polls HLW8032 at 25 Hz in round-robin fashion across 8 relay channels
 * - Samples system ADC telemetry at 5 Hz (die temperature, VUSB, 12V supply)
 * - Publishes power measurements via FreeRTOS queue for consumer tasks
 * - Maintains cached telemetry accessible via non-blocking getter APIs
 * - Implements energy accumulation in kWh and relay uptime tracking
 *
 * Key Features:
 * - Per-channel power measurements (voltage, current, active power, power factor)
 * - Energy accumulation with millisecond-resolution integration
 * - Relay uptime tracking per channel
 * - System health monitoring (die temperature, supply voltages)
 * - Standby mode support: pauses measurements to reduce CPU load
 * - Configurable per-device temperature calibration (1-point and 2-point)
 * - Overcurrent protection integration with real-time current summation
 * - Thread-safe telemetry access without blocking measurement loop
 *
 * Measurement Flow:
 * 1. HLW8032 polling: 40ms period, 8 channels × ~5ms each
 * 2. Instantaneous values cached per-channel after each poll slice
 * 3. Energy integration: power × time accumulated in kWh
 * 4. Power factor calculation: P / (V × I) clamped to [0, 1]
 * 5. Telemetry publication: every 5th sample (~200ms) to queue
 * 6. ADC sampling: 200ms period for system telemetry (temp, voltages)
 *
 * Standby Mode Behavior:
 * - When system enters standby via Power_EnterStandby():
 *   - Stops all HLW8032 polling to prevent relay state queries
 *   - Stops ADC sampling to reduce power consumption
 *   - Maintains heartbeat at reduced rate (500ms) for watchdog
 * - On exit from standby via Power_ExitStandby():
 *   - Detects state transition and resumes normal operation
 *
 * Temperature Calibration:
 * - Supports uncalibrated mode (RP2040 typical values)
 * - Single-point calibration: adjusts offset at known ambient temperature
 * - Two-point calibration: derives slope and intercept from two references
 * - Calibration parameters persisted to EEPROM and loaded at boot
 * - API provides compute and apply functions for calibration workflow
 *
 * Usage Pattern:
 * 1. Call MeterTask_Init(true) during system initialization (after NetTask)
 * 2. Query readiness via Meter_IsReady() before accessing telemetry
 * 3. Use MeterTask_GetTelemetry() for per-channel measurements (non-blocking)
 * 4. Use MeterTask_GetSystemTelemetry() for ADC values (non-blocking)
 * 5. Optional: set calibration with MeterTask_SetTempCalibration() after EEPROM load
 * 6. Consumer tasks can read from q_meter_telemetry queue or use getter APIs
 *
 * Integration Points:
 * - HLW8032 driver: exclusive owner, calls hlw8032_poll_once() in loop
 * - Overcurrent protection: updates OCP module with total current each cycle
 * - Power manager: respects standby mode to stop measurements
 * - Health monitor: sends heartbeat at configured interval
 * - EEPROM storage: loads/stores temperature calibration parameters
 *
 * @note MeterTask must be initialized after NetTask due to deterministic sequencing.
 * @note No other task should access ADC or HLW8032 hardware directly.
 * @note Telemetry queue depth is 16 samples; older data discarded if consumers lag.
 * @note Energy accumulation starts from zero at boot and persists during runtime only.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef METER_TASK_H
#define METER_TASK_H

#include "../CONFIG.h"

/* =====================  Telemetry Data Structures  ==================== */
/**
 * @name Telemetry Data Structures
 * @{
 */
/**
 * @struct meter_telemetry_t
 * @brief Telemetry sample for a single output channel.
 * @ingroup tasks06
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
 * @struct system_telemetry_t
 * @brief Snapshot of system-level ADC telemetry.
 * @ingroup tasks06
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
/** @} */

/* =====================  Task Configuration  =========================== */
/**
 * @name Task Configuration
 * @{
 */
#define METER_TASK_STACK_SIZE 512
#define METER_POLL_RATE_HZ 25        /**< Polling rate: 25 Hz (40 ms period) */
#define METER_AVERAGE_WINDOW_MS 1000 /**< Rolling average window: 1 second */
#define METER_TELEMETRY_QUEUE_LEN 16 /**< Telemetry queue depth */
/** @} */

/* =====================  Global Queue Handle  ========================== */
/**
 * @var QueueHandle_t q_meter_telemetry
 * @brief Telemetry queue handle for publishing measurements.
 * @note NetTask and ConsoleTask read from this queue
 * @ingroup tasks06
 */
extern QueueHandle_t q_meter_telemetry;

/* =====================  Task API  ===================================== */
/**
 * @name Task API
 * @{
 */
/**
 * @brief Create and start the Meter Task with a deterministic enable gate.
 * @ingroup tasks06
 *
 * Creates the MeterTask FreeRTOS task, initializes the HLW8032 driver, sets up
 * ADC ownership, configures the telemetry queue, and loads temperature calibration
 * from EEPROM. Implements deterministic boot sequencing by waiting for NetTask
 * readiness before proceeding.
 *
 * Initialization Sequence:
 * 1. Wait for NetTask readiness with 5-second timeout
 * 2. Create telemetry queue (16-deep, meter_telemetry_t entries)
 * 3. Initialize HLW8032 driver for 8-channel power measurement
 * 4. Initialize overcurrent protection module
 * 5. Initialize ADC hardware and enable temperature sensor
 * 6. Load per-device temperature calibration from EEPROM
 * 7. Zero all per-channel energy accumulators and sample counters
 * 8. Initialize system telemetry cache to invalid state
 * 9. Spawn MeterTask with configured priority and 512-word stack
 *
 * @param[in] enable Set true to initialize and start task, false to skip
 *                   initialization deterministically without side effects.
 *
 * @return pdPASS on successful initialization or when skipped (enable=false).
 * @return pdFAIL if initialization fails (queue creation, task creation, or NetTask timeout).
 *
 * @note Call after NetTask_Init(true) in boot sequence (step 7/7).
 * @note Logs error codes on failure via error logger if enabled.
 * @note Idempotent: safe to call multiple times, creates resources only once.
 * @note Task begins polling immediately after creation; no explicit start needed.
 */
BaseType_t MeterTask_Init(bool enable);

/**
 * @brief Query meter subsystem readiness status.
 * @ingroup tasks06
 *
 * Provides a thread-safe method to check whether MeterTask has been created
 * and is operational. Used for deterministic sequencing to ensure metering
 * subsystem is available before accessing telemetry.
 *
 * @return true if MeterTask has been created successfully and is running.
 * @return false if MeterTask_Init() was not called, called with enable=false,
 *         or initialization failed.
 *
 * @note Based on task handle existence, providing immediate readiness indication.
 * @note Safe to call from any task context or early in boot before scheduler starts.
 */
bool Meter_IsReady(void);

/**
 * @brief Get latest cached telemetry for a specific channel (non-blocking).
 * @ingroup tasks06
 *
 * Retrieves the most recent power measurement data for a specific relay channel
 * from the internal cache without triggering a new measurement. Data includes
 * voltage, current, power, power factor, uptime, accumulated energy, relay state,
 * and validity flag.
 *
 * @param[in]  channel Channel index 0..7 corresponding to relay outputs.
 * @param[out] telem   Pointer to meter_telemetry_t structure to receive data.
 *
 * @return true if telemetry data is valid and copied successfully.
 * @return false if channel index out of range, telem pointer is NULL, or
 *         telemetry data has not yet been populated (early in boot).
 *
 * @note Non-blocking: returns immediately with cached data.
 * @note Data may be up to 40ms old (one polling period) in worst case.
 * @note Check telem->valid flag to confirm measurement success.
 * @note Safe to call from any task context.
 */
bool MeterTask_GetTelemetry(uint8_t channel, meter_telemetry_t *telem);

/**
 * @brief Request immediate refresh of all channels (blocking).
 * @ingroup tasks06
 *
 * Forces an immediate poll of all 8 HLW8032 channels and updates the internal
 * telemetry cache. Useful when synchronous measurement is required, such as
 * responding to console commands or generating reports.
 *
 * Operation:
 * 1. Calls hlw8032_refresh_all() to poll all channels sequentially
 * 2. Updates latest_telemetry[] cache with fresh measurements
 * 3. Returns after all channels have been sampled (~40ms blocking time)
 *
 * @note Blocking: may take up to 40ms to complete (8 channels × ~5ms each).
 * @note Does not update energy accumulation; only instantaneous values.
 * @note Power factor is set to 0 in returned data (not recomputed).
 * @note Safe to call from tasks; avoid calling from ISRs or high-priority contexts.
 */
void MeterTask_RefreshAll(void);

/**
 * @brief Get the latest system ADC telemetry snapshot (non-blocking).
 * @ingroup tasks06
 *
 * Retrieves cached system-level telemetry sampled from the RP2040 ADC subsystem.
 * Data includes die temperature, USB rail voltage, 12V supply voltage, raw ADC
 * codes, timestamp, and validity flag. Values are updated periodically by MeterTask
 * at 5 Hz; no other task should access ADC hardware directly.
 *
 * Provided Measurements:
 * - die_temp_c: RP2040 internal temperature sensor reading [°C]
 * - vusb_volts: USB VBUS rail voltage (5V nominal) [V]
 * - vsupply_volts: 12V supply rail voltage (12V nominal) [V]
 * - raw_temp, raw_vusb, raw_vsupply: Raw 12-bit ADC codes for diagnostics
 * - timestamp_ms: Timestamp of last ADC sample [ms since boot]
 * - valid: true if data has been populated at least once
 *
 * @param[out] sys Pointer to system_telemetry_t structure to receive snapshot.
 *
 * @return true if snapshot is valid and has been populated.
 * @return false if sys pointer is NULL or data has not yet been sampled.
 *
 * @note Non-blocking: returns immediately with cached data.
 * @note Data may be up to 200ms old (one ADC sampling period) in worst case.
 * @note Check sys->valid flag to confirm data has been populated.
 * @note Safe to call from any task context.
 * @note Logs error code if NULL pointer provided (when error logger enabled).
 */
bool MeterTask_GetSystemTelemetry(system_telemetry_t *sys);

/**
 * @brief Compute single-point temperature calibration (offset only).
 * @ingroup tasks06
 *
 * Performs a single-point temperature calibration by computing the offset needed
 * to align the RP2040 die sensor reading with a known reference temperature. This
 * method preserves the existing slope and intercept (V0) from the current calibration
 * and only adjusts the additive offset term.
 *
 * Calibration Algorithm:
 * 1. Convert raw ADC code to voltage: V = raw_temp × (ADC_VREF / ADC_MAX)
 * 2. Compute uncalibrated temperature using current model: T_raw = 27 - (V - V0) / S
 * 3. Calculate required offset: OFFSET = ambient_c - T_raw
 * 4. Return current V0, S, and computed OFFSET for persistence
 *
 * Workflow:
 * 1. Measure die sensor at known ambient temperature with reference thermometer
 * 2. Call this function with reference temperature and raw ADC code
 * 3. Persist returned V0, S, OFFSET to EEPROM via StorageTask
 * 4. Apply calibration at next boot via MeterTask_SetTempCalibration()
 *
 * @param[in]  ambient_c  True ambient temperature [°C] from reference thermometer.
 * @param[in]  raw_temp   Raw 12-bit ADC code from die sensor (AINSEL=4).
 * @param[out] out_v0     Current intercept V0 at 27 °C [V] (unchanged by 1-point calibration).
 * @param[out] out_slope  Current slope S [V/°C] (unchanged by 1-point calibration).
 * @param[out] out_offset Computed offset [°C] to align measured temperature to reference.
 *
 * @return true on success with valid outputs.
 * @return false if any output pointer is NULL.
 *
 * @note Single-point calibration corrects for offset errors but not slope errors.
 * @note For better accuracy across temperature range, use two-point calibration.
 * @note Logs error code if NULL pointers provided (when error logger enabled).
 */
bool MeterTask_TempCalibration_SinglePointCompute(float ambient_c, uint16_t raw_temp, float *out_v0,
                                                  float *out_slope, float *out_offset);

/**
 * @brief Compute two-point temperature calibration (slope + intercept, zero offset).
 * @ingroup tasks06
 *
 * Performs a two-point temperature calibration by deriving both slope and intercept
 * from two measured reference points. This method provides superior accuracy across
 * the full temperature range compared to single-point calibration, as it compensates
 * for both offset and slope errors in the die sensor.
 *
 * Calibration Algorithm:
 * 1. Convert raw codes to voltages: V1 = raw1 × (ADC_VREF / ADC_MAX), V2 = raw2 × (ADC_VREF /
 * ADC_MAX)
 * 2. Compute calibrated slope: S = (V2 - V1) / (T1 - T2) [V/°C]
 * 3. Compute calibrated intercept: V0 = V1 - S × (27.0 - T1) [V at 27 °C]
 * 4. Set residual offset to 0 (linear fit eliminates need for additive offset)
 * 5. Validate slope and V0 are within plausible ranges (guards against bad inputs)
 *
 * Workflow:
 * 1. Measure die sensor at known temperature T1 (e.g., room temperature) → raw1
 * 2. Heat or cool MCU to different known temperature T2 (>10°C difference) → raw2
 * 3. Call this function with both reference points
 * 4. Persist returned V0, S, OFFSET to EEPROM via StorageTask
 * 5. Apply calibration at next boot via MeterTask_SetTempCalibration()
 *
 * @param[in]  t1_c        True reference temperature at point 1 [°C].
 * @param[in]  raw1        Raw 12-bit ADC code measured at temperature T1.
 * @param[in]  t2_c        True reference temperature at point 2 [°C].
 * @param[in]  raw2        Raw 12-bit ADC code measured at temperature T2.
 * @param[out] out_v0      Calibrated intercept V0 at 27 °C [V].
 * @param[out] out_slope   Calibrated slope S [V/°C] (positive magnitude).
 * @param[out] out_offset  Residual offset [°C] (always set to 0 for two-point calibration).
 *
 * @return true on success with valid outputs within plausible ranges.
 * @return false if any output pointer is NULL, T1 equals T2 (divide by zero), or
 *         computed parameters fall outside sanity ranges (slope: 0.0005-0.005 V/°C,
 *         V0: 0.60-0.85 V).
 *
 * @note Two-point calibration provides best accuracy across full temperature range.
 * @note Temperature difference should be ≥10°C for stable results.
 * @note Optional: perform single-point trim after two-point calibration for fine tuning.
 * @note Logs warning codes if parameters out of range (when error logger enabled).
 */
bool MeterTask_TempCalibration_TwoPointCompute(float t1_c, uint16_t raw1, float t2_c, uint16_t raw2,
                                               float *out_v0, float *out_slope, float *out_offset);

/**
 * @brief Apply per-device temperature calibration parameters.
 * @ingroup tasks06
 *
 * Applies temperature calibration parameters to the internal die temperature conversion
 * function. Parameters are validated for plausibility before being accepted. This
 * function should be called at boot after loading calibration values from EEPROM.
 *
 * Temperature Model:
 * T[°C] = 27 - (V - v0_volts_at_27c) / slope_volts_per_deg + offset_c
 *
 * Where V = raw_adc_code × (ADC_VREF / ADC_MAX)
 *
 * Validation Ranges:
 * - slope_volts_per_deg: 0.0005 to 0.005 V/°C
 * - v0_volts_at_27c: 0.60 to 0.85 V
 * - offset_c: no range limit (typically -5 to +5 °C)
 *
 * @param[in] v0_volts_at_27c    Calibrated intercept V0 at 27 °C [V], typical ~0.706.
 * @param[in] slope_volts_per_deg Calibrated slope S [V/°C], typical ~0.001721.
 * @param[in] offset_c           Residual additive offset [°C] after linear model (can be 0).
 *
 * @return true if parameters are within valid ranges and applied successfully.
 * @return false if parameters fall outside validation ranges (not applied).
 *
 * @note This function does not persist calibration to EEPROM; caller is responsible.
 * @note Call after loading values from EEPROM during boot sequence.
 * @note Logs warning codes if parameters out of range (when error logger enabled).
 */
bool MeterTask_SetTempCalibration(float v0_volts_at_27c, float slope_volts_per_deg, float offset_c);

/**
 * @brief Query active temperature calibration parameters and inferred mode.
 * @ingroup tasks06
 *
 * Retrieves the currently active temperature calibration parameters and infers
 * the calibration mode based on comparison with RP2040 typical values. Useful
 * for diagnostics, displaying calibration status, and verifying loaded parameters.
 *
 * Mode Inference Logic:
 * - Mode 0 (NONE): All parameters match RP2040 typicals (V0=0.706, S=0.001721, OFFSET=0)
 * - Mode 1 (1PT): V0 and S match typicals, but OFFSET ≠ 0 (offset-only calibration)
 * - Mode 2 (2PT): V0 or S differ from typicals (slope+intercept calibration)
 *
 * Comparison uses small epsilons to account for floating-point precision:
 * - V0 epsilon: 0.5 mV
 * - Slope epsilon: 20 µV/°C
 * - Offset epsilon: 0.005 °C
 *
 * @param[out] out_mode   Calibration mode: 0=NONE, 1=1PT (offset only), 2=2PT (slope+intercept).
 *                        May be NULL if mode information not needed.
 * @param[out] out_v0     Intercept V0 at 27 °C [V]. May be NULL if not needed.
 * @param[out] out_slope  Slope S [V/°C] (positive magnitude). May be NULL if not needed.
 * @param[out] out_offset Residual °C offset after linear model. May be NULL if not needed.
 *
 * @return true always (values are retrieved from active calibration state).
 *
 * @note All output parameters are optional (pass NULL if not needed).
 * @note Mode inference is heuristic-based and does not track calibration history.
 * @note Safe to call from any task context.
 */
bool MeterTask_GetTempCalibrationInfo(uint8_t *out_mode, float *out_v0, float *out_slope,
                                      float *out_offset);
/** @} */

#endif /* METER_TASK_H */

/** @} */
