/**
 * @file status_handler.c
 * @author DvidMakesThings - David Sipos
 * @brief Handler for building JSON for /api/status
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
 */
void handle_status_request(uint8_t sock) {
    INFO_PRINT(">> handle_status_request()\n");

    // --- 1) Read raw die temperature in °C via ADC4 -----------------------
    DEBUG_PRINT("Selecting ADC input 4 for temp sensor\n");
    adc_select_input(4);
    uint16_t raw = adc_read();
    DEBUG_PRINT("Raw ADC reading = %u\n", raw);

    // Convert raw→voltage (use your true Vref here, e.g. 3.0f or 3.3f)
    float voltage = raw * 3.0f / 4096.0f;
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;
    DEBUG_PRINT("Calculated temp_c = %.2f °C\n", temp_c);

    // --- 2) Load user preference for unit -------------------------------
    userPrefInfo pref;
    if (EEPROM_ReadUserPrefsWithChecksum(&pref) != 0) {
        DEBUG_PRINT("Failed to read prefs; defaulting to Celsius\n");
        pref.temp_unit = 0;
    }
    DEBUG_PRINT("temp_unit pref = %u (0=C,1=F,2=K)\n", pref.temp_unit);

    // --- 3) Convert °C→selected unit and pick the unit string -----------
    float temp_value;
    const char *unit_str;
    switch (pref.temp_unit) {
    case 1: // Fahrenheit
        temp_value = temp_c * 9.0f / 5.0f + 32.0f;
        unit_str = "°F";
        break;
    case 2: // Kelvin
        temp_value = temp_c + 273.15f;
        unit_str = "K";
        break;
    case 0:
    default: // Celsius
        temp_value = temp_c;
        unit_str = "°C";
        break;
    }
    DEBUG_PRINT("Reporting temp_value = %.2f %s\n", temp_value, unit_str);

    // --- 4) Build JSON payload with both value & unit --------------------
    char json[1024];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{ \"channels\": [");
    for (int i = 0; i < 8; i++) {
        // your actual channel readings here…
        float V = 230.0f;
        float I = 2.1f;
        uint32_t U = 32000;
        float P = V * I;
        bool state = mcp_relay_read_pin(i);

        pos += snprintf(json + pos, sizeof(json) - pos,
                        "{ \"voltage\": %.2f, \"current\": %.2f, \"uptime\": %u, "
                        "\"power\": %.2f, \"state\": %s }%s",
                        V, I, U, P, state ? "true" : "false", (i < 7 ? "," : ""));
    }
    pos += snprintf(json + pos, sizeof(json) - pos,
                    "], "
                    "\"internalTemperature\": %.2f, "
                    "\"temperatureUnit\": \"%s\", "
                    "\"systemStatus\": \"%s\" }",
                    temp_value, unit_str, "OK");
    DEBUG_PRINT("Built JSON payload (%d bytes)\n", pos);

    // --- 5) Send HTTP headers --------------------------------------------
    char header[128];
    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "\r\n",
                           pos);
    DEBUG_PRINT("Sending HTTP header (%d bytes)\n", hdr_len);
    send(sock, (uint8_t *)header, hdr_len);

    // --- 6) Send JSON body -----------------------------------------------
    send(sock, (uint8_t *)json, pos);
    DEBUG_PRINT("Sent JSON body\n");
    INFO_PRINT("<< handle_status_request() done\n");
}