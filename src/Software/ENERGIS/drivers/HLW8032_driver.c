/**
 * @file HLW8032_driver.c
 * @author DvidMakesThings - David Sipos
 * @brief Driver for HLW8032 energy measurement IC.
 * @version 1.0
 * @date 2025-05-24
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "HLW8032_driver.h"
#include "../CONFIG.h"

static uint8_t frame[HLW8032_FRAME_LENGTH];
static uint32_t VolPar, VolData;
static uint32_t CurPar, CurData;
static uint32_t PowPar, PowData;
static uint16_t PF;
static uint32_t PF_Count = 1;
static uint8_t rx_buf[MAX_RX_BYTES];

static float last_voltage = 0, last_current = 0, last_power = 0;

/**
 * @brief Check if the received frame checksum is valid.
 * This function calculates the checksum of the received frame
 * @return true if checksum is valid, false otherwise
 */
static bool checksum_ok(void) {
    uint8_t check = 0;
    for (uint8_t a = 2; a <= 22; a++) {
        check = check + frame[a];
    }
    DEBUG_PRINT("Checksum calc = %02X, expected = %02X\n", check, frame[23]);
    if (check == frame[23]) {

        return true;
    } else {
        return false;
    }
}

/**
 * @brief Select the MUX channel for HLW8032.
 * This function sets the MUX select pins to choose the desired channel.
 * @param ch Channel index (0–7)
 */
static void mux_select(uint8_t ch) {
    // Set MUX pins to 0 by default
    mcp_relay_write_pin(MUX_SELECT_A, 0);
    mcp_relay_write_pin(MUX_SELECT_B, 0);
    mcp_relay_write_pin(MUX_SELECT_C, 0);
    sleep_us(500);

    // Set MUX pins based on channel
    mcp_relay_write_pin(MUX_SELECT_A, (ch >> 0) & 1);
    mcp_relay_write_pin(MUX_SELECT_B, (ch >> 1) & 1);
    mcp_relay_write_pin(MUX_SELECT_C, (ch >> 2) & 1);
    sleep_us(500);
}

/**
 * @brief Flush the HLW8032 UART RX buffer.
 * This function clears any unread data in the UART RX buffer.
 */
static void hlw8032_flush_rx(void) {
    while (uart_is_readable(HLW8032_UART_ID))
        (void)uart_getc(HLW8032_UART_ID);
}

/**
 * @brief Initialize the HLW8032 interface.
 * This function sets up the UART for HLW8032 communication and configures the MUX.
 */
void hlw8032_init(void) {
    uart_init(HLW8032_UART_ID, HLW8032_BAUDRATE);
    uart_set_format(HLW8032_UART_ID, 8, 1, UART_PARITY_EVEN);
    mcp_relay_write_pin(MUX_ENABLE, 0); // active low
}

/** * @brief Read and parse measurement frame for a given channel (0–7).
 * This function reads the HLW8032 data for the specified channel and parses the frame.
 * It reads 64 bytes maximum or until a valid frame is found.
 * If a valid frame is found, it extracts the voltage, current, power, and power factor values.
 * @param channel Channel index (0–7)
 * @return true if frame read and parsed successfully, false otherwise
 */
