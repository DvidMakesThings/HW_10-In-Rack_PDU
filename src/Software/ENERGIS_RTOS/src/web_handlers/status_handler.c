/**
 * @file src/web_handlers/status_handler.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details Handles GET requests to /api/status endpoint. Returns JSON with
 * channel states (cached from SwitchTask), voltage/current/power (cached from
 * MeterTask), channel labels (from RAM cache), internal temperature, and system status.
 * Decouples UI immediacy from measurement cadence.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define STATUS_HANDLER_TAG "<Status Handler>"

/**
 * @brief Convert overcurrent state enum to string.
 *
 * @param state Overcurrent protection state
 * @return Constant string representation
 */
static const char *oc_state_to_string(overcurrent_state_t state) {
    switch (state) {
    case OC_STATE_NORMAL:
        return "NORMAL";
    case OC_STATE_WARNING:
        return "WARNING";
    case OC_STATE_CRITICAL:
        return "CRITICAL";
    case OC_STATE_LOCKOUT:
        return "LOCKOUT";
    default:
        return "UNKNOWN";
    }
}

/**
 * @brief Escape a string for JSON output.
 *
 * Handles special characters that need escaping in JSON strings.
 * Copies src to dst with proper escaping, null-terminates result.
 *
 * @param dst Destination buffer
 * @param dst_len Size of destination buffer
 * @param src Source string to escape
 */
static void json_escape_string(char *dst, size_t dst_len, const char *src) {
    if (!dst || dst_len == 0)
        return;

    size_t di = 0;
    for (size_t si = 0; src[si] != '\0' && di + 1 < dst_len; si++) {
        char c = src[si];
        switch (c) {
        case '"':
        case '\\':
            if (di + 2 < dst_len) {
                dst[di++] = '\\';
                dst[di++] = c;
            }
            break;
        case '\n':
            if (di + 2 < dst_len) {
                dst[di++] = '\\';
                dst[di++] = 'n';
            }
            break;
        case '\r':
            if (di + 2 < dst_len) {
                dst[di++] = '\\';
                dst[di++] = 'r';
            }
            break;
        case '\t':
            if (di + 2 < dst_len) {
                dst[di++] = '\\';
                dst[di++] = 't';
            }
            break;
        default:
            /* Only include printable ASCII characters */
            if (c >= 0x20 && c <= 0x7E) {
                dst[di++] = c;
            }
            break;
        }
    }
    dst[di] = '\0';
}

/**
 * @brief Handles the HTTP request for the status page (/api/status)
 *
 * @param sock The socket number
 *
 * @details Returns JSON with:
 *  - channels[0..7]: { voltage, current, uptime, power, state, label }
 *  - internalTemperature, temperatureUnit ("°C"|"°F"|"K")
 *  - systemStatus ("OK"|"WARNING"|"LOCKOUT")
 *  - overcurrent: { state, total_current_a, limit_a, warning_threshold_a,
 *                   critical_threshold_a, switching_allowed, trip_count, region }
 *
 * Uses cached die temperature from MeterTask (non-blocking). No direct ADC access here.
 * Channel labels are fetched from RAM cache (non-blocking).
 *
 * @http
 * - 200 OK on success with JSON body and Connection: close.
 */
