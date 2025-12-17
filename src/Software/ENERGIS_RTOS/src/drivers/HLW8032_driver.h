/**
 * @file HLW8032_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers04 4. HLW8032 Power Measurement Driver
 * @ingroup drivers
 * @brief RTOS-safe driver for HLW8032 power measurement IC
 * @{
 * @version 1.1.0
 * @date 2025-12-11
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
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HLW8032_DRIVER_H
#define HLW8032_DRIVER_H

#include "../CONFIG.h"

/* =====================  HLW8032 Constants  =============================== */

/** @brief HLW8032 nominal voltage calibration factor (volts) */
#define HLW8032_VF 1.88f

/** @brief HLW8032 nominal current calibration factor (amps) */
#define HLW8032_CF 1.0f

/** @brief Maximum number of bytes to read during frame sync */
#define MAX_RX_BYTES 128

/** @brief UART read timeout in microseconds */
#define HLW_UART_TIMEOUT_US 250000ULL

/** @brief Hardware settling time after MUX change (microseconds) */
#define MUX_SETTLE_US 1000

/* =====================  RTOS Synchronization  ============================ */

/** @brief UART mutex for HLW8032 communication (extern, defined in .c) */
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
 * @brief Start asynchronous current calibration for all channels.
 *
 * @param ref_current Reference current in amps (must be > 0.0f)
 * @return true if calibration sequence successfully started
 * @return false if another calibration is running or ref_current invalid
 */
bool hlw8032_calibration_start_current_all(uint8_t channel, float ref_current);

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

#endif /* HLW8032_DRIVER_H */

/** @} */
/** @} */