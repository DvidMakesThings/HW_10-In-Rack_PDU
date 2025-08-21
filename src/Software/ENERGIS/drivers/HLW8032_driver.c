/* HLW8032_driver.c */

#include "HLW8032_driver.h"
#include "../CONFIG.h" // for HLW8032_UART_ID, HLW8032_BAUDRATE, mcp_relay_write_pin
#include "pico/stdlib.h"
#include <string.h>

// frame buffer
static uint8_t frame[HLW8032_FRAME_LENGTH];
static uint8_t rx_buf[MAX_RX_BYTES];
static uint32_t channel_uptime[8] = {0};
static absolute_time_t channel_last_on[8];

// raw registers
static uint32_t VolPar, VolData;
static uint32_t CurPar, CurData;
static uint32_t PowPar, PowData;
static uint16_t PF;
static uint32_t PF_Count = 1;

// last scaled readings
static float last_voltage = 0.0f;
static float last_current = 0.0f;
static float last_power = 0.0f;

// Cached measurements
static float cached_voltage[8];
static float cached_current[8];
static float cached_power[8];
static uint32_t cached_uptime[8];
static bool cached_state[8];

// For reading schedule
static uint8_t poll_channel = 0;

// select MUX channel (3-bit)
static void mux_select(uint8_t ch) {
    mcp_relay_write_pin(MUX_SELECT_A, 0);
    mcp_relay_write_pin(MUX_SELECT_B, 0);
    mcp_relay_write_pin(MUX_SELECT_C, 0);
    sleep_us(500);
    mcp_relay_write_pin(MUX_SELECT_A, (ch >> 0) & 1);
    mcp_relay_write_pin(MUX_SELECT_B, (ch >> 1) & 1);
    mcp_relay_write_pin(MUX_SELECT_C, (ch >> 2) & 1);
    sleep_us(500);
}

// verify checksum over bytes 2–22 matches byte 23
static bool checksum_ok(const uint8_t *f) {
    uint8_t sum = 0;
    for (int i = 2; i <= 22; i++)
        sum += f[i];
    return sum == f[23];
}

// read one valid 24-byte frame into `frame[]`, return false on timeout or CRC fail
static bool read_frame(void) {
    uint64_t start = time_us_64();
    int cnt = 0;
    while (cnt < MAX_RX_BYTES && (time_us_64() - start) < UART_READ_TIMEOUT_US) {
        if (uart_is_readable(HLW8032_UART_ID)) {
            rx_buf[cnt++] = uart_getc(HLW8032_UART_ID);
        }
    }
    // scan for a window of 24 bytes starting at any index i
    for (int i = 0; i + HLW8032_FRAME_LENGTH <= cnt; i++) {
        if (rx_buf[i + 1] != 0x5A)
            continue;
        memcpy(frame, rx_buf + i, HLW8032_FRAME_LENGTH);
        if (checksum_ok(frame))
            return true;
    }
    return false;
}

void hlw8032_init(void) {
    uart_init(HLW8032_UART_ID, HLW8032_BAUDRATE);
    uart_set_format(HLW8032_UART_ID, 8, 1, UART_PARITY_EVEN);
    mcp_relay_write_pin(MUX_ENABLE, 0);
}

bool hlw8032_read(uint8_t channel) {
    // 1) select the channel on your MUX
    mux_select(channel);
    sleep_ms(55);

    // 2) flush any garbage
    while (uart_is_readable(HLW8032_UART_ID))
        uart_getc(HLW8032_UART_ID);

    // 3) discard the first (stale) frame
    (void)read_frame();

    // 4) now read the real frame
    if (!read_frame())
        return false;

    // 5) parse VolPar, VolData, CurPar, CurData, PowPar, PowData, PF from 'frame[]'
    VolPar = ((uint32_t)frame[2] << 16) | (frame[3] << 8) | frame[4];
    VolData = (frame[20] & 0x40) ? ((uint32_t)frame[5] << 16) | (frame[6] << 8) | frame[7] : 0;
    CurPar = ((uint32_t)frame[8] << 16) | (frame[9] << 8) | frame[10];
    CurData = (frame[20] & 0x20) ? ((uint32_t)frame[11] << 16) | (frame[12] << 8) | frame[13] : 0;
    PowPar = ((uint32_t)frame[14] << 16) | (frame[15] << 8) | frame[16];
    PowData = (frame[20] & 0x10) ? ((uint32_t)frame[17] << 16) | (frame[18] << 8) | frame[19] : 0;
    PF = ((uint16_t)frame[21] << 8) | frame[22];
    if (frame[20] & 0x80)
        PF_Count++;

    // 6) compute your scaled values in one shot
    last_voltage = (VolData > 0) ? ((float)VolPar / VolData) * HLW8032_VF : 0.0f;
    last_current = (CurData > 0) ? ((float)CurPar / CurData) * HLW8032_CF : 0.0f;
    last_power = last_voltage * last_current;

    return true;
}

