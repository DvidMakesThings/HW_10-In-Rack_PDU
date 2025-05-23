/**
 * @file HLW8032_driver.c
 * @author David Sipos
 * @brief Implementation of HLW8032 UART reading via SN74HC151 mux
 * @version 1.1
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "HLW8032_driver.h"
#include "../CONFIG.h"

/**
 * @brief Initializes HLW8032 logic by disabling MUX output initially.
 */
void hlw8032_init(void) {
    uart_init(HLW8032_UART_ID, HLW8032_BAUDRATE);
    uart_set_format(HLW8032_UART_ID, 8, 1, UART_PARITY_EVEN); // 8E1
    mcp_relay_write_pin(MUX_ENABLE, 0);
}

/**
 * @brief Selects an HLW8032 channel via the SN74HC151 mux.
 * @param channel Channel index (0–7).
 * @return true if valid, false if invalid channel.
 */
bool hlw8032_select_channel(uint8_t channel) {
    if (channel > 7) {
        INFO_PRINT("Invalid HLW8032 channel: %u\n", channel);
        return false;
    }

    DEBUG_PRINT("\tSelecting HLW8032 channel %u\n", channel);
    mcp_relay_write_pin(MUX_SELECT_A, 0);
    mcp_relay_write_pin(MUX_SELECT_B, 0);
    mcp_relay_write_pin(MUX_SELECT_C, 0);

    mcp_relay_write_pin(MUX_SELECT_A, (channel >> 0) & 0x01);
    mcp_relay_write_pin(MUX_SELECT_B, (channel >> 1) & 0x01);
    mcp_relay_write_pin(MUX_SELECT_C, (channel >> 2) & 0x01);

    sleep_us(500);
    DEBUG_PRINT("\tMUX configured for channel %u\n", channel);
    return true;
}

/**
 * @brief Reads raw UART data from the selected HLW8032.
 */
bool hlw8032_read_raw(uint8_t channel, uint8_t *buffer, size_t length) {
    if (!hlw8032_select_channel(channel))
        return false;

    // Clear out old data
    while (uart_is_readable(HLW8032_UART_ID))
        uart_getc(HLW8032_UART_ID);

    DEBUG_PRINT("\tWaiting for HLW8032 frame header...\n");

    // Sync timeout
    absolute_time_t sync_timeout = make_timeout_time_ms(300);
    uint8_t sync_buf[3] = {0};

    // Wait for 0x5A 0x5A 0x5A
    while (true) {

        if (!uart_is_readable(HLW8032_UART_ID))
            continue;

        sync_buf[0] = sync_buf[1];
        sync_buf[1] = sync_buf[2];
        sync_buf[2] = uart_getc(HLW8032_UART_ID);

        if (sync_buf[0] == 0x5A && sync_buf[1] == 0x5A && sync_buf[2] == 0x5A)
            break;
    }

    DEBUG_PRINT("\tHeader detected, reading frame...\n");
    sleep_ms(2); // Small delay to ensure full frame is available

    buffer[0] = sync_buf[0];
    buffer[1] = sync_buf[1];
    buffer[2] = sync_buf[2];

    size_t received = 3;
    absolute_time_t byte_timeout = make_timeout_time_ms(300);

    while (received < length) {
        if (uart_is_readable(HLW8032_UART_ID)) {
            buffer[received++] = uart_getc(HLW8032_UART_ID);
        } else if (absolute_time_diff_us(get_absolute_time(), byte_timeout) < 0) {
            break;
        }
    }

    DEBUG_PRINT("\tReceived %u/%u bytes\n", received, length);

    return received == length;
}

/**
 * @brief Reads voltage, current, and power from an HLW8032 channel.
 * @param channel Channel index (0–7).
 * @param voltage Pointer to store the voltage value.
 * @param current Pointer to store the current value.
 * @param power Pointer to store the power value.
 * */
bool hlw8032_read_values(uint8_t channel, float *voltage, float *current, float *power) {
    INFO_PRINT("Reading HLW8032 channel %u\n", channel);
    uint8_t frame[HLW8032_FRAME_LENGTH + 10]; // margin to slide window if needed

    if (!hlw8032_read_raw(channel, frame, HLW8032_FRAME_LENGTH)) {
        INFO_PRINT("Failed to read raw HLW8032 frame.\n");
        return false;
    }

    // Dump raw data for debug
    DEBUG_PRINT("\tRaw HLW8032 frame: ");
    for (int i = 0; i < HLW8032_FRAME_LENGTH; i++)
        DEBUG_PRINT("%02X ", frame[i]);
    DEBUG_PRINT("\n");

    // Look for valid frame header (triple 0x5A) and enough space left
    int start = -1;
    for (int i = 0; i <= HLW8032_FRAME_LENGTH - 3; i++) {
        if (frame[i] == 0x5A && frame[i + 1] == 0x5A && frame[i + 2] == 0x5A) {
            start = i;
            break;
        }
    }

    if (start < 0 || (start + 24) > HLW8032_FRAME_LENGTH) {
        INFO_PRINT("Frame header not found or incomplete frame.\n");
        return false;
    }

    const uint8_t *aligned = &frame[start];

    // Check State REG early for corruption (frame[3])
    if (aligned[3] == 0xAA || (aligned[3] & 0x0F) != 0x00) {
        INFO_PRINT("State REG indicates overflow or unusable parameters (0x%02X)\n", aligned[3]);
        return false;
    }

    // Checksum = sum of [2..22] must equal frame[23]
    uint8_t crc = 0;
    for (int i = 2; i <= 22; i++)
        crc += aligned[i];

    if (crc != aligned[23]) {
        INFO_PRINT("Checksum mismatch: expected 0x%02X, got 0x%02X\n", crc, aligned[23]);
        return false;
    }

    uint32_t v_raw = (aligned[6] << 16) | (aligned[7] << 8) | aligned[8];
    uint32_t c_raw = (aligned[12] << 16) | (aligned[13] << 8) | aligned[14];
    uint32_t p_raw = (aligned[18] << 16) | (aligned[19] << 8) | aligned[20];

    const float V_COEFF = 1.0f / 65536.0f;
    const float C_COEFF = 1.0f / 65536.0f;
    const float P_COEFF = 1.0f / 65536.0f;

    *voltage = v_raw * V_COEFF;
    *current = c_raw * C_COEFF;
    *power = p_raw * P_COEFF;

    INFO_PRINT("Voltage: %.3f V\n", *voltage);
    INFO_PRINT("Current: %.3f A\n", *current);
    INFO_PRINT("Power  : %.3f W\n", *power);

    return true;
}
