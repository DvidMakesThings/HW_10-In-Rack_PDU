/**
 * @file HLW8032_driver.h
 * @brief HLW8032 Power Measurement Driver Header
 *
 * Provides API for interfacing with the HLW8032 power measurement chip.
 * Includes functions for initialization, reading measurements, uptime tracking,
 * cached access for multi-core systems, and EEPROM-backed calibration.
 *
 * @author Dvid
 * @date 2025-08-21
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HLW8032_DRIVER_H
#define HLW8032_DRIVER_H

#include "../CONFIG.h" // for HLW8032_UART_ID, HLW8032_BAUDRATE, mcp_relay_write_pin
#include "../utils/EEPROM_MemoryMap.h"
#include <stdbool.h>
#include <stdint.h>

/* =====================  Hardware Calibration Constants  =============== */
#define HLW8032_FRAME_LENGTH 24
#define HLW8032_VF 1.88f
#define HLW8032_CF 1.0f
#define UART_READ_TIMEOUT_US 200000
#define MAX_RX_BYTES 64

/* =====================  Driver API  =================================== */
/**
 * @brief Initialize HLW8032 and load calibration from EEPROM.
 */
void hlw8032_init(void);

/**
 * @brief Read and parse a measurement frame for a given channel.
 * @param channel 0..7
 * @return true if a valid frame was read and parsed successfully.
 */
bool hlw8032_read(uint8_t channel);

/**
 * @brief Get last voltage [V].
 */
float hlw8032_get_voltage(void);

/**
 * @brief Get last current [A].
 */
float hlw8032_get_current(void);

/**
 * @brief Get last power [W].
 */
float hlw8032_get_power(void);

/**
 * @brief Inspect power as V*I [W].
 */
float hlw8032_get_power_inspect(void);

/**
 * @brief Get power factor [0..1].
 */
float hlw8032_get_power_factor(void);

/**
 * @brief Get accumulated energy [kWh].
 */
float hlw8032_get_kwh(void);

/**
 * @brief Update uptime accounting for a channel.
 * @param ch 0..7
 * @param state true=ON
 */
void hlw8032_update_uptime(uint8_t ch, bool state);

/**
 * @brief Get uptime [s] for a channel.
 * @param ch 0..7
 */
uint32_t hlw8032_get_uptime(uint8_t ch);

/* =====================  Producer / Consumer API  ====================== */
/**
 * @brief Poll one channel (round-robin) and update caches.
 */
void hlw8032_poll_once(void);

/**
 * @brief Refresh all 8 channels (blocking).
 */
void hlw8032_refresh_all(void);

/**
 * @brief Get cached voltage [V].
 * @param ch 0..7
 */
float hlw8032_cached_voltage(uint8_t ch);

/**
 * @brief Get cached current [A].
 * @param ch 0..7
 */
float hlw8032_cached_current(uint8_t ch);

/**
 * @brief Get cached power [W].
 * @param ch 0..7
 */
float hlw8032_cached_power(uint8_t ch);

/**
 * @brief Get cached uptime [s].
 * @param ch 0..7
 */
uint32_t hlw8032_cached_uptime(uint8_t ch);

/**
 * @brief Get cached relay state.
 * @param ch 0..7
 * @return true if ON
 */
bool hlw8032_cached_state(uint8_t ch);

/* =====================  Calibration API  ============================== */
/**
 * @brief Calibrate a specific channel. Pass 0/0 for zero-cal only.
 * @param channel 0..7
 * @param ref_voltage volts
 * @param ref_current amps
 * @return true on success
 */
bool hlw8032_calibrate_channel(uint8_t channel, float ref_voltage, float ref_current);

/**
 * @brief Load calibration blob from generic Sensor Calibration region.
 */
void hlw8032_load_calibration(void);

/**
 * @brief Get a copy of calibration for a channel.
 * @param channel 0..7
 * @param calib out
 * @return true if channel calibrated (0xCA)
 */
bool hlw8032_get_calibration(uint8_t channel, hlw_calib_t *calib);

/**
 * @brief Print calibration data for a channel.
 * @param channel 0..7
 */
void hlw8032_print_calibration(uint8_t channel);

#endif /* HLW8032_DRIVER_H */
