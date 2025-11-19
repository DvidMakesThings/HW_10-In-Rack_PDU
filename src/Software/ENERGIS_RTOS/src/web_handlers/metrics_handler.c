/**
 * @file src/web_handlers/metrics_handler.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-11-15
 *
 * @details Implements /metrics HTTP endpoint that exposes ENERGIS runtime data
 * in OpenMetrics/Prometheus text format. Renders system health gauges,
 * monotonic counters, and per-channel power telemetry. No heap allocation,
 * no blocking operations, reads only from cached snapshots.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define METRICS_HANDLER_TAG "<Metrics Handler>"

/* =====================  Global Counters  ============================== */
metrics_counters_t g_metrics = {0};

/* =====================  Static Buffer  ================================ */
static char metrics_buffer[METRICS_BUFFER_SIZE];

/**
 * @brief Initialize metrics subsystem.
 *
 * @return None
 *
 * @details Resets all counters to zero. Call during system initialization
 * before any other subsystems increment counters.
 */
void metrics_init(void) {
    memset(&g_metrics, 0, sizeof(g_metrics));
    INFO_PRINT("%s Metrics subsystem initialized\n", METRICS_HANDLER_TAG);
}

/**
 * @brief Render OpenMetrics exposition format to static buffer.
 *
 * @return Length of rendered text in bytes, or -1 on buffer overflow
 *
 * @details Formats all system metrics according to OpenMetrics 0.0.4
 * specification. Uses snprintf to prevent buffer overruns. Reads from
 * cached telemetry and global counters without blocking.
 *
 * @note Buffer overflow is handled gracefully by truncating output
 */
