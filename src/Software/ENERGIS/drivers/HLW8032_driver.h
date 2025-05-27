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
 * @param channel Channel index (0 = voltage/power, 1 = current/pulse)
 * @return true if frame read and parsed successfully
 */
bool hlw8032_read(uint8_t channel);

/**
 * @brief Get last parsed voltage reading (in volts).
 */
float hlw8032_get_voltage(void);

/**
 * @brief Get last parsed current reading (in amps).
 */
float hlw8032_get_current(void);

/**
 * @brief Get last parsed active power (in watts).
 */
float hlw8032_get_power(void);

/**
 * @brief Get estimated power (V × I).
 */
float hlw8032_get_power_inspect(void);

/**
 * @brief Get estimated power factor (Active / Apparent).
 */
float hlw8032_get_power_factor(void);

/**
 * @brief Get estimated energy in kilowatt-hours.
 */
float hlw8032_get_kwh(void);

#endif // HLW8032_DRIVER_H
