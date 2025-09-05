/**
 * @file status_handler.c
 * @author DvidMakesThings - David Sipos
 * @defgroup webui5 5. Status Handler
 * @ingroup webui
 * @brief Handler for building JSON for /api/status
 * @{
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "status_handler.h"
#include "CONFIG.h"

/**
 * @brief Handles the HTTP request for the status page.
 * @param sock The socket number.
 * @note This function is called when the user interacts with the status page.
 *
 * CHANGE SUMMARY (fix slider delay):
 *  - Sliders showed a 5 s lag because "state" was coming from the HLW refresh cache,
 *    which is only updated once per POLL_INTERVAL_MS. To make the UI reflect the
 *    switch instantly, this version reads the relay state LIVE from the MCP expander
 *    (mcp_relay_read_pin()) for each channel, while still using CACHED HLW values
 *    for Voltage/Current/Power. This decouples UI immediacy from measurement cadence.
 */
void handle_status_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_status_request()\n");

    /* --- 1) Internal temperature (fast ADC) ----------------------------- */
    NETLOG_PRINT("Selecting ADC input 4 for temp sensor\n");
    adc_select_input(4);
    uint16_t raw = adc_read();
    NETLOG_PRINT("Raw ADC reading = %u\n", raw);

    float v_die = raw * 3.0f / 4096.0f;
    float temp_c = 27.0f - (v_die - 0.706f) / 0.001721f;
    // PLOT("temp_sensor_voltage: %.2f\n", v_die);
    PLOT("temp_sensor_temperature: %.2f\n", temp_c);

    /* --- 2) Unit preference -------------------------------------------- */
    userPrefInfo pref;
    if (EEPROM_ReadUserPrefsWithChecksum(&pref) != 0) {
        NETLOG_PRINT("User preferences are:  %s, %s, %u\n", pref.device_name, pref.location,
                     pref.temp_unit);
        NETLOG_PRINT("Failed to read prefs; defaulting to Celsius\n");
        pref.temp_unit = 0;
    }
    NETLOG_PRINT("temp_unit pref = %u (0=C,1=F,2=K)\n", pref.temp_unit);
    DEBUG_PRINT("User preferences are:  %s, %s, %u\n", pref.device_name, pref.location,
                pref.temp_unit);
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

    /* --- 3) Build JSON: STATE = LIVE (instant sliders), MEAS = CACHED --- */
    char json[1024];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{ \"channels\": [");

    for (int i = 0; i < 8; i++) {
        /* Live relay state → instant UI feedback after All On/Off */
        bool state = mcp_relay_read_pin((uint8_t)i);

        /* Measurement values are non‑blocking cached snapshots */
        float V = hlw8032_cached_voltage((uint8_t)i);
        float I = hlw8032_cached_current((uint8_t)i);
        float P = hlw8032_cached_power((uint8_t)i);
        uint32_t up = hlw8032_cached_uptime((uint8_t)i);

        PLOT("voltage%d_Channel%d: %.2f\n", i + 1, i + 1, V);
        // PLOT("current%d_Channel%d: %.2f\n", i + 1, i + 1, I);

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

    /* --- 4) Send HTTP response ----------------------------------------- */
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