static int render_metrics(void) {
    int pos = 0;
    const int bufsize = METRICS_BUFFER_SIZE;

    /* Uptime */
    uint32_t uptime_sec = (uint32_t)(to_ms_since_boot(get_absolute_time()) / 1000);

    /* Build info */
    const char *version = FIRMWARE_VERSION;
    const char *serial = SERIAL_NUMBER;

    /* Calibrated system telemetry (owned by MeterTask) */
    system_telemetry_t sys = {0};
    bool sys_ok = MeterTask_GetSystemTelemetry(&sys);

    float temp_c = (sys_ok && sys.valid) ? sys.die_temp_c : 0.0f;
    float v_usb = (sys_ok && sys.valid) ? sys.vusb_volts : 0.0f;
    float v_12v = (sys_ok && sys.valid) ? sys.vsupply_volts : 0.0f;

    /* Temp calibration flags */
    uint8_t cal_mode = 0; /* 0=none, 1=1pt, 2=2pt */
    float v0, slope, offs;
    bool have_cal = MeterTask_GetTempCalibrationInfo(&cal_mode, &v0, &slope, &offs);
    int calibrated = (have_cal && cal_mode != 0) ? 1 : 0;

    /* Handler up */
    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_up 1 if the metrics handler is healthy.\n"
                    "# TYPE energis_up gauge\n"
                    "energis_up 1\n");
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x0);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_up\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }
    net_beat();

    /* Build info */
    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_build_info Build and device identifiers.\n"
                    "# TYPE energis_build_info gauge\n"
                    "energis_build_info{version=\"%s\",serial=\"%s\"} 1\n",
                    version, serial);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_build_info\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }
    net_beat();

    /* Uptime */
    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_uptime_seconds_total System uptime in seconds.\n"
                    "# TYPE energis_uptime_seconds_total counter\n"
                    "energis_uptime_seconds_total %u\n",
                    uptime_sec);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x2);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_uptime_seconds_total\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    /* Temp and rails */
    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_internal_temperature_celsius Internal temperature.\n"
                    "# TYPE energis_internal_temperature_celsius gauge\n"
                    "energis_internal_temperature_celsius %.3f\n",
                    temp_c);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x3);
        ERROR_PRINT_CODE(errorcode,
                         "%s Metrics buffer overflow on energis_internal_temperature_celsius\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_temp_calibrated 1 if temperature calibration is applied.\n"
                    "# TYPE energis_temp_calibrated gauge\n"
                    "energis_temp_calibrated %d\n",
                    calibrated);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x4);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_temp_calibrated\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_temp_calibration_mode Calibration mode: 0=none, 1=1pt, 2=2pt.\n"
                    "# TYPE energis_temp_calibration_mode gauge\n"
                    "energis_temp_calibration_mode %u\n",
                    (unsigned)cal_mode);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x5);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_temp_calibration_mode\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_vusb_volts USB rail voltage.\n"
                    "# TYPE energis_vusb_volts gauge\n"
                    "energis_vusb_volts %.3f\n",
                    v_usb);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x6);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_vusb_volts\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_vsupply_volts 12V supply rail voltage.\n"
                    "# TYPE energis_vsupply_volts gauge\n"
                    "energis_vsupply_volts %.3f\n",
                    v_12v);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x7);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_vsupply_volts\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }
    net_beat();

    /* HTTP counter */
    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_http_requests_total Total HTTP requests served.\n"
                    "# TYPE energis_http_requests_total counter\n"
                    "energis_http_requests_total %u\n",
                    g_metrics.http_requests_total);
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x8);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_http_requests_total\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }
    net_beat();

    /* Relay state */
    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_channel_state Relay state (1=ON, 0=OFF).\n"
                    "# TYPE energis_channel_state gauge\n");
    if (pos >= bufsize)
        return -1;

    for (int ch = 0; ch < 8; ch++) {
        bool state = mcp_get_channel_state((uint8_t)ch);
        pos += snprintf(metrics_buffer + pos, bufsize - pos,
                        "energis_channel_state{ch=\"%d\"} %d\n", ch + 1, state ? 1 : 0);
        if (pos >= bufsize) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0x9);
            ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_channel_state\n",
                             METRICS_HANDLER_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif

            return -1;
        }
    }
    net_beat();

    /* Telemetry valid */
    pos += snprintf(
        metrics_buffer + pos, bufsize - pos,
        "# HELP energis_channel_telemetry_valid 1 if telemetry for channel is fresh/valid.\n"
        "# TYPE energis_channel_telemetry_valid gauge\n");
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xA);
        ERROR_PRINT_CODE(errorcode,
                         "%s Metrics buffer overflow on energis_channel_telemetry_valid\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    for (int ch = 0; ch < 8; ch++) {
        meter_telemetry_t telem = (meter_telemetry_t){0};
        (void)MeterTask_GetTelemetry((uint8_t)ch, &telem);
        pos += snprintf(metrics_buffer + pos, bufsize - pos,
                        "energis_channel_telemetry_valid{ch=\"%d\"} %d\n", ch + 1,
                        telem.valid ? 1 : 0);
        if (pos >= bufsize) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xA);
            ERROR_PRINT_CODE(errorcode,
                             "%s Metrics buffer overflow on energis_channel_telemetry_valid\n",
                             METRICS_HANDLER_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif

            return -1;
        }
    }
    net_beat();

    /* Per-channel V/I/P/E */
    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_channel_voltage_volts Channel voltage.\n"
                    "# TYPE energis_channel_voltage_volts gauge\n");
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xB);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_channel_voltage_volts\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    for (int ch = 0; ch < 8; ch++) {
        meter_telemetry_t telem = (meter_telemetry_t){0};
        float V =
            (MeterTask_GetTelemetry((uint8_t)ch, &telem) && telem.valid) ? telem.voltage : 0.0f;
        pos += snprintf(metrics_buffer + pos, bufsize - pos,
                        "energis_channel_voltage_volts{ch=\"%d\"} %.3f\n", ch + 1, V);
        if (pos >= bufsize) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xB);
            ERROR_PRINT_CODE(errorcode,
                             "%s Metrics buffer overflow on energis_channel_voltage_volts\n",
                             METRICS_HANDLER_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif

            return -1;
        }
    }
    net_beat();

    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_channel_current_amps Channel current.\n"
                    "# TYPE energis_channel_current_amps gauge\n");
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xC);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_channel_current_amps\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    for (int ch = 0; ch < 8; ch++) {
        meter_telemetry_t telem = (meter_telemetry_t){0};
        float I =
            (MeterTask_GetTelemetry((uint8_t)ch, &telem) && telem.valid) ? telem.current : 0.0f;
        pos += snprintf(metrics_buffer + pos, bufsize - pos,
                        "energis_channel_current_amps{ch=\"%d\"} %.3f\n", ch + 1, I);
        if (pos >= bufsize) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xC);
            ERROR_PRINT_CODE(errorcode,
                             "%s Metrics buffer overflow on energis_channel_current_amps\n",
                             METRICS_HANDLER_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif

            return -1;
        }
    }
    net_beat();

    pos += snprintf(metrics_buffer + pos, bufsize - pos,
                    "# HELP energis_channel_power_watts Active power per channel.\n"
                    "# TYPE energis_channel_power_watts gauge\n");
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xD);
        ERROR_PRINT_CODE(errorcode, "%s Metrics buffer overflow on energis_channel_power_watts\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    for (int ch = 0; ch < 8; ch++) {
        meter_telemetry_t telem;
        float P = 0.0f;
        if (MeterTask_GetTelemetry((uint8_t)ch, &telem) && telem.valid)
            P = telem.power;
        pos += snprintf(metrics_buffer + pos, bufsize - pos,
                        "energis_channel_power_watts{ch=\"%d\"} %.3f\n", ch + 1, P);
        if (pos >= bufsize) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xD);
            ERROR_PRINT_CODE(errorcode,
                             "%s Metrics buffer overflow on energis_channel_power_watts\n",
                             METRICS_HANDLER_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif

            return -1;
        }
    }
    net_beat();

    pos +=
        snprintf(metrics_buffer + pos, bufsize - pos,
                 "# HELP energis_channel_energy_watt_hours_total Accumulated energy per channel.\n"
                 "# TYPE energis_channel_energy_watt_hours_total counter\n");
    if (pos >= bufsize) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xE);
        ERROR_PRINT_CODE(errorcode,
                         "%s Metrics buffer overflow on energis_channel_energy_watt_hours_total\n",
                         METRICS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return -1;
    }

    for (int ch = 0; ch < 8; ch++) {
        meter_telemetry_t telem;
        float E_wh = 0.0f;
        if (MeterTask_GetTelemetry((uint8_t)ch, &telem) && telem.valid)
            E_wh = telem.energy_kwh * 1000.0f;
        pos += snprintf(metrics_buffer + pos, bufsize - pos,
                        "energis_channel_energy_watt_hours_total{ch=\"%d\"} %.3f\n", ch + 1, E_wh);
        if (pos >= bufsize) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_METRICS, 0xE);
            ERROR_PRINT_CODE(
                errorcode,
                "%s Metrics buffer overflow on energis_channel_energy_watt_hours_total\n",
                METRICS_HANDLER_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif

            return -1;
        }
    }
    net_beat();

    return pos;
}