float hlw8032_get_voltage(void) { return last_voltage; }
float hlw8032_get_current(void) { return last_current; }
float hlw8032_get_power(void) { return last_power; }
float hlw8032_get_power_inspect(void) { return last_voltage * last_current; }
float hlw8032_get_power_factor(void) {
    float app = last_voltage * last_current;
    return app > 0.0f ? last_power / app : 0.0f;
}
float hlw8032_get_kwh(void) {
    float app = last_voltage * last_current;
    if (app <= 0.0f || PowPar == 0)
        return 0.0f;
    float imp_per_wh = ((float)PowPar / app) / 1e9f;
    return ((float)PF * PF_Count) / (imp_per_wh * 3600.0f);
}

// Call when channel is ON to update uptime
void hlw8032_update_uptime(uint8_t ch, bool state) {
    if (state) {
        if (is_nil_time(channel_last_on[ch])) {
            channel_last_on[ch] = get_absolute_time();
        } else {
            // accumulate elapsed seconds
            absolute_time_t now = get_absolute_time();
            int64_t diff_us = absolute_time_diff_us(channel_last_on[ch], now);
            channel_uptime[ch] += diff_us / 1000000;
            channel_last_on[ch] = now;
        }
    } else {
        channel_last_on[ch] = nil_time; // reset timer if off
    }
}

uint32_t hlw8032_get_uptime(uint8_t ch) {
    return channel_uptime[ch];
}

void hlw8032_poll_once(void) {
    bool state = mcp_relay_read_pin(poll_channel);
    cached_state[poll_channel] = state;

    hlw8032_update_uptime(poll_channel, state);

    bool ok = hlw8032_read(poll_channel);
    cached_voltage[poll_channel] = ok ? hlw8032_get_voltage() : 0.0f;
    cached_current[poll_channel] = ok ? hlw8032_get_current() : 0.0f;
    cached_power[poll_channel]   = ok ? hlw8032_get_power()   : 0.0f;
    cached_uptime[poll_channel]  = hlw8032_get_uptime(poll_channel);

    poll_channel = (poll_channel + 1) % 8;  // next channel next time
}

void hlw8032_refresh_all(void) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        // Latch current relay state and update uptime bookkeeping
        bool state = mcp_relay_read_pin(ch);
        cached_state[ch] = state;
        hlw8032_update_uptime(ch, state);

        // Read one fresh frame for this channel (blocking per channel)
        bool ok = hlw8032_read(ch);

        // Store scaled results into SRAM cache (read-only for Core1)
        cached_voltage[ch] = ok ? hlw8032_get_voltage() : 0.0f;
        cached_current[ch] = ok ? hlw8032_get_current() : 0.0f;
        cached_power[ch]   = ok ? hlw8032_get_power()   : 0.0f;
        cached_uptime[ch]  = hlw8032_get_uptime(ch);
    }
}

// Getters for cached values
float hlw8032_cached_voltage(uint8_t ch) { return cached_voltage[ch]; }
float hlw8032_cached_current(uint8_t ch) { return cached_current[ch]; }
float hlw8032_cached_power(uint8_t ch)   { return cached_power[ch]; }
uint32_t hlw8032_cached_uptime(uint8_t ch) { return cached_uptime[ch]; }
bool hlw8032_cached_state(uint8_t ch)    { return cached_state[ch]; }