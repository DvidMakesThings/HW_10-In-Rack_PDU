/**
 * @file src/drivers/HLW8032_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers04 4. HLW8032 Power Measurement Driver
 * @ingroup drivers
 * @brief RTOS-safe driver for HLW8032 power measurement IC
 * @{
 *
 * @version 12.0.0
 * @date 2026-01-07
 *
 * @details RTOS-safe driver for interfacing with HLW8032 power measurement chips.
 * Features:
 * - Mutex-protected UART access for thread safety
 * - Multiplexed channel selection via MCP23017
 * - Hardware v1.0.0 pin swap compensation
 * - Per-channel calibration with EEPROM persistence
 * - Cached measurements and uptime tracking
 * - Total current sum for overcurrent protection
 * - Queue-based logging (no direct printf)
 *
 * Hardware Configuration:
 * - 8 HLW8032 ICs sharing UART0 RX (4800 baud)
 * - Individual TX lines selected by 74HC4051 multiplexer
 * - Multiplexer control via MCP23017 port B (MUX_A/B/C, MUX_EN)
 * - Frame format: 24 bytes starting with 0x55 0x5A
 *
 * Calibration Overview (Async):
 *
 * Purpose
 * - Provide a safe, non-blocking way to calibrate HLW8032 channels while the system
 *   continues normal operation. Results are persisted per-channel to EEPROM.
 *
 * High-level Flow
 * 1) Start (arm state machine)
 *    - ZERO (0V/0A): zero offsets for V and I across one or all channels
 *      Functions: hlw8032_calibration_start_zero_all(), hlw8032_calibration_start_zero_single(ch)
 *    - VOLT (Vref/0A): voltage gain factor using a known mains reference (current ~0A)
 *      Functions: hlw8032_calibration_start_voltage_all(refV),
 * hlw8032_calibration_start_voltage_single(ch, refV)
 *    - CURR (Iref): current gain factor using a known measured current
 *      Functions:
 *        - hlw8032_calibration_start_current_single(ch, refI): single-channel
 *        - hlw8032_calibration_start_current_all(refI): driver sequences all channels
 *    - Single-channel runs reuse the ALL mode internally but limit the window to
 *      current_channel=ch, total_channels=ch+1.
 *
 * 2) Sampling (cooperative)
 *    - Regular calls to hlw8032_read()/hlw8032_poll_once() feed frames to the engine.
 *    - Only valid frames (checksum/state OK) contribute to accumulated sums:
 *      VolPar/VolData and CurPar/CurData.
 *    - Collection stops per-channel after a fixed target: HLW_CAL_SAMPLES_PER_CH (see .c).
 *
 * 3) Finish per channel (compute + persist)
 *    - ZERO mode: measure offsets at 0V/0A
 *      voltage_offset = (VolPar/VolData) * voltage_factor
 *      current_offset = (CurPar/CurData) * current_factor
 *    - VOLT mode: compute new voltage_factor using reference voltage
 *      voltage_factor_new = (Vref + voltage_offset) * (VolData/VolPar)
 *    - CURR mode: compute new current_factor using reference current
 *      current_factor_new = (Iref + current_offset) * (CurData/CurPar)
 *    - The updated per-channel record (factors, offsets, flags) is written immediately to
 *      EEPROM via EEPROM_WriteSensorCalibrationForChannel(ch, &cal).
 *    - On EEPROM write failure, the channel is marked failed; processing continues.
 *
 * 4) Advance or complete (sequence control)
 *    - For ALL-channel runs: advance to the next channel and repeat sampling.
 *    - For single-channel runs: complete and clear the running flag.
 *    - A summary is logged: total ok vs failed channels.
 *
 * Persistence and Load
 * - Each channel’s calibration is committed to EEPROM at the moment its computation finishes.
 * - On boot, hlw8032_load_calibration() loads all channels into RAM, sanitizes values,
 *   and falls back to nominal defaults if reads fail.
 *
 * Concurrency & Safety
 * - Only one calibration can run at a time; starts will fail if already running.
 * - Sampling is cooperative with normal operation (no busy loops); UART access is mutexed.
 * - Power loss mid-run is safe: completed channels are already persisted; incomplete ones
 *   remain unchanged.
 * - Console "ALL" for current calibration invokes
 *   hlw8032_calibration_start_current_all(refI). The driver sequences channels 0..7
 *   internally and the polling loop pins to the active channel to accelerate
 *   sampling. No parallel calibrations are executed. The console may wait on
 *   `hlw8032_calibration_is_running()` until completion.
 *
 * Constraints & Recommendations
 * - ZERO: ensure all relays OFF (0V/0A) on the target channels.
 * - VOLT: ensure a stable mains reference across the selected channels; keep current ~0A.
 * - CURR: apply a known current on the selected channel and measure with a DMM.
 * - Best accuracy: perform ZERO, then VOLT, then CURR (for the needed channels).
 *
 * Monitoring & Control
 * - Check `hlw8032_calibration_is_running()` to see if a sequence is active.
 * - Progress and per-channel results are logged by the driver.
 * - Console commands provide a user-facing interface:
 *   AUTO_CAL_ZERO [ch|ALL]
 *   AUTO_CAL_V <voltage> [ch|ALL]
 *   AUTO_CAL_I <current> <ch|ALL>
 *   Note: For current calibration with ALL, the console handler blocks between
 *   channels while the async engine runs each channel to completion.
 *
 * Timing
 * - Total time depends on HLW_CAL_SAMPLES_PER_CH and UART throughput (4800 baud) and
 *   the poll cadence. Expect a short per-channel dwell while samples accumulate.
 *
 * Idempotency
 * - Re-running a calibration overwrites that channel’s EEPROM record with the newest values.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HLW8032_DRIVER_H
#define HLW8032_DRIVER_H

#include "../CONFIG.h"

/* =====================  HLW8032 Constants  =============================== */

