/**
 * @file src/tasks/ConsoleTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-11-08
 *
 * @details
 * Architecture:
 * 1. ConsoleTask polls USB-CDC at 10ms intervals (no ISR)
 * 2. Accumulates characters into line buffer
 * 3. On complete line: parses and dispatches to handlers
 * 4. Handlers execute directly or query other tasks
 *
 * Note: Console input comes from USB-CDC (stdio), not a hardware UART.
 * This matches the CMakeLists.txt config: pico_enable_stdio_usb(... 1)
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ==================== Bootloader Trigger ==================== */

/** Magic value to enter BOOTSEL mode on next reboot (survives reset) */
__attribute__((section(".uninitialized_data"))) static uint32_t bootloader_trigger;

/* ==================== Queue Handles ==================== */

QueueHandle_t q_power = NULL;
QueueHandle_t q_cfg = NULL;
QueueHandle_t q_meter = NULL;
QueueHandle_t q_net = NULL;

/* ==================== Console Task State ==================== */

#define LINE_BUF_SIZE 128
#define CONSOLE_POLL_MS 10

/** Line accumulator (task context, no ISR) */
static char line_buf[LINE_BUF_SIZE];
static uint16_t line_len = 0;

/* ==================== Helper Functions ==================== */

/**
 * @brief Trim leading and trailing whitespace from string (in-place).
 * @param str Input string (modified).
 * @return Pointer to trimmed string (same buffer).
 */
static char *trim(char *str) {
    /* Trim leading */
    while (*str == ' ' || *str == '\t')
        str++;

    /* Trim trailing */
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    return str;
}

/**
 * @brief Parse IP address string into 4-byte array.
 * @param str IP address string (e.g., "192.168.1.100")
 * @param ip Output array of 4 bytes
 * @return true on success, false on parse error
 */
static bool parse_ip(const char *str, uint8_t ip[4]) {
    int a, b, c, d;
    if (sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
        return false;
    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255)
        return false;
    ip[0] = (uint8_t)a;
    ip[1] = (uint8_t)b;
    ip[2] = (uint8_t)c;
    ip[3] = (uint8_t)d;
    return true;
}

/* ==================== Command Handlers ==================== */

/**
 * @brief Print help message with available commands.
 */
static void cmd_help(void) {
    ECHO("\n=== ENERGIS PDU Console Commands ===\n");
    ECHO("%-32s %s\n", "HELP", "Show this help");
    if (DEBUG)
        ECHO("%-32s %s\n", "REBOOT", "Reboot system");
    if (DEBUG)
        ECHO("%-32s %s\n", "BOOTSEL", "Enter bootloader mode");
    ECHO("%-32s %s\n", "SYSINFO", "Show system information");
    ECHO("%-32s %s\n", "GET_TEMP", "Read die temperature");
    ECHO("%-32s %s\n", "------------ Output Control ------------", "");
    ECHO("%-32s %s\n", "SET_CH <ch> <0|1|ON|OFF|ALL>", "Set relay channel (ch=1-8)");
    ECHO("%-32s %s\n", "GET_CH <ch>", "Get relay channel state (ch=1-8)");
    ECHO("%-32s %s\n", "CLR_ERR", "Clear error LED");
    ECHO("%-32s %s\n", "------------ Power Measurement ------------", "");
    ECHO("%-32s %s\n", "METER <ch>", "Read power data for channel (1-8 or ALL)");
    ECHO("%-32s %s\n", "READ_HLW8032", "Read power data from all channels");
    ECHO("%-32s %s\n", "READ_HLW8032 <ch>", "Read power data from specific channel (1-8)");
    ECHO("%-32s %s\n", "------------ Calibration ------------", "");
    ECHO("%-32s %s\n", "CALIBRATE <ch> <V> <I>", "Calibrate channel (ch=1-8)");
    ECHO("%-32s %s\n", "AUTO_CAL_ZERO", "Zero-calibrate all channels");
    ECHO("%-32s %s\n", "AUTO_CAL_V <voltage>", "Voltage-calibrate all channels");
    ECHO("%-32s %s\n", "SHOW_CALIB <ch>", "Show calibration data (1-8 or ALL)");
    ECHO("%-32s %s\n", "------------ Network Settings ------------", "");
    ECHO("%-32s %s\n", "SET_IP <ip>", "Set IP address");
    ECHO("%-32s %s\n", "SET_SN <mask>", "Set subnet mask");
    ECHO("%-32s %s\n", "SET_GW <gw>", "Set gateway");
    ECHO("%-32s %s\n", "SET_DNS <dns>", "Set DNS server");
    ECHO("%-32s %s\n", "CONFIG_NETWORK <ip$sn$gw$dns>", "Configure all network settings");
    ECHO("%-32s %s\n", "NETINFO", "Show network information");
    ECHO("%-32s %s\n", "------------ System ------------", "");
    if (DEBUG) ECHO("%-32s %s\n", "GET_SUPPLY", "Read 12V supply voltage");
    if (DEBUG) ECHO("%-32s %s\n", "GET_USB", "Read USB supply voltage");
    if (DEBUG) ECHO("%-32s %s\n", "DUMP_EEPROM", "Dump EEPROM contents");
    if (DEBUG) ECHO("%-32s %s\n", "RFS", "Reset to factory settings");
    ECHO("=====================================\n");
}

