
/**
 * @file HLW8032_driver.h
 * @brief HLW8032 Power Measurement Driver Header
 *
 * Provides API for interfacing with the HLW8032 power measurement chip.
 * Includes functions for initialization, reading measurements, uptime tracking,
 * and cached access for multi-core systems.
 *
 * @author DvidMakesThings - David Sipos
 * @date 2025-08-21
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HLW8032_DRIVER_H
#define HLW8032_DRIVER_H

#include "../CONFIG.h" // for HLW8032_UART_ID, HLW8032_BAUDRATE, mcp_relay_write_pin
#include <stdbool.h>
#include <stdint.h>

// Hardware Calibration Constants
#define HLW8032_FRAME_LENGTH 24
#define HLW8032_VF 1.88f
#define HLW8032_CF 1.0f
#define UART_READ_TIMEOUT_US 200000
#define MAX_RX_BYTES 64

/**
 * @brief Initialize the HLW8032 power measurement interface
 *
 * Sets up UART with appropriate baud rate and format (8E1).
 * Configures the multiplexer enable pin.
 */
void hlw8032_init(void);

/**
 * @brief Read and parse a measurement frame for a given channel
 *
 * Selects the specified multiplexer channel, reads data from the HLW8032 chip,
 * and parses the raw values into voltage, current, and power measurements.
 *
 * @param channel Channel index (0-7) to read from
 * @return true if a valid frame was read and parsed successfully, false otherwise
 */
bool hlw8032_read(uint8_t channel);

/**
 * @brief Get the most recently measured voltage
 *
 * @return Voltage in volts (V) from the last successful measurement
 */
float hlw8032_get_voltage(void);

/**
 * @brief Get the most recently measured current
 *
 * @return Current in amperes (A) from the last successful measurement
 */
float hlw8032_get_current(void);

/**
 * @brief Get the most recently measured power
 *
 * @return Power in watts (W) from the last successful measurement
 */
float hlw8032_get_power(void);

/**
 * @brief Get power value calculated from voltage and current measurements
 *
 * @return Power in watts (W) calculated as voltage * current
 */
float hlw8032_get_power_inspect(void);

/**
 * @brief Get the power factor from the most recent measurements
 *
 * @return Power factor (0.0-1.0) calculated as actual power / apparent power
 */
float hlw8032_get_power_factor(void);

/**
 * @brief Get the accumulated energy consumption
 *
 * @return Energy in kilowatt-hours (kWh) based on power measurements and pulse counts
 */
float hlw8032_get_kwh(void);

/**
 * @brief Update the uptime counter for a channel based on its state
 *
 * Tracks the total time a channel has been in the ON state.
 * It accumulates elapsed time when a channel remains ON and resets timing
 * when a channel is turned OFF.
 *
 * @param ch Channel index (0-7) to update
 * @param state Current state of the channel (true = ON, false = OFF)
 */
void hlw8032_update_uptime(uint8_t ch, bool state);

/**
 * @brief Get the accumulated uptime for a channel
 *
 * @param ch Channel index (0-7) to query
 * @return Total uptime in seconds for the specified channel
 */
uint32_t hlw8032_get_uptime(uint8_t ch);

/* =====================  Producer / Consumer API  ====================== */
/**
 * @brief Poll a single channel for power measurements
 *
 * Reads the current state and power measurements for one channel
 * in a round-robin fashion. It updates cached values for the current channel
 * and increments to the next channel for the next call.
 */
void hlw8032_poll_once(void);

/**
 * @brief Refresh power measurements for all channels
 *
 * Reads and caches power measurements for all 8 channels.
 * It is a blocking function that updates voltage, current, power and uptime
 * values for each channel sequentially.
 *
 * Should be called by Core0 once every POLL_INTERVAL_MS.
 * Results are stored into SRAM caches for non-blocking reads.
 */
void hlw8032_refresh_all(void);

/**
 * @brief Get the cached voltage for a channel
 *
 * @param ch Channel index (0-7) to query
 * @return Cached voltage in volts (V) for the specified channel
 */
float hlw8032_cached_voltage(uint8_t ch);

/**
 * @brief Get the cached current for a channel
 *
 * @param ch Channel index (0-7) to query
 * @return Cached current in amperes (A) for the specified channel
 */
float hlw8032_cached_current(uint8_t ch);

/**
 * @brief Get the cached power for a channel
 *
 * @param ch Channel index (0-7) to query
 * @return Cached power in watts (W) for the specified channel
 */
float hlw8032_cached_power(uint8_t ch);

/**
 * @brief Get the cached uptime for a channel
 *
 * @param ch Channel index (0-7) to query
 * @return Cached uptime in seconds for the specified channel
 */
uint32_t hlw8032_cached_uptime(uint8_t ch);

/**
 * @brief Get the cached state for a channel
 *
 * @param ch Channel index (0-7) to query
 * @return Cached state (true = ON, false = OFF) for the specified channel
 */
bool hlw8032_cached_state(uint8_t ch);

#endif // HLW8032_DRIVER_H
