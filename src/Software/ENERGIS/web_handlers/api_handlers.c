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

#include "api_handlers.h"
#include "../CONFIG.h"
#include "../utils/EEPROM_MemoryMap.h"
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to extract form values
static char *get_form_value(const char *body, const char *key) {
    static char value[64];
    char search[32];
    char *start;
    char *end;

    snprintf(search, sizeof(search), "%s=", key);
    start = strstr(body, search);
    if (!start)
        return NULL;

    start += strlen(search);
    end = strstr(start, "&");
    if (!end)
        end = start + strlen(start);

    size_t len = end - start;
    if (len >= sizeof(value))
        len = sizeof(value) - 1;

    strncpy(value, start, len);
    value[len] = '\0';

    return value;
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
    // --- 1) Load network config from EEPROM -------------------------------
    networkInfo net;
    EEPROM_ReadUserNetworkWithChecksum(&net);

    // --- 2) Build network strings (4 args) --------------------------------
    char ip_s[16], gw_s[16], sn_s[16], dns_s[16];
    snprintf(ip_s, sizeof(ip_s), "%u.%u.%u.%u", net.ip[0], net.ip[1], net.ip[2], net.ip[3]);
    snprintf(gw_s, sizeof(gw_s), "%u.%u.%u.%u", net.gw[0], net.gw[1], net.gw[2], net.gw[3]);
    snprintf(sn_s, sizeof(sn_s), "%u.%u.%u.%u", net.sn[0], net.sn[1], net.sn[2], net.sn[3]);
    snprintf(dns_s, sizeof(dns_s), "%u.%u.%u.%u", net.dns[0], net.dns[1], net.dns[2], net.dns[3]);

    // --- 3) Stub out the device/time/security defaults (5 args) ----------
    char name_s[] = "ENERGIS";
    char loc_s[] = "Server Room";
    char tz_s[] = "UTC";
    char time_s[] = "12:00:00";
    char auto_s[] = "5";

    // --- 4) Radio‐button flags (3 args) -----------------------------------
    char c_chk[8] = "checked", f_chk[8] = "", k_chk[8] = "";

    // --- 5) Render into a page buffer (4+5+3 = 12 args) ------------------
    char page[4096];
    int len = snprintf(page, sizeof(page), settings_html,
                       // network
                       ip_s, gw_s, sn_s, dns_s,
                       // device/time/security
                       name_s, loc_s, tz_s, time_s, auto_s,
                       // radios
                       c_chk, f_chk, k_chk);

    // --- 6) Send HTTP headers + page -------------------------------------
    char hdr[128];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: %d\r\n"
                        "\r\n",
                        len);
    send(sock, (uint8_t *)hdr, hlen);
    send(sock, (uint8_t *)page, len);
}

void handle_settings_post(uint8_t sock, char *body) {
    char buf[64], *tmp;

    // --- 1) Load & backup existing networkInfo ---------------------------
    networkInfo net, backup_net;
    net = LoadUserNetworkConfig();
    backup_net = net;

    // --- 2) Parse incoming network fields into net -----------------------
    char ip_buf[64] = {0}, gw_buf[64] = {0};
    char sn_buf[64] = {0}, dns_buf[64] = {0};

    if ((tmp = get_form_value(body, "ip")))
        strncpy(ip_buf, tmp, sizeof(ip_buf) - 1);
    if ((tmp = get_form_value(body, "gateway")))
        strncpy(gw_buf, tmp, sizeof(gw_buf) - 1);
    if ((tmp = get_form_value(body, "subnet")))
        strncpy(sn_buf, tmp, sizeof(sn_buf) - 1);
    if ((tmp = get_form_value(body, "dns")))
        strncpy(dns_buf, tmp, sizeof(dns_buf) - 1);

    unsigned a, b, c, d;
    if (ip_buf[0] && sscanf(ip_buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
        memcpy(net.ip, (uint8_t[]){a, b, c, d}, 4);
    if (gw_buf[0] && sscanf(gw_buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
        memcpy(net.gw, (uint8_t[]){a, b, c, d}, 4);
    if (sn_buf[0] && sscanf(sn_buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
        memcpy(net.sn, (uint8_t[]){a, b, c, d}, 4);
    if (dns_buf[0] && sscanf(dns_buf, "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
        memcpy(net.dns, (uint8_t[]){a, b, c, d}, 4);

    // --- 3) Only write network EEPROM if it changed ----------------------
    if (memcmp(&net, &backup_net, sizeof(net)) != 0) {
        EEPROM_WriteUserNetworkWithChecksum(&net);
        sleep_ms(10);
        // apply immediately
        wiz_NetInfo wiz = {
            .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
            .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
            .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
            .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
            .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
            .dhcp = net.dhcp};
        wizchip_setnetinfo(&wiz);
    }

    // --- 4) Load & backup existing userPrefInfo --------------------------
    userPrefInfo pref, backup_pref;
    pref = LoadUserPreferences();
    backup_pref = pref;

    // --- 5) Parse incoming prefs fields into pref -----------------------
    if ((tmp = get_form_value(body, "device_name"))) {
        strncpy(pref.device_name, tmp, sizeof(pref.device_name) - 1);
        pref.device_name[sizeof(pref.device_name) - 1] = '\0';
    }
    if ((tmp = get_form_value(body, "location"))) {
        strncpy(pref.location, tmp, sizeof(pref.location) - 1);
        pref.location[sizeof(pref.location) - 1] = '\0';
    }
    if ((tmp = get_form_value(body, "temp_unit"))) {
        if (!strcmp(tmp, "fahrenheit"))
            pref.temp_unit = 1;
        else if (!strcmp(tmp, "kelvin"))
            pref.temp_unit = 2;
        else
            pref.temp_unit = 0;
    }

    // --- 6) Only write prefs EEPROM if it changed ------------------------
    if (memcmp(&pref, &backup_pref, sizeof(pref)) != 0) {
        EEPROM_WriteUserPrefsWithChecksum(&pref);
        sleep_ms(10);
    }

    // --- 7) Send simple OK and reboot -----------------------------------
    const char *resp = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: 2\r\n"
                       "Connection: close\r\n"
                       "\r\n"
                       "OK";
    send(sock, (uint8_t *)resp, strlen(resp));

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
