/**
 * @file src/web_handlers/status_handler.c
 * @author
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Handles GET requests to /api/status endpoint. Returns JSON with
 * channel states (live from MCP), voltage/current/power (cached from
 * MeterTask), internal temperature, and system status. Decouples UI
 * immediacy from measurement cadence.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define STATUS_HANDLER_TAG "<Status Handler>"

/**
 * @brief Handles the HTTP request for the status page (/api/status)
 * @param sock The socket number
 * @return None
 *
 * @details Returns JSON with:
 *  - channels[0..7]: { voltage, current, uptime, power, state }
 *  - internalTemperature, temperatureUnit ("°C"|"°F"|"K"), systemStatus ("OK")
 *
 * @http
 * - 200 OK on success with JSON body and Connection: close.
 */
void handle_status_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_status_request()\n");
    net_beat();

    /* Internal temperature (ADC) */
    adc_select_input(4);
    uint16_t raw = adc_read();

    float v_die = raw * 3.0f / 4096.0f;
    float temp_c = 27.0f - (v_die - 0.706f) / 0.001721f;

    /* Unit preference */
    userPrefInfo pref;
    if (EEPROM_ReadUserPrefsWithChecksum(&pref) != 0) {
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

    /* Build JSON */
    char json[1024];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{ \"channels\": [");

    for (int i = 0; i < 8; i++) {
        bool state = mcp_get_channel_state((uint8_t)i);

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
                        "{ \"voltage\": %.2f, \"current\": %.2f, \"uptime\": %u, "
                        "\"power\": %.2f, \"state\": %s }%s",
                        V, I, up, P, state ? "true" : "false", (i < 7 ? "," : ""));
        if ((i & 1) == 0)
            net_beat();
    }

    pos += snprintf(json + pos, sizeof(json) - pos,
                    "], \"internalTemperature\": %.2f, "
                    "\"temperatureUnit\": \"%s\", \"systemStatus\": \"%s\" }",
                    temp_value, unit_str, "OK");

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
