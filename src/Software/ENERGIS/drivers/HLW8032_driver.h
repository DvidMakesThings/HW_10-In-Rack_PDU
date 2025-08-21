/* HLW8032_driver.h */

#ifndef HLW8032_DRIVER_H
#define HLW8032_DRIVER_H

#include "../CONFIG.h" // for HLW8032_UART_ID, HLW8032_BAUDRATE, mcp_relay_write_pin
#include <stdbool.h>
#include <stdint.h>

// Hardware Calibration Constants
#define HLW8032_FRAME_LENGTH 24
#define HLW8032_VF 372.0f
#define HLW8032_CF 1.0f
#define UART_READ_TIMEOUT_US 200000
#define MAX_RX_BYTES 64

/**
 * @brief Initialize the HLW8032 interface.
 * Sets up UART (4800 8E1) and configures the MUX enable pin.
 */
void hlw8032_init(void);

/**
 * @brief Read and parse measurement frame for a given channel (0–7).
 * Selects MUX, reads up to MAX_RX_BYTES or until a valid 24-byte frame is found,
 * parses it into internal regs.
 * @param channel Channel index (0..7)
 * @return true if frame read and parsed successfully
 */
bool hlw8032_read(uint8_t channel);

/**
 * @brief Get last parsed voltage/current/power values of the most recent read.
 */
float hlw8032_get_voltage(void);
float hlw8032_get_current(void);
float hlw8032_get_power(void);
float hlw8032_get_power_inspect(void);
float hlw8032_get_power_factor(void);
float hlw8032_get_kwh(void);

/**
 * @brief Uptime bookkeeping helpers.
 */
void     hlw8032_update_uptime(uint8_t ch, bool state);
uint32_t hlw8032_get_uptime(uint8_t ch);

/* =====================  Producer / Consumer API  ====================== */
/**
 * @brief Producer: refresh ALL 8 channels now (blocking).
 *        Should be called by Core0 once every POLL_INTERVAL_MS.
 *        Results are stored into SRAM caches for non-blocking reads.
 */
void hlw8032_refresh_all(void);

/**
 * @brief Consumer-side cached getters (no UART access).
 */
float    hlw8032_cached_voltage(uint8_t ch);
float    hlw8032_cached_current(uint8_t ch);
float    hlw8032_cached_power(uint8_t ch);
uint32_t hlw8032_cached_uptime(uint8_t ch);
bool     hlw8032_cached_state(uint8_t ch);

#endif // HLW8032_DRIVER_H