/** @brief HLW8032 nominal voltage calibration factor (volts) */
/** @name HLW8032 Constants
 *  @ingroup drivers04
 *  @{ */
#define HLW8032_VF 1.88f

/** @brief HLW8032 nominal current calibration factor (amps) */
#define HLW8032_CF 1.0f

/** @brief Maximum number of bytes to read during frame sync */
#define MAX_RX_BYTES 128

/** @brief UART read timeout in microseconds */
#define HLW_UART_TIMEOUT_US 250000ULL

/** @brief Hardware settling time after MUX change (microseconds) */
#define MUX_SETTLE_US 1000
/** @} */

/* =====================  RTOS Synchronization  ============================ */

/** @brief UART mutex for HLW8032 communication (extern, defined in .c) */
/** @var uartHlwMtx
 *  @ingroup drivers04
 */
extern SemaphoreHandle_t uartHlwMtx;

/* =====================  Calibration Structure  =========================== */

/**
 * @brief Per-channel calibration data structure.
 *
 * @details Defined in EEPROM_MemoryMap.h, included here for convenience.
 * Contains voltage/current factors, offsets, resistor values, and flags.
 */
typedef hlw_calib_t hlw_calib_t;

/* =====================  Public API Functions  ============================ */

/** @name Public API
 *  @ingroup drivers04
 *  @{ */

/**
 * @brief Initialize HLW8032 driver subsystem.
 *
 * @details
 * - Creates UART mutex for thread-safe access
 * - Initializes UART0 at 4800 baud
 * - Configures GPIO pins for UART
 * - Loads calibration data from EEPROM
 * - Initializes uptime tracking for all channels
 *
 * @note Must be called during system initialization before any other HLW functions.
 * @note Requires MCP23017 driver to be initialized first.
 */
void hlw8032_init(void);

