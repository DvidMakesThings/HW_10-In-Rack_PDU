/**
 * @file status_handler.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui4 4. Status Handler
 * @ingroup webhandlers
 * @brief Handler for building JSON for /api/status
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-08
 *
 * @details Handles GET requests to /api/status endpoint. Returns JSON with
 *          channel states (live from MCP), voltage/current/power (cached from
 *          MeterTask), internal temperature, and system status. Decouples UI
 *          immediacy from measurement cadence.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define STATUS_HANDLER_TAG "<Status Handler>"

/**
 * @brief Handles the HTTP request for the status page
 * @param sock The socket number
 * @return None
 * @note Returns JSON with live relay states and cached measurements.
 *       STATE = LIVE (instant UI feedback), MEAS = CACHED (non-blocking).
 */
void handle_status_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_status_request()\n");

    /* Internal temperature (fast ADC) */
    NETLOG_PRINT("Selecting ADC input 4 for temp sensor\n");
    adc_select_input(4);
    uint16_t raw = adc_read();
    NETLOG_PRINT("Raw ADC reading = %u\n", raw);

    float v_die = raw * 3.0f / 4096.0f;
    float temp_c = 27.0f - (v_die - 0.706f) / 0.001721f;
    PLOT("temp_sensor_temperature: %.2f\n", temp_c);

    /* Unit preference */
    userPrefInfo pref;
    if (EEPROM_ReadUserPrefsWithChecksum(&pref) != 0) {
        NETLOG_PRINT("Failed to read prefs; defaulting to Celsius\n");
        pref.temp_unit = 0;
    }
    NETLOG_PRINT("temp_unit pref = %u (0=C,1=F,2=K)\n", pref.temp_unit);

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
    NETLOG_PRINT("Reporting temp_value = %.2f %s\n", temp_value, unit_str);

    /* Build JSON: STATE = LIVE (instant sliders), MEAS = CACHED */
    char json[1024];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{ \"channels\": [");

    for (int i = 0; i < 8; i++) {
        /* Live relay state from MCP -> instant UI feedback */
        bool state = mcp_get_channel_state((uint8_t)i);

        /* Get cached measurements from MeterTask (non-blocking) */
        meter_telemetry_t telem;
        float V = 0.0f;
        float I = 0.0f;
        float P = 0.0f;
        uint32_t up = 0;

        if (MeterTask_GetTelemetry((uint8_t)i, &telem) && telem.valid) {
            /* Valid measurement data available */
            V = telem.voltage;
            I = telem.current;
            P = telem.power;
            up = telem.uptime;
            NETLOG_PRINT("Ch%d: V=%.2f I=%.2f P=%.2f uptime=%u valid=true\n", i, V, I, P, up);
        } else {
            /* No valid data yet - return zeros */
            NETLOG_PRINT("Ch%d: No valid measurement data\n", i);
        }

        PLOT("voltage%d_Channel%d: %.2f\n", i + 1, i + 1, V);

        pos += snprintf(json + pos, sizeof(json) - pos,
                        "{ \"voltage\": %.2f, \"current\": %.2f, \"uptime\": %u, "
                        "\"power\": %.2f, \"state\": %s }%s",
                        V, I, up, P, state ? "true" : "false", (i < 7 ? "," : ""));
    }

    pos += snprintf(json + pos, sizeof(json) - pos,
                    "], \"internalTemperature\": %.2f, "
                    "\"temperatureUnit\": \"%s\", \"systemStatus\": \"%s\" }",
                    temp_value, unit_str, "OK");

    NETLOG_PRINT("Built JSON payload (%d bytes)\n", pos);

    /* Send HTTP response */
    char header[128];
    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "\r\n",
                           pos);
    NETLOG_PRINT("Sending HTTP header (%d bytes)\n", hdr_len);
    send(sock, (uint8_t *)header, hdr_len);

    send(sock, (uint8_t *)json, pos);
    NETLOG_PRINT("Sent JSON body\n");
    NETLOG_PRINT("<< handle_status_request() done\n");
}

/**
 * @}
 */