/**
 * @file src/drivers/HLW8032_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup driver4 4. HLW8032 Energy Measurement Driver
 * @ingroup drivers
 * @brief HLW8032 Power Measurement Driver Source (RTOS-compatible)
 * @{
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

#ifndef HLW8032_DRIVER_H
#define HLW8032_DRIVER_H

#include "../CONFIG.h"

/* =====================  Hardware Calibration Constants  =============== */
#define HLW8032_FRAME_LENGTH 24
#define HLW8032_VF 1.88f
#define HLW8032_CF 1.0f
#define UART_READ_TIMEOUT_US 200000
#define MAX_RX_BYTES 64

/* =====================  RTOS Mutex  =================================== */
/**
 * @brief UART mutex for HLW8032 access - must be created by MeterTask
 */
extern SemaphoreHandle_t uartHlwMtx;

/* =====================  Driver API  =================================== */
/**
 * @brief Initialize HLW8032 hardware (UART, load calibration).
 *
 * @note Call once during system init, before creating MeterTask.
 * @param None
 * @return None
 */
void hlw8032_init(void);

/**
 * @brief Read and parse a measurement frame for a given channel.
 *
 * @param channel Channel index 0..7
 * @return true if a valid frame was read and parsed successfully
 * @note UART access is mutex-protected; blocks until mutex available
 */
bool hlw8032_read(uint8_t channel);

/**
 * @brief Get last voltage measurement [V].
 *
 * @param None
 * @return Voltage in volts from most recent successful read
 */
float hlw8032_get_voltage(void);

/**
 * @brief Get last current measurement [A].
 *
 * @param None
 * @return Current in amps from most recent successful read
 */
float hlw8032_get_current(void);

/**
 * @brief Get last active power measurement [W].
 *
 * @param None
 * @return Active power in watts from most recent successful read
 */
float hlw8032_get_power(void);

/**
 * @brief Calculate apparent power as V×I [W].
 *
 * @param None
 * @return Apparent power in watts (voltage × current)
 */
float hlw8032_get_power_inspect(void);

/**
 * @brief Update uptime accounting for a channel.
 *
 * @param ch Channel index 0..7
 * @param state true if relay is ON, false if OFF
 * @note Called by MeterTask to track ON-time per channel
 */
void hlw8032_update_uptime(uint8_t ch, bool state);

/**
 * @brief Get accumulated uptime [s] for a channel.
 *
 * @param ch Channel index 0..7
 * @return Uptime in seconds since last power-on
 */
uint32_t hlw8032_get_uptime(uint8_t ch);

/* =====================  Producer / Consumer API  ====================== */
/**
 * @brief Poll one channel (round-robin) and update internal cache.
 *
 * @param None
 * @return None
 *
 * @note Advances to next channel automatically; called by MeterTask loop
 */
void hlw8032_poll_once(void);

/**
 * @brief Refresh all 8 channels sequentially (blocking).
 *
 * @param None
 * @return None
 *
 * @note Use sparingly; takes ~2s to complete all channels
 */
void hlw8032_refresh_all(void);

/**
 * @brief Get cached voltage [V] for a channel.
 *
 * @param ch Channel index 0..7
 * @return Cached voltage reading (clamped 0-400V)
 */
float hlw8032_cached_voltage(uint8_t ch);

/**
 * @brief Get cached current [A] for a channel.
 *
 * @param ch Channel index 0..7
 * @return Cached current reading (clamped 0-100A)
 */
float hlw8032_cached_current(uint8_t ch);

/**
 * @brief Get cached power [W] for a channel.
 *
 * @param ch Channel index 0..7
 * @return Cached power reading (recomputed from V×I, sanity-checked)
 */
float hlw8032_cached_power(uint8_t ch);

/**
 * @brief Get cached uptime [s] for a channel.
 *
 * @param ch Channel index 0..7
 * @return Cached uptime in seconds
 */
uint32_t hlw8032_cached_uptime(uint8_t ch);

/**
 * @brief Get cached relay state for a channel.
 *
 * @param ch Channel index 0..7
 * @return true if relay is ON, false if OFF
 */
bool hlw8032_cached_state(uint8_t ch);

/* =====================  Calibration API  ============================== */
/**
 * @brief Calibrate a specific channel against known reference values.
 *
 * @param channel Channel index 0..7
 * @param ref_voltage Reference voltage in volts (0 for zero-cal only)
 * @param ref_current Reference current in amps (0 for zero-cal only)
 * @return true on success, false on failure
 *
 * @note Zero calibration: pass (0.0, 0.0) to measure and store offsets
 * @note Voltage calibration: apply known voltage and pass actual value
 */
bool hlw8032_calibrate_channel(uint8_t channel, float ref_voltage, float ref_current);

/**
 * @brief Zero-calibrate all 8 channels simultaneously.
 *
 * @return true if all channels succeeded, false on any failure
 * @note Assumes no load connected; measures zero offsets
 */
bool hlw8032_zero_calibrate_all(void);

/**
 * @brief Load calibration data from EEPROM for all channels.
 *
 * @param None
 * @return None
 *
 * @note Called automatically by hlw8032_init()
 */
void hlw8032_load_calibration(void);

/**
 * @brief Get a copy of calibration data for a channel.
 *
 * @param channel Channel index 0..7
 * @param calib Pointer to structure to receive calibration data
 * @return true if channel has valid calibration (0xCA marker)
 */
bool hlw8032_get_calibration(uint8_t channel, hlw_calib_t *calib);

/**
 * @brief Print calibration data for a channel to console.
 *
 * @param channel Channel index 0..7
 * @return None
 *
 * @note Uses ECHO macro; may be disabled in RTOS builds
 */
void hlw8032_print_calibration(uint8_t channel);

#endif /* HLW8032_DRIVER_H */

/** @} */