bool hlw8032_read(uint8_t channel) {
    mux_select(channel);

    uint8_t buffer[MAX_RX_BYTES];
    int count = 0;

    // Read for ~200ms max or until buffer full
    absolute_time_t start = get_absolute_time();
    while (absolute_time_diff_us(get_absolute_time(), start) < 200000 && count < sizeof(buffer)) {
        if (uart_is_readable(HLW8032_UART_ID))
            buffer[count++] = uart_getc(HLW8032_UART_ID);
        else
            sleep_us(100);
    }

    if (count < 24) {
        ERROR_PRINT("CH%u: Not enough data received\n", channel + 1);
        return false;
    }

    if (DEBUG) {
        DEBUG_PRINT("\tCH%u RX BUFFER (%d bytes):\n\t", channel + 1, count);
        for (int i = 0; i < count; i++) {
            printf("%02X ", buffer[i]);
            if ((i + 1) % 16 == 0)
                printf("\n\t");
        }
        if (count % 16 != 0)
            printf("\n");
        printf("\n");
    }

    // Search for 0x5A 0x02/0x03 pattern, indicating start of frame (offset 1)
    for (int i = 1; i <= count - 23; i++) {
        if (buffer[i] == 0x5A && (buffer[i + 1] == 0x02 || buffer[i + 1] == 0x03)) {
            frame[0] = 0x55;
            memcpy(&frame[1], &buffer[i], HLW8032_FRAME_LENGTH - 1);

            if (INFO) {
                INFO_PRINT("CH%u RAW FRAME:\t", channel + 1);
                for (int j = 0; j < HLW8032_FRAME_LENGTH; j++)
                    printf("%02X ", frame[j]);
                printf("\n");
            }

            if (!checksum_ok()) {
                ERROR_PRINT("CH%u: Bad checksum\n", channel + 1);
                return false;
            }

            // Parse
            uint8_t state = frame[2];
            VolPar = (frame[3] << 16) | (frame[4] << 8) | frame[5];
            VolData = (state & (1 << 6)) ? (frame[6] << 16) | (frame[7] << 8) | frame[8] : 0;

            CurPar = (frame[9] << 16) | (frame[10] << 8) | frame[11];
            CurData = (state & (1 << 5)) ? (frame[12] << 16) | (frame[13] << 8) | frame[14] : 0;

            PowPar = (frame[15] << 16) | (frame[16] << 8) | frame[17];
            PowData = (state & (1 << 4)) ? (frame[18] << 16) | (frame[19] << 8) | frame[20] : 0;

            PF = (frame[21] << 8) | frame[22];
            if (state & (1 << 7))
                PF_Count++;

            last_voltage = (VolData > 0) ? ((float)VolPar / VolData) * HLW8032_VF : 0.0f;
            last_current = (CurData > 0) ? ((float)CurPar / CurData) * HLW8032_CF : 0.0f;
            last_power = (PowData > 0) ? ((float)PowPar / PowData) * HLW8032_VF * HLW8032_CF : 0.0f;

            return true;
        }
    }

    ERROR_PRINT("CH%u: No valid frame in buffered data\n", channel + 1);
    return false;
}

/**
 * @brief Get last parsed voltage reading (in volts).
 * This function returns the last measured voltage value.
 * @return Last voltage reading in volts
 */
float hlw8032_get_voltage(void) { return last_voltage; }

/**
 * @brief Get last parsed current reading (in amps).
 * This function returns the last measured current value.
 * @return Last current reading in amps
 */
float hlw8032_get_current(void) { return last_current; }

/**
 * @brief Get last parsed active power (in watts).
 * This function returns the last measured power value.
 * @return Last power reading in watts
 */
float hlw8032_get_power(void) { return last_power; }

/**
 * @brief Get estimated power using V × I (not factory-calibrated).
 * This function calculates the estimated power by multiplying voltage and current.
 * @return Estimated power in watts
 */
float hlw8032_get_power_inspect(void) { return last_voltage * last_current; }

/**
 * @brief Get estimated power factor (Active / Apparent).
 * This function calculates the power factor based on the last measured power and apparent power.
 * @return Power factor as a float
 */
float hlw8032_get_power_factor(void) {
    float inspected = hlw8032_get_power_inspect();
    return (inspected > 0) ? (last_power / inspected) : 0.0f;
}

/**
 * @brief Get estimated energy in kilowatt-hours.
 * This function calculates the total energy consumed in kWh based on the power factor and count.
 * It uses the formula: kWh = (PF * PF_Count) / (imp_per_wh * 3600)
 * where imp_per_wh is derived from the power parameters.
 * @return Estimated energy in kilowatt-hours
 */
float hlw8032_get_kwh(void) {
    float inspected = hlw8032_get_power_inspect();
    if (inspected <= 0 || PowPar == 0)
        return 0.0f;
    float imp_per_wh = (1.0f / PowPar) * (1.0f / inspected) * 1000000000.0f;
    float kwh = (PF * PF_Count) / (imp_per_wh * 3600.0f);
    return kwh;
}
