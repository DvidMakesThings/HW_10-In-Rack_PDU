/**
 * @file settings_handler.c
 * @author DvidMakesThings - David Sipos
 * @defgroup webui4 4. Settings Handler
 * @ingroup webui
 * @brief Handler for the page control.html
 * @{
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "settings_handler.h"
#include "CONFIG.h"

/**
 * @brief Handles the HTTP request for the settings page.
 * @param sock The socket number.
 * @note This function is called when the user interacts with the settings page.
 */
void handle_settings_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_settings_request()\n");

    // --- 1) Load network from EEPROM --------------------------------------
    networkInfo net;
    EEPROM_ReadUserNetworkWithChecksum(&net);
    NETLOG_PRINT("Network EEPROM: IP=%u.%u.%u.%u  GW=%u.%u.%u.%u\n", net.ip[0], net.ip[1],
                 net.ip[2], net.ip[3], net.gw[0], net.gw[1], net.gw[2], net.gw[3]);

    // --- 2) Load prefs from EEPROM ----------------------------------------
    userPrefInfo pref;
    EEPROM_ReadUserPrefsWithChecksum(&pref);
    NETLOG_PRINT("Prefs EEPROM: device_name=\"%s\"  location=\"%s\"  unit=%u\n", pref.device_name,
                 pref.location, pref.temp_unit);

    // --- 3) Read the internal temp the same way as your status handler ----
    adc_select_input(4);
    NETLOG_PRINT("Selecting ADC input 4 for temp sensor\n");
    int raw = adc_read();
    NETLOG_PRINT("Raw ADC reading = %d\n", raw);
    // convert to voltage (use actual VREF you measured, e.g. 3.0V)
    const float VREF = 3.00f;
    float voltage = (raw * VREF) / (1 << 12);
    NETLOG_PRINT("Converted voltage = %.3fV\n", voltage);
    // RP2040 internal formula: Temp (°C) = 27 – (voltage – 0.706)/0.001721
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;
    NETLOG_PRINT("Calculated temp_c = %.2f °C\n", temp_c);

    // --- 4) Convert to user’s unit ----------------------------------------
    float temp_out;
    const char *unit_suffix;
    switch (pref.temp_unit) {
    case 1: // Fahrenheit
        temp_out = temp_c * 9.0f / 5.0f + 32.0f;
        unit_suffix = "°F";
        break;
    case 2: // Kelvin
        temp_out = temp_c + 273.15f;
        unit_suffix = "K";
        break;
    default: // Celsius
        temp_out = temp_c;
        unit_suffix = "°C";
    }
    NETLOG_PRINT("Displaying temp = %.2f%s\n", temp_out, unit_suffix);

    // --- 5) Build dotted‐decimal strings for network -----------------------
    char ip_s[16], gw_s[16], sn_s[16], dns_s[16];
    snprintf(ip_s, sizeof(ip_s), "%u.%u.%u.%u", net.ip[0], net.ip[1], net.ip[2], net.ip[3]);
    snprintf(gw_s, sizeof(gw_s), "%u.%u.%u.%u", net.gw[0], net.gw[1], net.gw[2], net.gw[3]);
    snprintf(sn_s, sizeof(sn_s), "%u.%u.%u.%u", net.sn[0], net.sn[1], net.sn[2], net.sn[3]);
    snprintf(dns_s, sizeof(dns_s), "%u.%u.%u.%u", net.dns[0], net.dns[1], net.dns[2], net.dns[3]);

    // --- 6) Prepare prefs strings -----------------------------------------
    char name_s[32], loc_s[32];
    strncpy(name_s, pref.device_name, sizeof(name_s) - 1);
    name_s[sizeof(name_s) - 1] = '\0';
    strncpy(loc_s, pref.location, sizeof(loc_s) - 1);
    loc_s[sizeof(loc_s) - 1] = '\0';

    // --- 7) Radio‐button flags ---------------------------------------------
    char c_chk[8] = "", f_chk[8] = "", k_chk[8] = "";
    switch (pref.temp_unit) {
    case 0:
        strcpy(c_chk, "checked");
        break;
    case 1:
        strcpy(f_chk, "checked");
        break;
    case 2:
        strcpy(k_chk, "checked");
        break;
    }

    // --- 8) Render the HTML template with two new placeholders at the end:
    //      %0.2f for the numeric temperature, and %s for the suffix
    char page[4096];
    int len = snprintf(page, sizeof(page), settings_html,
                       /* network (4) */ ip_s, gw_s, sn_s, dns_s,
                       /* device prefs (2)*/ name_s, loc_s,
                       /* radios (3)*/ c_chk, f_chk, k_chk,
                       /* temp display (2)*/ temp_out, unit_suffix);
    NETLOG_PRINT("Rendered HTML len=%d\n", len);

    // --- 9) Send HTTP headers + body --------------------------------------
    char hdr[128];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n",
                        len);
    NETLOG_PRINT("Sending header len=%d\n", hlen);
    send(sock, (uint8_t *)hdr, hlen);
    send(sock, (uint8_t *)page, len);

    NETLOG_PRINT("<< handle_settings_request() done\n");
}

/**
 * @brief Handles the HTTP POST request for the settings page.
 * @param sock The socket number.
 * @param body The body of the POST request.
 * @note This function is called when the user submits the settings form.
 */
