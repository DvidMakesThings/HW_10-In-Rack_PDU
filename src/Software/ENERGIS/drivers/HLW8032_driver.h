/**
 * @file HLW8032_driver.h
 * @author David Sipos (DvidMakesThings)
 * @brief Header file for HLW8032 driver.
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HLW8032_DISPLAY_DRIVER_H
#define HLW8032_DISPLAY_DRIVER_H

#include "../CONFIG.h" // Ensure this includes the MCP23017 register definitions and bus/address info
#include <stdint.h>

#define SAMPLE_RATE (HLW8032_BAUDRATE * 10)     // 10× oversampling
#define SAMPLE_INTERVAL (1000000 / SAMPLE_RATE) // in microseconds
#define SAMPLE_TIME_MS 300
#define SAMPLE_COUNT ((SAMPLE_RATE * SAMPLE_TIME_MS) / 1000)
#define UART_BIT_COUNT 11 // 1 start + 8 data + 1 parity + 1 stop

/**
 * @brief Initializes HLW8032 logic by disabling MUX output initially.
 */
void hlw8032_init(void);

/**
 * @brief Selects an HLW8032 channel via the SN74HC151 mux.
 * @param channel Channel index (0–7).
 * @return true if valid, false if invalid channel.
 */
bool hlw8032_select_channel(uint8_t channel);

/**
 * @brief Reads raw UART data from the selected HLW8032.
 * @param channel The HLW8032 channel.
 * @param buffer Buffer to fill.
 * @param length Number of bytes to read.
 * @return true if successful.
 */
bool hlw8032_read_raw(uint8_t channel, uint8_t *buffer, size_t length);

/**
 * @brief Reads a 24-byte HLW8032 frame from the specified channel.
 * @param channel Channel index.
 * @param frame Pointer to 24-byte buffer.
 * @return true if successful and frame is valid.
 */
bool hlw8032_read_frame(uint8_t channel, uint8_t *frame);

/**
 * @brief Parses HLW8032 frame and extracts voltage, current, and power.
 * @param frame Pointer to 24-byte buffer.
 * @param voltage Pointer to store voltage in volts.
 * @param current Pointer to store current in amps.
 * @param power Pointer to store power in watts.
 * @return true if CRC is valid and values parsed.
 */
bool hlw8032_read_values(uint8_t channel, float *voltage, float *current, float *power);

#endif // HLW8032_DISPLAY_DRIVER_H