/**
 * @brief Enter BOOTSEL (USB ROM bootloader) now, robustly.
 *        Flush CDC, stop scheduling, disconnect USB, then jump.
 */
static void cmd_bootsel(void) {
    ECHO("Entering BOOTSEL mode now...\n");

    /* Optional breadcrumb if you read noinit on next cold boot */
    bootloader_trigger = 0xDEADBEEF;

    /* Give host a moment to read the last line */
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Flush stdio USB if present */
#if PICO_STDIO_USB
    extern void stdio_usb_flush(void);
    stdio_usb_flush();
#endif

    /* Try TinyUSB CDC flush if linked */
#ifdef CFG_TUD_CDC
    extern bool tud_cdc_connected(void);
    extern uint32_t tud_task_interval_ms;
    extern void tud_task(void);
    extern void tud_cdc_write_flush(void);
    for (int i = 0; i < 10; i++) {
        tud_cdc_write_flush();
        tud_task();
        busy_wait_ms(2);
    }
#endif

    /* Stop everyone else from running mid-transition */
    taskENTER_CRITICAL();
    vTaskSuspendAll();

    /* Best effort: disconnect from USB to force re-enumeration on host */
#if PICO_STDIO_USB
    extern void tud_disconnect(void);
    tud_disconnect(); /* ignore if not linked */
#endif
    busy_wait_ms(40); /* let host notice the drop */

    /* Jump to ROM bootloader */
    reset_usb_boot(0, 0);

    /* Should never return; if it does, park CPU */
    for (;;)
        __asm volatile("wfi");
}

/**
 * @brief Reboot system via watchdog.
 */