/**
 * @brief Send 404 Not Found response.
 *
 * @param sock Socket number
 * @return None
 *
 * @details Used when metrics feature is disabled or not available.
 */
static void send_404(uint8_t sock) {
    const char *body = "404 Not Found\n";
    int body_len = (int)strlen(body);

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 404 Not Found\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              body_len);
    if (header_len < 0 || header_len >= (int)sizeof(header))
        return;

    send(sock, (uint8_t *)header, header_len);
    net_beat();
    send(sock, (uint8_t *)body, body_len);
    net_beat();
}

/**
 * @brief Send 503 Service Unavailable response.
 *
 * @param sock Socket number
 * @return None
 *
 * @details Used when rendering fails (e.g., buffer overflow) to hint retry.
 */
static void send_503(uint8_t sock) {
    const char *body = "503 Service Unavailable\n";
    int body_len = (int)strlen(body);

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 503 Service Unavailable\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              body_len);
    if (header_len < 0 || header_len >= (int)sizeof(header))
        return;

    send(sock, (uint8_t *)header, header_len);
    net_beat();
    send(sock, (uint8_t *)body, body_len);
    net_beat();
}

/**
 * @brief Handle HTTP GET request to /metrics endpoint.
 *
 * @param sock Socket number for the established HTTP connection
 *
 * @return None
 *
 * @details Checks if metrics feature is enabled (compile-time via
 * CFG_ENABLE_METRICS). If disabled, returns 404. Otherwise, renders
 * OpenMetrics text and sends 200 response with proper Content-Type.
 *
 * @http
 * - 200 OK with text/plain; version=0.0.4; charset=utf-8 on success
 * - 404 Not Found if feature disabled
 */
void handle_metrics_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_metrics_request()\n");
    net_beat();

#ifndef CFG_ENABLE_METRICS
    send_404(sock);
    return;
#else
#if (CFG_ENABLE_METRICS == 0)
    send_404(sock);
    return;
#endif
#endif

    int body_len = render_metrics();
    if (body_len < 0) {
        ERROR_PRINT("%s Buffer overflow during metrics render\n", METRICS_HANDLER_TAG);
        send_503(sock); /* CHANGED: return 503 instead of 404 on overflow */
        return;
    }

    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
                              "Content-Length: %d\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              body_len);
    if (header_len < 0)
        return;
    if (header_len >= (int)sizeof(header))
        header_len = (int)sizeof(header);

    send(sock, (uint8_t *)header, header_len);
    net_beat();

    if (body_len > 0) {
        send(sock, (uint8_t *)metrics_buffer, body_len);
        net_beat();
    }

    INFO_PRINT("%s Served %d bytes of metrics\n", METRICS_HANDLER_TAG, body_len);
}