void handle_settings_post(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_settings_post()\n");
    NETLOG_PRINT("Raw POST body: \"%s\"\n", body);

    // --- 1) Update network config ------------------------------------------
    networkInfo net = LoadUserNetworkConfig();
    networkInfo backup_net = net;
    char buf[64];
    char *tmp;

    // IP
    if ((tmp = get_form_value(body, "ip"))) {
        NETLOG_PRINT("Raw ip=\"%s\"\n", tmp);
        urldecode(tmp);
        NETLOG_PRINT("Decoded ip=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.ip[0] = a;
            net.ip[1] = b;
            net.ip[2] = c;
            net.ip[3] = d;
            NETLOG_PRINT("  -> new IP=%u.%u.%u.%u\n", a, b, c, d);
        }
    }
    // Gateway
    if ((tmp = get_form_value(body, "gateway"))) {
        NETLOG_PRINT("Raw gateway=\"%s\"\n", tmp);
        urldecode(tmp);
        NETLOG_PRINT("Decoded gateway=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.gw[0] = a;
            net.gw[1] = b;
            net.gw[2] = c;
            net.gw[3] = d;
            NETLOG_PRINT("  -> new GW=%u.%u.%u.%u\n", a, b, c, d);
        }
    }
    // Subnet
    if ((tmp = get_form_value(body, "subnet"))) {
        NETLOG_PRINT("Raw subnet=\"%s\"\n", tmp);
        urldecode(tmp);
        NETLOG_PRINT("Decoded subnet=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.sn[0] = a;
            net.sn[1] = b;
            net.sn[2] = c;
            net.sn[3] = d;
            NETLOG_PRINT("  -> new SN=%u.%u.%u.%u\n", a, b, c, d);
        }
    }
    // DNS
    if ((tmp = get_form_value(body, "dns"))) {
        NETLOG_PRINT("Raw dns=\"%s\"\n", tmp);
        urldecode(tmp);
        NETLOG_PRINT("Decoded dns=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.dns[0] = a;
            net.dns[1] = b;
            net.dns[2] = c;
            net.dns[3] = d;
            NETLOG_PRINT("  -> new DNS=%u.%u.%u.%u\n", a, b, c, d);
        }
    }

    // Write network only if changed
    if (memcmp(&net, &backup_net, sizeof(net)) != 0) {
        NETLOG_PRINT("Network changed; writing EEPROM\n");
        EEPROM_WriteUserNetworkWithChecksum(&net);
        sleep_ms(10);
        wiz_NetInfo wiz = {
            .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
            .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
            .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
            .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
            .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
            .dhcp = net.dhcp};
        wizchip_setnetinfo(&wiz);
        NETLOG_PRINT("Network reconfigured\n");
    } else {
        NETLOG_PRINT("Network unchanged; skipping EEPROM write\n");
    }

    // --- 2) Update user preferences ----------------------------------------
    userPrefInfo pref = LoadUserPreferences();
    userPrefInfo backup_pref = pref;

    // Device Name
    if ((tmp = get_form_value(body, "device_name"))) {
        NETLOG_PRINT("Raw device_name=\"%s\"\n", tmp);
        urldecode(tmp);
        NETLOG_PRINT("Decoded device_name=\"%s\"\n", tmp);
        strncpy(pref.device_name, tmp, sizeof(pref.device_name) - 1);
        pref.device_name[sizeof(pref.device_name) - 1] = '\0';
    }
    // Location
    if ((tmp = get_form_value(body, "location"))) {
        NETLOG_PRINT("Raw location=\"%s\"\n", tmp);
        urldecode(tmp);
        NETLOG_PRINT("Decoded location=\"%s\"\n", tmp);
        strncpy(pref.location, tmp, sizeof(pref.location) - 1);
        pref.location[sizeof(pref.location) - 1] = '\0';
    }
    // Temp Unit
    if ((tmp = get_form_value(body, "temp_unit"))) {
        NETLOG_PRINT("Raw temp_unit=\"%s\"\n", tmp);
        urldecode(tmp);
        NETLOG_PRINT("Decoded temp_unit=\"%s\"\n", tmp);
        if (!strcmp(tmp, "fahrenheit"))
            pref.temp_unit = 1;
        else if (!strcmp(tmp, "kelvin"))
            pref.temp_unit = 2;
        else
            pref.temp_unit = 0;
        NETLOG_PRINT("  -> new temp_unit=%u\n", pref.temp_unit);
    }

    // Write prefs only if changed
    if (memcmp(&pref, &backup_pref, sizeof(pref)) != 0) {
        NETLOG_PRINT("Prefs changed; writing EEPROM\n");
        EEPROM_WriteUserPrefsWithChecksum(&pref);
        sleep_ms(10);
    } else {
        NETLOG_PRINT("Prefs unchanged; skipping EEPROM write\n");
    }

    // --- 3) Send 204 No Content then reboot -------------------------------
    const char *resp = "HTTP/1.1 204 No Content\r\n"
                       "Connection: close\r\n"
                       "\r\n";
    send(sock, (uint8_t *)resp, strlen(resp));
    sleep_ms(100);
    watchdog_reboot(0, 0, 0);
}
/**
 * @}
 */