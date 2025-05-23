/**
 * @file api_handlers.c
 * @author David Sipos
 * @brief API endpoint handlers for the ENERGIS PDU web interface
 * @version 1.0
 * @date 2025-05-19
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include <ctype.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../CONFIG.h"
#include "../utils/EEPROM_MemoryMap.h"
#include "api_handlers.h"
#include "socket.h"

// Helper function to extract form values
static char *get_form_value(const char *body, const char *key) {
    static char value[64];
    char search[32];
    char *start, *end;
    snprintf(search, sizeof(search), "%s=", key);
    start = strstr(body, search);
    if (!start) {
        DEBUG_PRINT("Field \"%s\" not found in body\n", key);
        return NULL;
    }
    start += strlen(search);
    end = strstr(start, "&");
    if (!end)
        end = start + strlen(start);
    size_t len = end - start;
    if (len >= sizeof(value))
        len = sizeof(value) - 1;
    strncpy(value, start, len);
    value[len] = '\0';
    DEBUG_PRINT("Parsed \"%s\" = \"%s\"\n", key, value);
    return value;
}

// quick hex‐to‐value or –1 if not hex
static inline int hexval(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

// Decode '+' → ' ' and "%XX" → byte, in‐place
static void urldecode(char *s) {
    char *dst = s, *src = s;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (src[0] == '%' && hexval(src[1]) >= 0 && hexval(src[2]) >= 0) {
            int hi = hexval(src[1]);
            int lo = hexval(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

void handle_status_request(uint8_t sock) {
    char json[1024];
    int pos = 0;

    pos += snprintf(json + pos, sizeof(json) - pos, "{ \"channels\": [");

    for (int i = 0; i < 8; i++) {
        float V = 230.0;    // read_channel_voltage(i);
        float I = 2.1;      // read_channel_current(i);
        uint32_t U = 32000; // read_channel_uptime(i);
        float P = V * I;
        bool state = mcp_relay_read_pin(i);

        pos += snprintf(json + pos, sizeof(json) - pos,
                        "{ \"voltage\": %.2f, \"current\": %.2f, \"uptime\": %u, "
                        "\"power\": %.2f, \"state\": %s }%s",
                        V, I, U, P, state ? "true" : "false", (i < 7 ? "," : ""));
    }

    pos += snprintf(json + pos, sizeof(json) - pos,
                    "], \"internalTemperature\": %.2f, \"systemStatus\": \"%s\" }", 25.0, "OK");

    char header[128];
    int hdr_len = snprintf(header, sizeof(header),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "\r\n",
                           pos);

    send(sock, (uint8_t *)header, hdr_len);
    send(sock, (uint8_t *)json, pos);
}

void handle_settings_request(uint8_t sock) {
    DEBUG_PRINT(">> handle_settings_request()\n");

    // --- 1) Load network from EEPROM --------------------------------------
    networkInfo net;
    int r_net = EEPROM_ReadUserNetworkWithChecksum(&net);
    DEBUG_PRINT("EEPROM_ReadUserNetwork ret=%d\n", r_net);
    DEBUG_PRINT("Network EEPROM: IP=%u.%u.%u.%u  GW=%u.%u.%u.%u  SN=%u.%u.%u.%u  DNS=%u.%u.%u.%u\n",
                net.ip[0], net.ip[1], net.ip[2], net.ip[3], net.gw[0], net.gw[1], net.gw[2],
                net.gw[3], net.sn[0], net.sn[1], net.sn[2], net.sn[3], net.dns[0], net.dns[1],
                net.dns[2], net.dns[3]);

    // --- 2) Load prefs from EEPROM ----------------------------------------
    userPrefInfo pref;
    int r_pref = EEPROM_ReadUserPrefsWithChecksum(&pref);
    DEBUG_PRINT("EEPROM_ReadUserPrefs ret=%d\n", r_pref);
    DEBUG_PRINT("Prefs EEPROM: device_name=\"%s\"  location=\"%s\"  temp_unit=%u\n",
                pref.device_name, pref.location, pref.temp_unit);

    // --- 3) Build dotted‐decimal strings ----------------------------------
    char ip_s[16], gw_s[16], sn_s[16], dns_s[16];
    snprintf(ip_s, sizeof(ip_s), "%u.%u.%u.%u", net.ip[0], net.ip[1], net.ip[2], net.ip[3]);
    snprintf(gw_s, sizeof(gw_s), "%u.%u.%u.%u", net.gw[0], net.gw[1], net.gw[2], net.gw[3]);
    snprintf(sn_s, sizeof(sn_s), "%u.%u.%u.%u", net.sn[0], net.sn[1], net.sn[2], net.sn[3]);
    snprintf(dns_s, sizeof(dns_s), "%u.%u.%u.%u", net.dns[0], net.dns[1], net.dns[2], net.dns[3]);

    // --- 4) Prepare prefs strings -----------------------------------------
    char name_s[32] = "", loc_s[32] = "";
    strncpy(name_s, pref.device_name, sizeof(name_s) - 1);
    strncpy(loc_s, pref.location, sizeof(loc_s) - 1);

    // --- 5) Radio‐button flags ---------------------------------------------
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

    // --- 6) Render the HTML template --------------------------------------
    char page[4096];
    int len = snprintf(page, sizeof(page), settings_html,
                       // network (4)
                       ip_s, gw_s, sn_s, dns_s,
                       // prefs   (2)
                       name_s, loc_s,
                       // radios  (3)
                       c_chk, f_chk, k_chk);
    DEBUG_PRINT("Rendered HTML len=%d\n", len);

    // --- 7) Send HTTP headers + body --------------------------------------
    char hdr[128];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n",
                        len);
    DEBUG_PRINT("Sending header len=%d\n", hlen);

    send(sock, (uint8_t *)hdr, hlen);
    send(sock, (uint8_t *)page, len);

    DEBUG_PRINT("<< handle_settings_request() done\n");
}

void handle_settings_post(uint8_t sock, char *body) {
    DEBUG_PRINT(">> handle_settings_post()\n");
    DEBUG_PRINT("Raw POST body: \"%s\"\n", body);

    // --- 1) Update network config ------------------------------------------
    networkInfo net = LoadUserNetworkConfig();
    networkInfo backup_net = net;
    char buf[64];
    char *tmp;

    // IP
    if ((tmp = get_form_value(body, "ip"))) {
        DEBUG_PRINT("Raw ip=\"%s\"\n", tmp);
        urldecode(tmp);
        DEBUG_PRINT("Decoded ip=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.ip[0] = a;
            net.ip[1] = b;
            net.ip[2] = c;
            net.ip[3] = d;
            DEBUG_PRINT("  -> new IP=%u.%u.%u.%u\n", a, b, c, d);
        }
    }
    // Gateway
    if ((tmp = get_form_value(body, "gateway"))) {
        DEBUG_PRINT("Raw gateway=\"%s\"\n", tmp);
        urldecode(tmp);
        DEBUG_PRINT("Decoded gateway=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.gw[0] = a;
            net.gw[1] = b;
            net.gw[2] = c;
            net.gw[3] = d;
            DEBUG_PRINT("  -> new GW=%u.%u.%u.%u\n", a, b, c, d);
        }
    }
    // Subnet
    if ((tmp = get_form_value(body, "subnet"))) {
        DEBUG_PRINT("Raw subnet=\"%s\"\n", tmp);
        urldecode(tmp);
        DEBUG_PRINT("Decoded subnet=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.sn[0] = a;
            net.sn[1] = b;
            net.sn[2] = c;
            net.sn[3] = d;
            DEBUG_PRINT("  -> new SN=%u.%u.%u.%u\n", a, b, c, d);
        }
    }
    // DNS
    if ((tmp = get_form_value(body, "dns"))) {
        DEBUG_PRINT("Raw dns=\"%s\"\n", tmp);
        urldecode(tmp);
        DEBUG_PRINT("Decoded dns=\"%s\"\n", tmp);
        strncpy(buf, tmp, sizeof(buf) - 1);
        buf[63] = 0;
        unsigned a, b, c, d;
        if (sscanf(buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            net.dns[0] = a;
            net.dns[1] = b;
            net.dns[2] = c;
            net.dns[3] = d;
            DEBUG_PRINT("  -> new DNS=%u.%u.%u.%u\n", a, b, c, d);
        }
    }

    // Write network only if changed
    if (memcmp(&net, &backup_net, sizeof(net)) != 0) {
        DEBUG_PRINT("Network changed; writing EEPROM\n");
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
        DEBUG_PRINT("Wiznet reconfigured\n");
    } else {
        DEBUG_PRINT("Network unchanged; skipping EEPROM write\n");
    }

    // --- 2) Update user preferences ----------------------------------------
    userPrefInfo pref = LoadUserPreferences();
    userPrefInfo backup_pref = pref;

    // Device Name
    if ((tmp = get_form_value(body, "device_name"))) {
        DEBUG_PRINT("Raw device_name=\"%s\"\n", tmp);
        urldecode(tmp);
        DEBUG_PRINT("Decoded device_name=\"%s\"\n", tmp);
        strncpy(pref.device_name, tmp, sizeof(pref.device_name) - 1);
        pref.device_name[sizeof(pref.device_name) - 1] = '\0';
    }
    // Location
    if ((tmp = get_form_value(body, "location"))) {
        DEBUG_PRINT("Raw location=\"%s\"\n", tmp);
        urldecode(tmp);
        DEBUG_PRINT("Decoded location=\"%s\"\n", tmp);
        strncpy(pref.location, tmp, sizeof(pref.location) - 1);
        pref.location[sizeof(pref.location) - 1] = '\0';
    }
    // Temp Unit
    if ((tmp = get_form_value(body, "temp_unit"))) {
        DEBUG_PRINT("Raw temp_unit=\"%s\"\n", tmp);
        urldecode(tmp);
        DEBUG_PRINT("Decoded temp_unit=\"%s\"\n", tmp);
        if (!strcmp(tmp, "fahrenheit"))
            pref.temp_unit = 1;
        else if (!strcmp(tmp, "kelvin"))
            pref.temp_unit = 2;
        else
            pref.temp_unit = 0;
        DEBUG_PRINT("  -> new temp_unit=%u\n", pref.temp_unit);
    }

    // Write prefs only if changed
    if (memcmp(&pref, &backup_pref, sizeof(pref)) != 0) {
        DEBUG_PRINT("Prefs changed; writing EEPROM\n");
        EEPROM_WriteUserPrefsWithChecksum(&pref);
        sleep_ms(10);
    } else {
        DEBUG_PRINT("Prefs unchanged; skipping EEPROM write\n");
    }

    // --- 3) Send 204 No Content then reboot -------------------------------
    const char *resp = "HTTP/1.1 204 No Content\r\n"
                       "Connection: close\r\n"
                       "\r\n";
    send(sock, (uint8_t *)resp, strlen(resp));
    sleep_ms(100);
    watchdog_reboot(0, 0, 0);
}

void handle_control_request(uint8_t sock, char *body) {
    if (!body) {
        const char *resp = "HTTP/1.1 400 Bad Request\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: 26\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "\r\n"
                           "{\"error\":\"Missing body\"}";
        send(sock, (uint8_t *)resp, strlen(resp));
        return;
    }

    // First read current states
    bool current_states[8];
    for (int i = 0; i < 8; i++) {
        current_states[i] = mcp_relay_read_pin(i);
    }

    // Process each channel
    for (int i = 1; i <= 8; i++) {
        char key[10];
        snprintf(key, sizeof(key), "channel%d", i);
        char *value = get_form_value(body, key);

        bool new_state = (value != NULL && strcmp(value, "on") == 0);
        bool current_state = current_states[i - 1];

        // Only toggle if state needs to change
        if (new_state != current_state) {
            PDU_Display_ToggleRelay(i);
        }
    }

    const char *resp = "HTTP/1.1 302 Found\r\n"
                       "Location: /control.html\r\n"
                       "Content-Length: 0\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "\r\n";
    send(sock, (uint8_t *)resp, strlen(resp));
}