static void cmd_reboot(void) {
    ECHO("Rebooting system...\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    Health_RebootNow("CLI REBOOT");

    /* Never returns */
}

/**
 * @brief Read power data from HLW8032 for a specific channel
 */
static void cmd_read_hlw8032_ch(const char *args) {
    int ch;
    if (sscanf(args, "%d", &ch) != 1 || ch < 1 || ch > 8) {
        ECHO("Error: Invalid channel. Usage: READ_HLW8032 <ch> (1-8)\n");
        return;
    }

    meter_telemetry_t telem;
    if (MeterTask_GetTelemetry(ch - 1, &telem) && telem.valid) {
        ECHO("CH%u: V=%.2f V, I=%.3f A, P=%.2f W\n\n", ch, telem.voltage, telem.current,
             telem.power);
    } else {
        ECHO("CH%u: No valid telemetry data available\n\n", ch);
    }
}

/**
 * @brief Read power data from HLW8032 for all channels
 */
static void cmd_read_hlw8032_all(void) {
    for (int ch = 0; ch < 8; ch++) {
        meter_telemetry_t telem;
        if (MeterTask_GetTelemetry(ch, &telem) && telem.valid) {
            ECHO("CH%u: V=%.2f V, I=%.3f A, P=%.2f W\n", ch + 1, telem.voltage, telem.current,
                 telem.power);
        } else {
            ECHO("CH%u: No valid telemetry data available\n", ch + 1);
        }
    }
    ECHO("\n");
}

/**
 * @brief Print system information (clocks, voltage, FreeRTOS status).
 */
static void cmd_sysinfo(void) {
    /* -------- Get clock frequencies -------- */
    float sys_freq_mhz = clock_get_hz(clk_sys) / 1000000.0f;
    float usb_freq_mhz = clock_get_hz(clk_usb) / 1000000.0f;
    float peri_freq_mhz = clock_get_hz(clk_peri) / 1000000.0f;
    float adc_freq_mhz = clock_get_hz(clk_adc) / 1000000.0f;

    ECHO("\n=== System Information ===\n");

    /* -------- Read core voltage regulator setting -------- */
    uint32_t vreg_raw = *((volatile uint32_t *)VREG_BASE);
    uint vsel = vreg_raw & VREG_VSEL_MASK;
    float voltage = 1.10f + 0.05f * vsel;

    /* -------- Read serial & firmware from EEPROM -------- */
    char serial_buf[64] = {0};
    char fw_buf[32] = {0};
    uint8_t sys_area[EEPROM_SYS_INFO_SIZE] = {0};
    EEPROM_ReadSystemInfo(sys_area, EEPROM_SYS_INFO_SIZE);

    if (sys_area[0] != 0) {
        size_t s_len = strnlen((const char *)sys_area, EEPROM_SYS_INFO_SIZE);
        if (s_len > 0 && s_len < sizeof(serial_buf)) {
            memcpy(serial_buf, sys_area, s_len);
            serial_buf[s_len] = '\0';

            size_t pos = s_len + 1;
            if (pos < EEPROM_SYS_INFO_SIZE && sys_area[pos] != 0) {
                size_t f_len = strnlen((const char *)&sys_area[pos], EEPROM_SYS_INFO_SIZE - pos);
                if (f_len > 0 && f_len < sizeof(fw_buf)) {
                    memcpy(fw_buf, &sys_area[pos], f_len);
                    fw_buf[f_len] = '\0';
                }
            }
        }
    }

    /* -------- Read VUSB from ADC (GPIO26, channel 0) -------- */
    adc_select_input(V_USB);
    vTaskDelay(pdMS_TO_TICKS(1)); /* Allow ADC to settle */
    uint16_t raw_vusb = adc_read();
    float v_usb = get_Voltage(V_USB) * VBUS_DIVIDER;

    /* -------- Read 12V supply (GPIO29, ADC channel 3) -------- */
    adc_select_input(V_SUPPLY);
    vTaskDelay(pdMS_TO_TICKS(1)); /* Allow ADC to settle */
    uint16_t raw_12v = adc_read();
    float v_12v = get_Voltage(V_SUPPLY) * SUPPLY_DIVIDER;

    /* --------  Assemble and print info -------- */
    ECHO("\n=== Hardware Specific Info ===\n");
    ECHO("CPU Frequency: %.2f MHz\n", sys_freq_mhz);
    ECHO("USB Frequency: %.2f MHz\n", usb_freq_mhz);
    ECHO("PERI Frequency: %.2f MHz\n", peri_freq_mhz);
    ECHO("ADC Frequency: %.2f MHz\n", adc_freq_mhz);
    ECHO("FreeRTOS Tick: %lu\n", (unsigned long)xTaskGetTickCount());
    ECHO("Core voltage: %.2f V (vsel = %u)\n", voltage, vsel);

    ECHO("\n=== Device Firmware Info ===\n");
    ECHO("Device Serial: %s\n", serial_buf);
    ECHO("Firmware Ver: %s\n", fw_buf);

    ECHO("\n=== Device Voltages ===\n");
    ECHO("USB Voltage: %.2f V (ADC raw=%u)\n", v_usb, raw_vusb);
    ECHO("12V Supply: %.2f V (ADC raw=%u)\n", v_12v, raw_12v);
    ECHO("==========================\n");
}

/**
 * @brief Read and print RP2040 die temperature.
 */
static void cmd_get_temp(void) {
    adc_select_input(4);          /* Temperature sensor */
    vTaskDelay(pdMS_TO_TICKS(1)); /* Allow ADC to settle */
    uint16_t raw = adc_read();

    float voltage = (raw * ADC_VREF) / ADC_MAX;
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;

    ECHO("Die Temperature: %.2f °C (ADC raw=%u, V=%.3f)\n", temp_c, raw, voltage);
}

/**
 * @brief Set relay channel state.
 * @param args Command arguments: "<ch> <0|1|ON|OFF>" where ch=1-8 or ALL
 */
static void cmd_set_ch(const char *args) {
    char ch_str[16] = {0};
    char tok[16] = {0};

    if (sscanf(args, "%15s %15s", ch_str, tok) != 2) {
        ERROR_PRINT("Usage: SET_CH <ch> <0|1|ON|OFF|ALL> \n");
        return;
    }

    /* Convert ch_str to uppercase for ALL comparison */
    for (char *p = ch_str; *p; ++p) {
        *p = (char)toupper((unsigned char)*p);
    }

    /* Normalize token and resolve to 0/1 */
    int val = -1;
    if (tok[0] == '0' && tok[1] == '\0') {
        val = 0;
    } else if (tok[0] == '1' && tok[1] == '\0') {
        val = 1;
    } else {
        for (char *p = tok; *p; ++p) {
            *p = (char)toupper((unsigned char)*p);
        }
        if (strcmp(tok, "OFF") == 0) {
            val = 0;
        } else if (strcmp(tok, "ON") == 0) {
            val = 1;
        }
    }

    if (val < 0) {
        ERROR_PRINT("Invalid value: '%s' (use 0/1 or ON/OFF)\n", tok);
        return;
    }

    /* Handle ALL channels */
    if (strcmp(ch_str, "ALL") == 0) {
        bool success = true;
        for (uint8_t ch_idx = 0; ch_idx < 8; ch_idx++) {
            if (!mcp_set_channel_state(ch_idx, (uint8_t)val)) {
                ERROR_PRINT("Failed to set CH%d\n", ch_idx + 1);
                success = false;
            }
        }
        if (success) {
            ECHO("All channels set to %s\n", val ? "ON" : "OFF");
        }
        return;
    }

    /* Handle single channel */
    int ch = atoi(ch_str);
    if (ch < 1 || ch > 8) {
        ERROR_PRINT("Invalid channel: %s (must be 1-8 or ALL)\n", ch_str);
        return;
    }

    uint8_t ch_idx = (uint8_t)(ch - 1);
    if (mcp_set_channel_state(ch_idx, (uint8_t)val)) {
        ECHO("CH%d = %s\n", ch, val ? "ON" : "OFF");
    } else {
        ERROR_PRINT("Failed to set CH%d\n", ch);
    }
}

/**
 * @brief Get relay channel state(s).
 * @param args Command arguments: "<ch>" where ch=1-8 or "ALL"
 */
static void cmd_get_ch(const char *args) {
    if (args == NULL || *args == '\0') {
        ERROR_PRINT("Usage: GET_CH <ch> (ch=1-8 or ALL)\n");
        return;
    }

    /* Make a local, trimmed/uppercased copy of the argument */
    char tok[16];
    size_t n = strnlen(args, sizeof(tok) - 1);
    strncpy(tok, args, n);
    tok[n] = '\0';
    char *p = tok;
    while (*p == ' ' || *p == '\t')
        p++; /* trim left */
    for (char *q = p + strlen(p) - 1;
         q >= p && (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n'); q--) {
        *q = '\0'; /* trim right */
    }
    for (char *q = p; *q; ++q) {
        *q = (char)toupper((unsigned char)*q); /* uppercase */
    }

    if (strcmp(p, "ALL") == 0) {
        for (uint8_t i = 0; i < 8; i++) {
            bool state = mcp_get_channel_state(i);
            ECHO("CH%d: %s\r\n", (int)(i + 1), state ? "ON" : "OFF");
        }
        return;
    }

    int ch;
    if (sscanf(p, "%d", &ch) != 1 || ch < 1 || ch > 8) {
        ERROR_PRINT("Invalid channel. Usage: GET_CH <ch> (ch=1-8 or ALL)\n");
        return;
    }

    uint8_t ch_idx = (uint8_t)(ch - 1);
    bool state = mcp_get_channel_state(ch_idx);
    ECHO("CH%d: %s\r\n", ch, state ? "ON" : "OFF");
}

/**
 * @brief Clear error LED on display board.
 */
static void cmd_clr_err(void) {
    setError(false);
    ECHO("Error LED cleared\n");
}

/**
 * @brief Read power measurement data from MeterTask.
 * @param args Command arguments: "<ch>" where ch=1-8 or "ALL"
 */
static void cmd_meter(const char *args) {
    if (args == NULL || strlen(args) == 0) {
        ERROR_PRINT("Usage: METER <ch> (ch=1-8 or ALL)\n");
        return;
    }

    /* Parse argument */
    char arg[16];
    strncpy(arg, args, sizeof(arg) - 1);
    arg[sizeof(arg) - 1] = '\0';
    for (char *p = arg; *p; p++)
        *p = (char)toupper((unsigned char)*p);

    if (strcmp(arg, "ALL") == 0) {
        /* Read all channels */
        ECHO("\n=== Power Measurement - All Channels ===\n");
        for (uint8_t ch = 0; ch < 8; ch++) {
            meter_telemetry_t telem;
            if (MeterTask_GetTelemetry(ch, &telem) && telem.valid) {
                ECHO("CH%d: V=%.2fV I=%.3fA P=%.1fW PF=%.3f kWh=%.3f Uptime=%us %s\n", ch + 1,
                     telem.voltage, telem.current, telem.power, telem.power_factor,
                     telem.energy_kwh, telem.uptime, telem.relay_state ? "ON" : "OFF");
            } else {
                ECHO("CH%d: No valid data\n", ch + 1);
            }
        }
        ECHO("========================================\n");
    } else {
        /* Read single channel */
        int ch = atoi(arg);
        if (ch < 1 || ch > 8) {
            ERROR_PRINT("Invalid channel: %d (must be 1-8 or ALL)\n", ch);
            return;
        }

        uint8_t ch_idx = (uint8_t)(ch - 1);
        meter_telemetry_t telem;
        if (MeterTask_GetTelemetry(ch_idx, &telem) && telem.valid) {
            ECHO("CH%d: V=%.2fV I=%.3fA P=%.1fW PF=%.3f kWh=%.3f Uptime=%us %s\n", ch,
                 telem.voltage, telem.current, telem.power, telem.power_factor, telem.energy_kwh,
                 telem.uptime, telem.relay_state ? "ON" : "OFF");
        } else {
            ERROR_PRINT("CH%d: No valid measurement data\n", ch);
        }
    }
}

/**
 * @brief Calibrate a single HLW8032 channel.
 * @param args Command arguments: "<ch> <voltage> <current>"
 */
static void cmd_calibrate(const char *args) {
    int ch;
    float ref_v, ref_a;

    if (sscanf(args, "%d %f %f", &ch, &ref_v, &ref_a) != 3) {
        ERROR_PRINT("Usage: CALIBRATE <ch> <voltage> <current>\n");
        ERROR_PRINT("Examples:\n");
        ERROR_PRINT("  CALIBRATE 1 0 0        (zero-point: channel OFF/disconnected)\n");
        ERROR_PRINT("  CALIBRATE 1 230.0 0    (voltage-only, no load required)\n");
        ERROR_PRINT("  CALIBRATE 1 230.0 1.5  (with known load)\n");
        return;
    }

    if (ch < 1 || ch > 8) {
        ERROR_PRINT("Invalid channel: %d (must be 1-8)\n", ch);
        return;
    }

    if (ref_v < 0.0f || ref_a < 0.0f) {
        ERROR_PRINT("Reference values cannot be negative\n");
        return;
    }

    ECHO("Starting calibration for channel %d...\n", ch);

    if (ref_v == 0.0f && ref_a == 0.0f) {
        ECHO("Zero-point calibration: measuring offsets\n");
        ECHO("Please ensure channel is OFF or disconnected\n\n");
    } else {
        ECHO("Reference: %.3fV, %.3fA\n", ref_v, ref_a);
    }

    if (hlw8032_calibrate_channel((uint8_t)(ch - 1), ref_v, ref_a)) {
        ECHO("Calibration completed successfully!\n");
    } else {
        ERROR_PRINT("Calibration failed!\n");
        setError(true);
    }
}

/**
 * @brief Auto-calibrate zero point for all channels.
 */
static void cmd_auto_cal_zero(void) {
    ECHO("\n========================================\n");
    ECHO("  AUTO ZERO-POINT CALIBRATION\n");
    ECHO("========================================\n");
    ECHO("Calibrating all 8 channels (0V, 0A)\n");
    ECHO("Ensure all channels are OFF/disconnected\n");
    ECHO("========================================\n\n");

    int success = 0, failed = 0;
    for (uint8_t ch = 0; ch < 8; ch++) {
        ECHO("Channel %d: ", ch + 1);
        if (hlw8032_calibrate_channel(ch, 0.0f, 0.0f)) {
            ECHO("OK\n");
            success++;
        } else {
            ECHO("FAILED\n");
            failed++;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ECHO("========================================\n");
    ECHO("RESULTS: %d successful, %d failed\n", success, failed);
    ECHO("========================================\n\n");

    if (failed > 0)
        setError(true);
}

/**
 * @brief Auto-calibrate voltage for all channels.
 * @param args Command arguments: "<voltage>"
 */
static void cmd_auto_cal_v(const char *args) {
    float ref_voltage = atof(args);
    if (ref_voltage <= 0.0f) {
        ERROR_PRINT("Invalid reference voltage: %.3f (must be > 0)\n", ref_voltage);
        return;
    }

    ECHO("\n========================================\n");
    ECHO("  AUTO VOLTAGE CALIBRATION\n");
    ECHO("========================================\n");
    ECHO("Calibrating all 8 channels (%.1fV, 0A)\n", ref_voltage);
    ECHO("Ensure all channels have mains voltage\n");
    ECHO("========================================\n\n");

    int success = 0, failed = 0;
    for (uint8_t ch = 0; ch < 8; ch++) {
        ECHO("Channel %d: ", ch + 1);
        if (hlw8032_calibrate_channel(ch, ref_voltage, 0.0f)) {
            ECHO("OK\n");
            success++;
        } else {
            ECHO("FAILED\n");
            failed++;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    ECHO("========================================\n");
    ECHO("RESULTS: %d successful, %d failed\n", success, failed);
    ECHO("========================================\n\n");

    if (failed > 0)
        setError(true);
}

/**
 * @brief Show calibration data for one or all channels.
 * @param args Command arguments: "<ch>" where ch=1-8, or empty for ALL
 */
static void cmd_show_calib(const char *args) {
    if (args == NULL || strlen(args) == 0) {
        /* Show all channels */
        for (uint8_t i = 0; i < 8; i++) {
            hlw8032_print_calibration(i);
        }
    } else {
        /* Show specific channel */
        int ch = atoi(args);
        if (ch < 1 || ch > 8) {
            ERROR_PRINT("Invalid channel: %d (must be 1-8)\n", ch);
            return;
        }
        hlw8032_print_calibration((uint8_t)(ch - 1));
    }
}

/**
 * @brief Set IP address.
 * @param args Command arguments: "<ip>" e.g., "192.168.1.100"
 */
static void cmd_set_ip(const char *args) {
    uint8_t ip[4];
    if (!parse_ip(args, ip)) {
        ERROR_PRINT("Invalid IP address format: %s\n", args);
        return;
    }

    networkInfo net;
    if (!storage_get_network(&net)) {
        ERROR_PRINT("Failed to read network config\n");
        return;
    }

    memcpy(net.ip, ip, 4);

    if (storage_set_network(&net)) {
        ECHO("IP address set to %d.%d.%d.%d (reboot required)\n", ip[0], ip[1], ip[2], ip[3]);
    } else {
        ERROR_PRINT("Failed to save IP address\n");
    }
}

/**
 * @brief Set subnet mask.
 * @param args Command arguments: "<mask>" e.g., "255.255.255.0"
 */
static void cmd_SET_SN(const char *args) {
    uint8_t sn[4];
    if (!parse_ip(args, sn)) {
        ERROR_PRINT("Invalid subnet mask format: %s\n", args);
        return;
    }

    networkInfo net;
    if (!storage_get_network(&net)) {
        ERROR_PRINT("Failed to read network config\n");
        return;
    }

    memcpy(net.sn, sn, 4);

    if (storage_set_network(&net)) {
        ECHO("Subnet mask set to %d.%d.%d.%d (reboot required)\n", sn[0], sn[1], sn[2], sn[3]);
    } else {
        ERROR_PRINT("Failed to save subnet mask\n");
    }
}

/**
 * @brief Set gateway address.
 * @param args Command arguments: "<gw>" e.g., "192.168.1.1"
 */
static void cmd_set_gw(const char *args) {
    uint8_t gw[4];
    if (!parse_ip(args, gw)) {
        ERROR_PRINT("Invalid gateway format: %s\n", args);
        return;
    }

    networkInfo net;
    if (!storage_get_network(&net)) {
        ERROR_PRINT("Failed to read network config\n");
        return;
    }

    memcpy(net.gw, gw, 4);

    if (storage_set_network(&net)) {
        ECHO("Gateway set to %d.%d.%d.%d (reboot required)\n", gw[0], gw[1], gw[2], gw[3]);
    } else {
        ERROR_PRINT("Failed to save gateway\n");
    }
}

/**
 * @brief Set DNS server address.
 * @param args Command arguments: "<dns>" e.g., "8.8.8.8"
 */
static void cmd_set_dns(const char *args) {
    uint8_t dns[4];
    if (!parse_ip(args, dns)) {
        ERROR_PRINT("Invalid DNS format: %s\n", args);
        return;
    }

    networkInfo net;
    if (!storage_get_network(&net)) {
        ERROR_PRINT("Failed to read network config\n");
        return;
    }

    memcpy(net.dns, dns, 4);

    if (storage_set_network(&net)) {
        ECHO("DNS set to %d.%d.%d.%d (reboot required)\n", dns[0], dns[1], dns[2], dns[3]);
    } else {
        ERROR_PRINT("Failed to save DNS\n");
    }
}

/**
 * @brief Configure all network settings at once.
 * @param args Command arguments: "<ip$sn$gw$dns>"
 */
static void cmd_config_network(const char *args) {
    char ip_str[20], sn_str[20], gw_str[20], dns_str[20];
    if (sscanf(args, "%19[^$]$%19[^$]$%19[^$]$%19s", ip_str, sn_str, gw_str, dns_str) != 4) {
        ERROR_PRINT("Usage: CONFIG_NETWORK <ip$sn$gw$dns>\n");
        ERROR_PRINT("Example: CONFIG_NETWORK 192.168.1.100$255.255.255.0$192.168.1.1$8.8.8.8\n");
        return;
    }

    networkInfo net;
    if (!storage_get_network(&net)) {
        ERROR_PRINT("Failed to read network config\n");
        return;
    }

    if (!parse_ip(ip_str, net.ip)) {
        ERROR_PRINT("Invalid IP: %s\n", ip_str);
        return;
    }
    if (!parse_ip(sn_str, net.sn)) {
        ERROR_PRINT("Invalid subnet: %s\n", sn_str);
        return;
    }
    if (!parse_ip(gw_str, net.gw)) {
        ERROR_PRINT("Invalid gateway: %s\n", gw_str);
        return;
    }
    if (!parse_ip(dns_str, net.dns)) {
        ERROR_PRINT("Invalid DNS: %s\n", dns_str);
        return;
    }

    if (storage_set_network(&net)) {
        ECHO("Network configured successfully:\n");
        ECHO("  IP: %s\n", ip_str);
        ECHO("  Subnet: %s\n", sn_str);
        ECHO("  Gateway: %s\n", gw_str);
        ECHO("  DNS: %s\n", dns_str);
        ECHO("Reboot required to apply changes.\n");
    } else {
        ERROR_PRINT("Failed to save network config\n");
    }
}

/**
 * @brief Display network information.
 */
static void cmd_netinfo(void) {
    ECHO("NETWORK INFORMATION:\n");
    networkInfo ni;
    if (!storage_get_network(&ni)) {
        ni = LoadUserNetworkConfig();
    }
    ECHO("[ETH] Network Configuration:\n");
    ECHO("[ETH]  MAC  : %02X:%02X:%02X:%02X:%02X:%02X\n", ni.mac[0], ni.mac[1], ni.mac[2],
         ni.mac[3], ni.mac[4], ni.mac[5]);
    ECHO("[ETH]  IP   : %u.%u.%u.%u\n", ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3]);
    ECHO("[ETH]  Mask : %u.%u.%u.%u\n", ni.sn[0], ni.sn[1], ni.sn[2], ni.sn[3]);
    ECHO("[ETH]  GW   : %u.%u.%u.%u\n", ni.gw[0], ni.gw[1], ni.gw[2], ni.gw[3]);
    ECHO("[ETH]  DNS  : %u.%u.%u.%u\n", ni.dns[0], ni.dns[1], ni.dns[2], ni.dns[3]);
    ECHO("[ETH]  DHCP : %s\n", ni.dhcp ? "Enabled" : "Disabled");
}

/**
 * @brief Read 12V supply voltage.
 */
static void cmd_get_supply(void) {
    adc_select_input(V_SUPPLY);
    vTaskDelay(pdMS_TO_TICKS(1));
    float voltage = get_Voltage(V_SUPPLY) * SUPPLY_DIVIDER;
    ECHO("12V Supply: %.2f V\n", voltage);
}

/**
 * @brief Read USB supply voltage.
 */
static void cmd_get_usb(void) {
    adc_select_input(V_USB);
    vTaskDelay(pdMS_TO_TICKS(1));
    float voltage = get_Voltage(V_USB) * VBUS_DIVIDER;
    ECHO("USB Supply: %.2f V\n", voltage);
}

/* ==================== Command Dispatcher ==================== */

/**
 * @brief Parse and dispatch command line to appropriate handler.
 * @param line Null-terminated command line string.
 */
static void dispatch_command(const char *line) {
    char buf[LINE_BUF_SIZE];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *trimmed = trim(buf);
    if (strlen(trimmed) == 0)
        return; /* Empty line */

    /* Split command and arguments */
    char *args = strchr(trimmed, ' ');
    if (args) {
        *args = '\0';
        args++;
        args = trim(args);
    }

    /* Convert command to uppercase for case-insensitive matching */
    for (char *p = trimmed; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }

    /* Dispatch to handlers */
    if (strcmp(trimmed, "HELP") == 0 || strcmp(trimmed, "?") == 0) {
        cmd_help();
    } else if (strcmp(trimmed, "REBOOT") == 0) {
        cmd_reboot();
    } else if (strcmp(trimmed, "BOOTSEL") == 0) {
        cmd_bootsel();
    } else if (strcmp(trimmed, "SYSINFO") == 0) {
        cmd_sysinfo();
    } else if (strcmp(trimmed, "GET_TEMP") == 0) {
        cmd_get_temp();
    } else if (strcmp(trimmed, "SET_CH") == 0) {
        cmd_set_ch(args ? args : "");
    } else if (strcmp(trimmed, "GET_CH") == 0) {
        cmd_get_ch(args ? args : "");
    } else if (strcmp(trimmed, "CLR_ERR") == 0) {
        cmd_clr_err();
    } else if (strcmp(trimmed, "METER") == 0) {
        cmd_meter(args ? args : "");
    } else if (strcmp(trimmed, "READ_HLW8032") == 0) {
        if (args) {
            cmd_read_hlw8032_ch(args);
        } else {
            cmd_read_hlw8032_all();
        }
    } else if (strcmp(trimmed, "CALIBRATE") == 0) {
        cmd_calibrate(args ? args : "");
    } else if (strcmp(trimmed, "AUTO_CAL_ZERO") == 0 ||
               strcmp(trimmed, "AUTO_CALIBRATE_ZERO") == 0) {
        cmd_auto_cal_zero();
    } else if (strcmp(trimmed, "AUTO_CAL_V") == 0 ||
               strcmp(trimmed, "AUTO_CALIBRATE_VOLTAGE") == 0) {
        cmd_auto_cal_v(args ? args : "");
    } else if (strcmp(trimmed, "SHOW_CALIB") == 0) {
        cmd_show_calib(args ? args : "");
    } else if (strcmp(trimmed, "SET_IP") == 0) {
        cmd_set_ip(args ? args : "");
    } else if (strcmp(trimmed, "SET_SN") == 0 || strcmp(trimmed, "SET_SN") == 0) {
        cmd_SET_SN(args ? args : "");
    } else if (strcmp(trimmed, "SET_GW") == 0) {
        cmd_set_gw(args ? args : "");
    } else if (strcmp(trimmed, "SET_DNS") == 0) {
        cmd_set_dns(args ? args : "");
    } else if (strcmp(trimmed, "CONFIG_NETWORK") == 0) {
        cmd_config_network(args ? args : "");
    } else if (strcmp(trimmed, "NETINFO") == 0) {
        cmd_netinfo();
    } else if (strcmp(trimmed, "GET_SUPPLY") == 0) {
        cmd_get_supply();
    } else if (strcmp(trimmed, "GET_USB") == 0) {
        cmd_get_usb();
    } else if (strcmp(trimmed, "DUMP_EEPROM") == 0) {
        CAT24C512_DumpFormatted();
    } else if (strcmp(trimmed, "RFS") == 0) {
        EEPROM_WriteFactoryDefaults();
        ECHO("Factory defaults written. Rebooting...\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        Health_RebootNow("CLI REBOOT");
    } else if (strcmp(trimmed, "BAADCAFE") == 0) {
        ECHO("Erasing EEPROM... (THIS CANNOT BE UNDONE!)\n");
        EEPROM_EraseAll();
        ECHO("EEPROM erased. Reboot required.\n");
    } else {
        ERROR_PRINT("Unknown command: '%s'. Type HELP for list.\n", trimmed);
    }
}

/* ==================== Console Task ==================== */

/**
 * @brief FreeRTOS task: polls USB-CDC, accumulates lines, dispatches commands.
 * @param arg Unused.
 */
static void ConsoleTask(void *arg) {
    (void)arg;

    /* Wait for logger to be ready */
    vTaskDelay(pdMS_TO_TICKS(1500));
    ECHO("\n");
    ECHO("=== ENERGIS Console Ready ===\n");
    ECHO("Type HELP for available commands\n");
    ECHO("\n");

    const TickType_t poll_ticks = pdMS_TO_TICKS(CONSOLE_POLL_MS);
    static uint32_t hb_cons_ms = 0;

    for (;;) {
        uint32_t __now = to_ms_since_boot(get_absolute_time());
        if ((__now - hb_cons_ms) >= CONSOLETASKBEAT_MS) {
            hb_cons_ms = __now;
            Health_Heartbeat(HEALTH_ID_CONSOLE);
        }

        /* Poll USB-CDC for available characters (non-blocking with timeout) */
        int ch_int = getchar_timeout_us(0); /* 0 = non-blocking */

        if (ch_int != PICO_ERROR_TIMEOUT) {
            char ch = (char)ch_int;

            /* Handle backspace (BS or DEL) */
            if (ch == '\b' || ch == 0x7F) {
                if (line_len > 0) {
                    line_len--;
                }
            } else if (ch == '\r' || ch == '\n') {
                line_buf[line_len] = '\0';
                if (line_len > 0) {
                    dispatch_command(line_buf);
                    line_len = 0;
                }
                log_printf("\r\n");
            } else if (line_len < (int)(sizeof(line_buf) - 1)) {
                line_buf[line_len++] = ch;
            }
        } else {
            vTaskDelay(poll_ticks);
        }
    }
}

/* ==================== Public API ==================== */

/**
 * @brief Initialize and start the Console task with a deterministic enable gate.
 *
 * @details
 * Deterministic boot order step 2/6.
 * - Waits up to 5 s for Logger_IsReady() to report ready.
 * - Creates Console queues (q_power, q_cfg, q_meter, q_net).
 * - Spawns the ConsoleTask.
 * - Returns pdPASS on success, pdFAIL on any creation error.
 *
 * @instructions
 * Call after LoggerTask_Init(true):
 *   BaseType_t rc = ConsoleTask_Init(true);
 * Gate subsequent steps with Console_IsReady().
 *
 * @param enable Gate that allows or skips starting this subsystem.
 * @return pdPASS on success (or when skipped), pdFAIL on creation error.
 */
BaseType_t ConsoleTask_Init(bool enable) {
    if (!enable) {
        return pdPASS;
    }

    /* Wait briefly for logger so prints don't jam CDC */
    {
        extern bool Logger_IsReady(void);
        TickType_t t0 = xTaskGetTickCount();
        while (!Logger_IsReady() && (xTaskGetTickCount() - t0) < pdMS_TO_TICKS(2000)) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    /* Queues: ensure item sizes MATCH the consumer task types */
    extern QueueHandle_t q_power, q_cfg, q_meter, q_net;
    q_power = xQueueCreate(8, sizeof(power_msg_t));
    q_meter = xQueueCreate(8, sizeof(meter_msg_t));
    q_net = xQueueCreate(8, sizeof(net_msg_t));

    /* The critical one: StorageTask expects storage_msg_t items on q_cfg */
    q_cfg = xQueueCreate(8, sizeof(storage_msg_t));

    if (!q_power || !q_cfg || !q_meter || !q_net) {
        ERROR_PRINT("[Console] Failed to create one or more queues\r\n");
        return pdFAIL;
    }

    extern void ConsoleTask(void *arg);
    if (xTaskCreate(ConsoleTask, "Console", 1024, NULL, CONSOLETASK_PRIORITY, NULL) != pdPASS) {
        ERROR_PRINT("[Console] Failed to create task\r\n");
        return pdFAIL;
    }

    INFO_PRINT("[Console] Task initialized (polling USB-CDC @ %ums)\r\n", CONSOLE_POLL_MS);
    return pdPASS;
}

/**
 * @brief Get Console READY state.
 *
 * @details
 * Console is considered READY once its core config queue has been created by
 * ConsoleTask_Init(). This avoids any extra latches or extern variables.
 *
 * @return true if the Console config queue exists, false otherwise.
 */
bool Console_IsReady(void) {
    extern QueueHandle_t q_cfg; /* created in ConsoleTask_Init() */
    return (q_cfg != NULL);
}