/**
 * @brief Read power measurements from a specific channel.
 *
 * @details
 * - Selects channel via multiplexer
 * - Reads and validates 24-byte frame
 * - Applies per-channel calibration
 * - Updates internal raw and scaled measurement cache
 * - Thread-safe (uses uartHlwMtx)
 *
 * @param ch Channel index [0..7]
 * @return true if valid frame received and parsed
 * @return false on timeout, invalid checksum, or out-of-range channel
 *
 * @note Blocks for up to HLW_UART_TIMEOUT_US waiting for frame
 * @note Uses vTaskDelay() for cooperative scheduling
 */
bool hlw8032_read(uint8_t ch);

/**
 * @brief Get last voltage reading from previous hlw8032_read().
 *
 * @return float Voltage in volts (calibrated)
 *
 * @note Returns last reading regardless of channel
 * @note Call after hlw8032_read() for corresponding channel
 */
float hlw8032_get_voltage(void);

/**
 * @brief Get last current reading from previous hlw8032_read().
 *
 * @return float Current in amperes (calibrated)
 *
 * @note Returns last reading regardless of channel
 * @note Call after hlw8032_read() for corresponding channel
 */
float hlw8032_get_current(void);

/**
 * @brief Get last power reading from previous hlw8032_read().
 *
 * @return float Active power in watts (calibrated)
 *
 * @note Returns last reading regardless of channel
 * @note Call after hlw8032_read() for corresponding channel
 */
float hlw8032_get_power(void);

/**
 * @brief Get channel uptime in seconds.
 *
 * @details Tracks cumulative ON time based on relay state.
 * Updated by hlw8032_update_uptime().
 *
 * @param ch Channel index [0..7]
 * @return uint32_t Uptime in seconds
 */
uint32_t hlw8032_get_uptime(uint8_t ch);

/**
 * @brief Update channel uptime based on current relay state.
 *
 * @details
 * - If state transitioned from OFF to ON, starts tracking
 * - If state is ON, accumulates time since last call
 * - If state transitioned from ON to OFF, stops accumulation
 *
 * @param ch Channel index [0..7]
 * @param state Current relay state (true=ON, false=OFF)
 *
 * @note Should be called periodically (e.g., by MeterTask)
 * @note Uses absolute_time_t for microsecond precision
 */
void hlw8032_update_uptime(uint8_t ch, bool state);

/**
 * @brief Poll one channel and cache results (round-robin).
 *
 * @details
 * - Queries relay state from MCP driver
 * - Updates uptime for current channel
 * - Reads and caches measurements
 * - Advances to next channel for subsequent call
 * - Updates total current sum after completing channel 7
 * - Thread-safe (internal mutex locking)
 *
 * @note Designed for periodic calling by MeterTask
 * @note Completes full 8-channel cycle in 8 calls
 * @note Total current sum is updated at end of each cycle (after CH7)
 */
void hlw8032_poll_once(void);

/**
 * @brief Read and cache measurements for all 8 channels.
 *
 * @details Sequentially polls all channels and updates cache.
 * Blocking operation (takes ~2 seconds at 4800 baud).
 * Updates total current sum after completing all channels.
 *
 * @note Use sparingly (e.g., on demand or system init)
 * @note Prefer hlw8032_poll_once() for continuous monitoring
 */
void hlw8032_refresh_all(void);

/* =====================  Cached Measurement Accessors  ==================== */

/**
 * @brief Get cached voltage for a channel.
 *
 * @param ch Channel index [0..7]
 * @return float Cached voltage in volts (0.0 if invalid or out of range)
 *
 * @note Returns 0.0 for readings <0V or >400V
 * @note Cache updated by hlw8032_poll_once() or hlw8032_refresh_all()
 */
float hlw8032_cached_voltage(uint8_t ch);

/**
 * @brief Get cached current for a channel.
 *
 * @param ch Channel index [0..7]
 * @return float Cached current in amperes (0.0 if invalid or out of range)
 *
 * @note Returns 0.0 for readings <0A or >100A
 * @note Cache updated by hlw8032_poll_once() or hlw8032_refresh_all()
 */