void handle_status_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_status_request()\n");
    net_beat();

    /* Internal temperature (cached from MeterTask) */
    system_telemetry_t sys_tele = {0};
    bool tele_ok = MeterTask_GetSystemTelemetry(&sys_tele);
    float temp_c = tele_ok ? sys_tele.die_temp_c : 0.0f;

    /* Unit preference */
    userPrefInfo pref;
    if (!storage_get_prefs(&pref)) {
        pref.temp_unit = 0;
    }

    float temp_value;
    const char *unit_str;
    switch (pref.temp_unit) {
    case 1:
        temp_value = temp_c * 9.0f / 5.0f + 32.0f;
        unit_str = "°F";
        break;
    case 2:
        temp_value = temp_c + 273.15f;
        unit_str = "K";
        break;
    default:
        temp_value = temp_c;
        unit_str = "°C";
        break;
    }

    /* Snapshot all channel states once from SwitchTask cache */
    uint8_t state_mask = 0u;
    (void)Switch_GetAllStates(&state_mask);

    /* Get overcurrent protection status */
    overcurrent_status_t oc_status = {0};
    (void)Overcurrent_GetStatus(&oc_status);
    const char *oc_state_str = oc_state_to_string(oc_status.state);

    /* Determine overall system status based on overcurrent state */
    const char *system_status_str;
    switch (oc_status.state) {
    case OC_STATE_LOCKOUT:
        system_status_str = "LOCKOUT";
        break;
    case OC_STATE_WARNING:
    case OC_STATE_CRITICAL:
        system_status_str = "WARNING";
        break;
    default:
        system_status_str = "OK";
        break;
    }

    net_beat();

    /* Get all channel labels from RAM cache (non-blocking) */
    char labels[8][32];
    char escaped_labels[8][64];
    for (uint8_t i = 0; i < 8; i++) {
        labels[i][0] = '\0';
        escaped_labels[i][0] = '\0';

        /* Read from RAM cache - no EEPROM access */
        (void)storage_get_channel_label(i, labels[i], sizeof(labels[i]));

        /* Escape for JSON safety */
        json_escape_string(escaped_labels[i], sizeof(escaped_labels[i]), labels[i]);
    }

    net_beat();

    /* Build JSON - use larger buffer to accommodate labels and overcurrent data */
    char json[2048];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{\"channels\":[");

    for (int i = 0; i < 8; i++) {
        bool state = (state_mask & (1u << i)) != 0u;

        meter_telemetry_t telem;
        float V = 0.0f, I = 0.0f, P = 0.0f;
        uint32_t up = 0;

        if (MeterTask_GetTelemetry((uint8_t)i, &telem) && telem.valid) {
            V = telem.voltage;
            I = telem.current;
            P = telem.power;
            up = telem.uptime;
        }

        pos += snprintf(json + pos, sizeof(json) - pos,
                        "{\"voltage\":%.2f,\"current\":%.2f,\"uptime\":%lu,"
                        "\"power\":%.2f,\"state\":%s,\"label\":\"%s\"}%s",
                        V, I, (unsigned long)up, P, state ? "true" : "false", escaped_labels[i],
                        (i < 7 ? "," : ""));
        if ((i & 1) == 0)
            net_beat();
    }

    /* Add temperature, system status, and overcurrent protection status */
    pos +=
        snprintf(json + pos, sizeof(json) - pos,
                 "],\"internalTemperature\":%.2f,"
                 "\"temperatureUnit\":\"%s\","
                 "\"systemStatus\":\"%s\","
                 "\"overcurrent\":{"
                 "\"state\":\"%s\","
                 "\"total_current_a\":%.2f,"
                 "\"limit_a\":%.1f,"
                 "\"warning_threshold_a\":%.2f,"
                 "\"critical_threshold_a\":%.2f,"
                 "\"recovery_threshold_a\":%.2f,"
                 "\"switching_allowed\":%s,"
                 "\"trip_count\":%lu,"
#if ENERGIS_EU_VERSION
                 "\"region\":\"EU\""
#else
                 "\"region\":\"US\""
#endif
                 "}}",
                 temp_value, unit_str, system_status_str, oc_state_str, oc_status.total_current_a,
                 oc_status.limit_a, oc_status.warning_threshold_a, oc_status.critical_threshold_a,
                 oc_status.recovery_threshold_a, oc_status.switching_allowed ? "true" : "false",
                 (unsigned long)oc_status.trip_count);

    net_beat();

    /* Send HTTP response */
    char header[160];
    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Cache-Control: no-cache\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           pos);
    send(sock, (uint8_t *)header, hdr_len);
    net_beat();

    send(sock, (uint8_t *)json, pos);
    net_beat();

    NETLOG_PRINT("<< handle_status_request() done\n");
}