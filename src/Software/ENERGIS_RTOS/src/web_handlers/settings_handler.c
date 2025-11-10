/**
 * @file settings_handler.c
 * @author
 *
 * @defgroup webui3 3. Settings Handler
 * @ingroup webhandlers
 * @brief Handler for the page settings.html
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Handles GET and POST requests for settings page. Reads/writes
 *          network configuration and user preferences from/to EEPROM.
 *          Triggers system reboot after settings changes.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define SETTINGS_HANDLER_TAG "<Settings Handler>"

/**
 * @brief Handles the HTTP request for the settings page (HTML)
 * @param sock The socket number
 * @return None
 * @note This function serves the settings page with current configuration values
 */
void handle_settings_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_settings_request()\n");
    net_beat();

    /* Load network from EEPROM */
    networkInfo net;
    EEPROM_ReadUserNetworkWithChecksum(&net);
    NETLOG_PRINT("Network EEPROM: IP=%u.%u.%u.%u  GW=%u.%u.%u.%u\n", net.ip[0], net.ip[1],
                 net.ip[2], net.ip[3], net.gw[0], net.gw[1], net.gw[2], net.gw[3]);
    net_beat();

    /* Load prefs from EEPROM */
    userPrefInfo pref;
    EEPROM_ReadUserPrefsWithChecksum(&pref);
    NETLOG_PRINT("Prefs EEPROM: device_name=\"%s\"  location=\"%s\"  unit=%u\n", pref.device_name,
                 pref.location, pref.temp_unit);
    net_beat();

    /* Read internal temperature */
    adc_select_input(4);
    int raw = adc_read();
    const float VREF = 3.00f;
    float voltage = (raw * VREF) / (1 << 12);
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;
    net_beat();

    /* Convert to user's unit */
    float temp_out;
    const char *unit_suffix;
    switch (pref.temp_unit) {
    case 1: /* Fahrenheit */
        temp_out = temp_c * 9.0f / 5.0f + 32.0f;
        unit_suffix = "°F";
        break;
    case 2: /* Kelvin */
        temp_out = temp_c + 273.15f;
        unit_suffix = "K";
        break;
    default: /* Celsius */
        temp_out = temp_c;
        unit_suffix = "°C";
    }

    /* Build dotted-decimal strings for network */
    char ip_s[16], gw_s[16], sn_s[16], dns_s[16];
    snprintf(ip_s, sizeof(ip_s), "%u.%u.%u.%u", net.ip[0], net.ip[1], net.ip[2], net.ip[3]);
    snprintf(gw_s, sizeof(gw_s), "%u.%u.%u.%u", net.gw[0], net.gw[1], net.gw[2], net.gw[3]);
    snprintf(sn_s, sizeof(sn_s), "%u.%u.%u.%u", net.sn[0], net.sn[1], net.sn[2], net.sn[3]);
    snprintf(dns_s, sizeof(dns_s), "%u.%u.%u.%u", net.dns[0], net.dns[1], net.dns[2], net.dns[3]);

    /* Prepare prefs strings */
    char name_s[32], loc_s[32];
    strncpy(name_s, pref.device_name, sizeof(name_s) - 1);
    name_s[sizeof(name_s) - 1] = '\0';
    strncpy(loc_s, pref.location, sizeof(loc_s) - 1);
    loc_s[sizeof(loc_s) - 1] = '\0';

    /* Radio button flags */
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

    /* Render the HTML template */
    char page[4096];
    int len = snprintf(page, sizeof(page), settings_html,
                       /* network (4) */ ip_s, gw_s, sn_s, dns_s,
                       /* device prefs (2)*/ name_s, loc_s,
                       /* radios (3)*/ c_chk, f_chk, k_chk,
                       /* temp display (2)*/ temp_out, unit_suffix);
    NETLOG_PRINT("Rendered HTML len=%d\n", len);
    net_beat();

    /* Send HTTP headers + body */
    char hdr[128];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n",
                        len);
    NETLOG_PRINT("Sending header len=%d\n", hlen);
    send(sock, (uint8_t *)hdr, hlen);
    net_beat();
    send(sock, (uint8_t *)page, len);
    net_beat();

    NETLOG_PRINT("<< handle_settings_request() done\n");
}

/**
 * @brief Handles the HTTP request for the settings API (returns JSON)
 * @param sock The socket number
 * @return None
 * @note This function is called when JavaScript requests settings data
 */