float hlw8032_cached_current(uint8_t ch);

/**
 * @brief Get cached power for a channel (computed from V*I).
 *
 * @param ch Channel index [0..7]
 * @return float Cached power in watts (0.0 if invalid or out of range)
 *
 * @note Computed as voltage * current from cache
 * @note Returns 0.0 for invalid voltages/currents or result >40kW
 */
float hlw8032_cached_power(uint8_t ch);

/**
 * @brief Get cached uptime for a channel.
 *
 * @param ch Channel index [0..7]
 * @return uint32_t Cached uptime in seconds
 */
uint32_t hlw8032_cached_uptime(uint8_t ch);

/**
 * @brief Get cached relay state for a channel.
 *
 * @param ch Channel index [0..7]
 * @return bool Cached relay state (true=ON, false=OFF)
 */
bool hlw8032_cached_state(uint8_t ch);

/* =====================  Total Current Sum Accessor  ====================== */

/**
 * @brief Get the total current sum across all 8 channels.
 *
 * @details
 * Returns the sum of all cached channel currents, updated at the end of
 * each complete 8-channel measurement cycle. Used by the overcurrent
 * protection system in MeterTask for real-time monitoring.
 *
 * @return float Total current in amperes [0.0 .. 800.0]
 *
 * @note Updated by hlw8032_poll_once() after channel 7 completes
 * @note Updated by hlw8032_refresh_all() after all channels complete
 * @note Thread-safe read (volatile)
 */
float hlw8032_get_total_current(void);

/**
 * @brief Check if a complete measurement cycle has finished since last check.
 *
 * @details
 * Returns true once per complete 8-channel cycle and clears the flag.
 * Used by MeterTask to know when to update overcurrent protection.
 *
 * @return true if a new cycle completed, false otherwise
 *
 * @note Flag is cleared after reading (consume-once semantics)
 */
bool hlw8032_cycle_complete(void);

/* =====================  Calibration Functions  =========================== */

/**
 * @brief Start asynchronous current calibration for a single channel.
 *
 * @details
 * - Measures a known reference current on the specified channel and computes
 *   the current gain factor (keeping offsets as determined by zero-cal).
 * - Runs cooperatively alongside normal HLW polling; persists to EEPROM upon
 *   per-channel completion.
 *
 * Requirements:
 * - Provide a stable, measured current on the target channel (use a DMM).
 * - Zero and voltage calibration should be completed beforehand for best
 *   accuracy.
 *
 * @param channel     Channel index [0..7]
 * @param ref_current Reference current in amps (must be > 0.0f)
 * @return true if calibration sequence successfully started
 * @return false if another calibration is running or parameters invalid
 */
bool hlw8032_calibration_start_current_single(uint8_t channel, float ref_current);

/**
 * @brief Start asynchronous current calibration for all channels.
 *
 * @details
 * - Runs a non-blocking calibration sequence across channels 0..7 using the
 *   provided reference current. The engine advances channel-by-channel and
 *   persists each result.
 * - Use `hlw8032_calibration_is_running()` to monitor progress.
 *
 * Requirements:
 * - Provide the same known current when prompted for each channel.
 * - Zero and voltage calibration should be completed beforehand for best accuracy.
 *
 * @param ref_current Reference current in amps (must be > 0.0f)
 * @return true if calibration sequence successfully started
 * @return false if another calibration is running or ref_current invalid
 */
bool hlw8032_calibration_start_current_all(float ref_current);

/**
 * @brief Load calibration data from EEPROM for all channels.
 *
 * @details
 * - Reads calibration records via EEPROM_ReadSensorCalibrationForChannel()
 * - Sanitizes loaded values (replaces invalid with defaults)
 * - Falls back to nominal values if EEPROM read fails
 *
 * @note Called automatically by hlw8032_init()
 * @note Can be called manually to reload after EEPROM updates
 */
void hlw8032_load_calibration(void);

/**
 * @brief Start asynchronous zero calibration (0V/0A) for all channels.
 *
 * @details
 * - Runs a non-blocking calibration sequence driven by hlw8032_poll_once()
 * - Collects a fixed number of samples per channel using normal HLW polling
 * - For each channel, computes voltage offset at 0V and stores to EEPROM
 *
 * Requirements:
 * - All relay channels must be OFF and unloaded (0V, 0A) during the sequence
 *
 * @return true if calibration sequence successfully started
 * @return false if another calibration is already running or parameters invalid
 *
 * @note Does not block; progress is logged asynchronously.
 */
bool hlw8032_calibration_start_zero_all(void);

/**
 * @brief Start asynchronous zero calibration (0V/0A) for a single channel.
 *
 * @details
 * Reuses the zero-calibration engine used for all channels but restricts the
 * calibration window to the specified channel only. Sampling and processing
 * are driven by the normal HLW polling loop.
 *
 * Requirements:
 * - The selected channel must be OFF and unloaded (0V/0A)
 * - No other calibration may be running concurrently
 *
 * @param channel Channel index [0..7]
 * @return true if the sequence was successfully started
 * @return false if busy or parameters invalid
 */
bool hlw8032_calibration_start_zero_single(uint8_t channel);

/**
 * @brief Start asynchronous voltage calibration for all channels.
 *
 * @details
 * - Runs a non-blocking calibration sequence driven by hlw8032_poll_once()
 * - Uses a single reference voltage for all channels (e.g. 230V)
 * - For each channel, computes new voltage gain factor and stores to EEPROM
 *
 * Requirements:
 * - All channels must see the same stable reference mains voltage
 * - Current calibration is still not implemented (assumes 0A)
 *
 * @param ref_voltage Reference voltage in volts (must be > 0.0f)
 *
 * @return true if calibration sequence successfully started
 * @return false if another calibration is already running or ref_voltage invalid
 *
 * @note Does not block; progress is logged asynchronously.
 */
bool hlw8032_calibration_start_voltage_all(float ref_voltage);

/**
 * @brief Start asynchronous voltage calibration for a single channel.
 *
 * @details
 * Reuses the same engine as the all-channels voltage calibration but restricts
 * the calibration window to the specified channel only.
 *
 * Requirements:
 * - The selected channel must see the stable reference mains voltage
 * - Current should be 0A during voltage calibration
 *
 * @param channel     Channel index [0..7]
 * @param ref_voltage Reference voltage in volts (must be > 0.0f)
 * @return true if calibration sequence successfully started
 * @return false if another calibration is running or parameters invalid
 */
bool hlw8032_calibration_start_voltage_single(uint8_t channel, float ref_voltage);

/**
 * @brief Query whether an asynchronous calibration sequence is currently running.
 *
 * @return true if a zero or voltage calibration is in progress
 * @return false otherwise
 */
bool hlw8032_calibration_is_running(void);

/**
 * @brief Retrieve calibration data for a channel.
 *
 * @param channel Channel index [0..7]
 * @param calib Pointer to destination structure
 * @return true if channel is calibrated (calib->calibrated == 0xCA)
 * @return false if invalid channel, null pointer, or not calibrated
 */
bool hlw8032_get_calibration(uint8_t channel, hlw_calib_t *calib);

/**
 * @brief Print calibration data for a channel to log.
 *
 * @param channel Channel index [0..7]
 */
void hlw8032_print_calibration(uint8_t channel);

/**
 * @brief Diagnostic: Dump all cached channel values to log.
 *
 * @note Useful for debugging cache corruption or channel cross-contamination issues
 */
void hlw8032_dump_cache(void);
/** @} */

#endif /* HLW8032_DRIVER_H */

/** @} */