void handle_settings_api(uint8_t sock) {
    NETLOG_PRINT(">> handle_settings_api()\n");
    net_beat();

    /* Load network from EEPROM */
    networkInfo net;
    EEPROM_ReadUserNetworkWithChecksum(&net);

    /* Load prefs from EEPROM */
    userPrefInfo pref;
    EEPROM_ReadUserPrefsWithChecksum(&pref);

    /* Read the internal temp */
    adc_select_input(4);
    int raw = adc_read();
    const float VREF = 3.00f;
    float voltage = (raw * VREF) / (1 << 12);
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;

    /* Convert to user's unit */
    float temp_out;
    const char *unit_suffix;
    switch (pref.temp_unit) {
    case 1: /* Fahrenheit */
        temp_out = temp_c * 9.0f / 5.0f + 32.0f;
        unit_suffix = "fahrenheit";
        break;
    case 2: /* Kelvin */
        temp_out = temp_c + 273.15f;
        unit_suffix = "kelvin";
        break;
    default: /* Celsius */
        temp_out = temp_c;
        unit_suffix = "celsius";
    }

    /* Build JSON response */
    char json[1024];
    int len =
        snprintf(json, sizeof(json),
                 "{"
                 "\"ip\":\"%u.%u.%u.%u\","
                 "\"gateway\":\"%u.%u.%u.%u\","
                 "\"subnet\":\"%u.%u.%u.%u\","
                 "\"dns\":\"%u.%u.%u.%u\","
                 "\"device_name\":\"%s\","
                 "\"location\":\"%s\","
                 "\"temp_unit\":\"%s\","
                 "\"temperature\":%.2f,"
                 "\"timezone\":\"\","
                 "\"time\":\"00:00:00\","
                 "\"autologout\":5"
                 "}",
                 net.ip[0], net.ip[1], net.ip[2], net.ip[3], net.gw[0], net.gw[1], net.gw[2],
                 net.gw[3], net.sn[0], net.sn[1], net.sn[2], net.sn[3], net.dns[0], net.dns[1],
                 net.dns[2], net.dns[3], pref.device_name, pref.location, unit_suffix, temp_out);

    NETLOG_PRINT("JSON response len=%d\n", len);
    net_beat();

    /* Send HTTP headers + JSON */
    char hdr[128];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %d\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        len);
    send(sock, (uint8_t *)hdr, hlen);
    net_beat();
    send(sock, (uint8_t *)json, len);
    net_beat();

    NETLOG_PRINT("<< handle_settings_api() done\n");
}

/**
 * @brief Handles the HTTP POST request for the settings page
 * @param sock The socket number
 * @param body The body of the POST request
 * @return None
 * @note This function updates network config and user preferences, then reboots
 */
void handle_settings_post(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_settings_post()\n");
    NETLOG_PRINT("Raw POST body: \"%s\"\n", body);
    net_beat();

    /* Update network config */
    networkInfo net = LoadUserNetworkConfig();
    networkInfo backup_net = net;
    char buf[64];
    char *tmp;

    /* IP */
    if ((tmp = get_form_value(body, "ip"))) {
        urldecode(tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.ip[0] = a;
            net.ip[1] = b;
            net.ip[2] = c;
            net.ip[3] = d;
        }
    }
    /* Gateway */
    if ((tmp = get_form_value(body, "gateway"))) {
        urldecode(tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.gw[0] = a;
            net.gw[1] = b;
            net.gw[2] = c;
            net.gw[3] = d;
        }
    }
    /* Subnet */
    if ((tmp = get_form_value(body, "subnet"))) {
        urldecode(tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.sn[0] = a;
            net.sn[1] = b;
            net.sn[2] = c;
            net.sn[3] = d;
        }
    }
    /* DNS */
    if ((tmp = get_form_value(body, "dns"))) {
        urldecode(tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.dns[0] = a;
            net.dns[1] = b;
            net.dns[2] = c;
            net.dns[3] = d;
        }
    }
    net_beat();

    /* Write network only if changed */
    if (memcmp(&net, &backup_net, sizeof(net)) != 0) {
        EEPROM_WriteUserNetworkWithChecksum(&net);
        vTaskDelay(pdMS_TO_TICKS(10));
        net_beat();

        w5500_NetConfig w5500_net = {
            .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
            .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
            .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
            .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
            .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
            .dhcp = net.dhcp};
        w5500_set_network(&w5500_net);
        net_beat();
    }

    /* Update user preferences */
    userPrefInfo pref = LoadUserPreferences();
    userPrefInfo backup_pref = pref;

    if ((tmp = get_form_value(body, "device_name"))) {
        urldecode(tmp);
        strncpy(pref.device_name, tmp, sizeof(pref.device_name) - 1);
        pref.device_name[sizeof(pref.device_name) - 1] = '\0';
    }
    if ((tmp = get_form_value(body, "location"))) {
        urldecode(tmp);
        strncpy(pref.location, tmp, sizeof(pref.location) - 1);
        pref.location[sizeof(pref.location) - 1] = '\0';
    }
    if ((tmp = get_form_value(body, "temp_unit"))) {
        urldecode(tmp);
        if (!strcmp(tmp, "fahrenheit"))
            pref.temp_unit = 1;
        else if (!strcmp(tmp, "kelvin"))
            pref.temp_unit = 2;
        else
            pref.temp_unit = 0;
    }

    if (memcmp(&pref, &backup_pref, sizeof(pref)) != 0) {
        EEPROM_WriteUserPrefsWithChecksum(&pref);
        vTaskDelay(pdMS_TO_TICKS(10));
        net_beat();
    }

    /* Send 204 No Content then reboot */
    const char *resp = "HTTP/1.1 204 No Content\r\n"
                       "Connection: close\r\n"
                       "\r\n";
    send(sock, (uint8_t *)resp, strlen(resp));
    net_beat();

    vTaskDelay(pdMS_TO_TICKS(100));
    net_beat();
    Health_RebootNow("HTTP settings applied");
}

/**
 * @}
 */
