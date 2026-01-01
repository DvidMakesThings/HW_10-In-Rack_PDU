/**
 * @file src/web_handlers/settings_handler.c
 * @author
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Handles GET and POST requests for settings page. Reads/writes
 * network configuration and user preferences from/to EEPROM.
 * Triggers system reboot after settings changes.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"
#include "../html/settings_gz.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define SETTINGS_HANDLER_TAG "<Settings Handler>"

#define SETTINGS_MAX_SEND_CHUNK 4096

/**
 * @brief Sends a buffer in safe chunks to avoid overrunning W5500 TX window.
 *
 * @param sock Socket number.
 * @param data Pointer to buffer.
 * @param len  Total length to send.
 *
 * @return Total bytes sent on success, -1 on send failure.
 */
static int settings_send_all(uint8_t sock, const uint8_t *data, int len) {
    int total_sent = 0;

    while (total_sent < len) {
        int remaining = len - total_sent;
        int to_send = (remaining > SETTINGS_MAX_SEND_CHUNK) ? SETTINGS_MAX_SEND_CHUNK : remaining;

        int sent = send(sock, (uint8_t *)(data + total_sent), (uint16_t)to_send);
        if (sent <= 0) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_SETTINGS, 0x9);
            ERROR_PRINT_CODE(errorcode, "%s Send failed on socket %u\r\n", SETTINGS_HANDLER_TAG,
                             sock);
            Storage_EnqueueErrorCode(errorcode);
#endif
            return -1;
        }

        total_sent += sent;
        net_beat();

        if (total_sent < len) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    return total_sent;
}

/**
 * @brief Handles the HTTP request for the settings page (HTML)
 * @param sock The socket number
 * @return None
 * @note This function serves the settings page (gzipped) from flash
 */
void handle_settings_request(uint8_t sock) {
    NETLOG_PRINT(">> handle_settings_request()\n");
    net_beat();

    const int plen = (int)settings_gz_len;

    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Encoding: gzip\r\n"
                        "Content-Length: %d\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Cache-Control: no-cache\r\n"
                        "Connection: close\r\n"
                        "\r\n",
                        plen);
    if (hlen < 0)
        hlen = 0;
    if (hlen > (int)sizeof(hdr))
        hlen = (int)sizeof(hdr);

    (void)settings_send_all(sock, (const uint8_t *)hdr, hlen);
    net_beat();

    if (plen > 0) {
        (void)settings_send_all(sock, (const uint8_t *)settings_gz, plen);
        net_beat();
    }

    NETLOG_PRINT("<< handle_settings_request() done\n");
}

/**
 * @brief Handles the HTTP request for the settings API (GET /api/settings)
 * @param sock The socket number
 * @return None
 *
 * @details Returns JSON with network configuration and user preferences:
 *  - ip, gateway, subnet, dns, mac, device_name, location, temp_unit, temperature, serial_number,
 *    firmware_version, hardware_version, device_region, current_limit, warning_limit,
 * critical_limit, timezone, time, autologout
 *
 * @http
 * - 200 OK on success with JSON body and Connection: close.
 */
void handle_settings_api(uint8_t sock) {
    NETLOG_PRINT(">> handle_settings_api()\n");
    net_beat();

    /* Load network via StorageTask (RAM cache-backed) */
    networkInfo net;
    if (!storage_get_network(&net)) {
        net = LoadUserNetworkConfig();
    }

    /* Load prefs via StorageTask (RAM cache-backed) */
    userPrefInfo pref;
    if (!storage_get_prefs(&pref)) {
        pref = LoadUserPreferences();
    }

    /* Read internal temp */
    adc_select_input(4);
    int raw = adc_read();
    const float VREF = 3.00f;
    float voltage = (raw * VREF) / (1 << 12);
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;

    /* Convert to user's unit */
    float temp_out;
    const char *unit_suffix;
    switch (pref.temp_unit) {
    case 1:
        unit_suffix = "fahrenheit";
        temp_out = temp_c * 9.0f / 5.0f + 32.0f;
        break;
    case 2:
        unit_suffix = "kelvin";
        temp_out = temp_c + 273.15f;
        break;
    default:
        unit_suffix = "celsius";
        temp_out = temp_c;
        break;
    }

    /* Device identity and limits */
    const device_identity_t *id = DeviceIdentity_Get();
    const char *serial_str = (id && id->valid) ? id->serial_number : "";
    const char *region_str = "UNKNOWN";
    if (id && id->valid) {
        region_str = (id->region == DEVICE_REGION_EU)   ? "EU"
                     : (id->region == DEVICE_REGION_US) ? "US"
                                                        : "UNKNOWN";
    }

    overcurrent_status_t oc = {0};
    (void)Overcurrent_GetStatus(&oc);

    char mac_s[24];
    snprintf(mac_s, sizeof(mac_s), "%02X:%02X:%02X:%02X:%02X:%02X", net.mac[0], net.mac[1],
             net.mac[2], net.mac[3], net.mac[4], net.mac[5]);

    char current_limit_s[32];
    char warning_limit_s[32];
    char critical_limit_s[32];
    snprintf(current_limit_s, sizeof(current_limit_s), "%.1f A", oc.limit_a);
    snprintf(warning_limit_s, sizeof(warning_limit_s), "%.2f A", oc.warning_threshold_a);
    snprintf(critical_limit_s, sizeof(critical_limit_s), "%.2f A", oc.critical_threshold_a);

    /* Build JSON response */
    char json[1400];
    int len = snprintf(json, sizeof(json),
                       "{"
                       "\"ip\":\"%u.%u.%u.%u\","
                       "\"gateway\":\"%u.%u.%u.%u\","
                       "\"subnet\":\"%u.%u.%u.%u\","
                       "\"dns\":\"%u.%u.%u.%u\","
                       "\"mac\":\"%s\","
                       "\"device_name\":\"%s\","
                       "\"location\":\"%s\","
                       "\"temp_unit\":\"%s\","
                       "\"temperature\":%.2f,"
                       "\"serial_number\":\"%s\","
                       "\"firmware_version\":\"%s\","
                       "\"hardware_version\":\"%s\","
                       "\"device_region\":\"%s\","
                       "\"current_limit\":\"%s\","
                       "\"warning_limit\":\"%s\","
                       "\"critical_limit\":\"%s\""
                       "}",
                       net.ip[0], net.ip[1], net.ip[2], net.ip[3], net.gw[0], net.gw[1], net.gw[2],
                       net.gw[3], net.sn[0], net.sn[1], net.sn[2], net.sn[3], net.dns[0],
                       net.dns[1], net.dns[2], net.dns[3], mac_s, pref.device_name, pref.location,
                       unit_suffix, temp_out, serial_str, FIRMWARE_VERSION, HARDWARE_VERSION,
                       region_str, current_limit_s, warning_limit_s, critical_limit_s);

    if (len < 0)
        len = 0;
    if (len > (int)sizeof(json))
        len = (int)sizeof(json);

    /* Send HTTP headers + JSON */
    char hdr[160];
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
 * @brief Handles the HTTP POST for settings (/api/settings)
 * @param sock The socket number
 * @param body Form-encoded POST body
 * @return None
 *
 * @details Updates network config and user preferences. Writes changes to EEPROM,
 *          applies runtime net config if needed, then reboots the device.
 *
 * @http
 * - 400 Bad Request if body is missing.
 * - 204 No Content on success, followed by reboot.
 */
void handle_settings_post(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_settings_post()\n");
    NETLOG_PRINT("Raw POST body: \"%s\"\n", body ? body : "(null)");
    net_beat();

    if (!body) {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 30\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Cache-Control: no-cache\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Missing form body\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_HTTP_SETTINGS, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s Missing POST body in settings handler\r\n",
                         SETTINGS_HANDLER_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        return;
    }

    /* Update network config (read from StorageTask cache) */
    networkInfo net;
    if (!storage_get_network(&net)) {
        net = LoadUserNetworkConfig();
    }
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
        (void)storage_set_network(&net);
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

    /* Update user preferences (read from StorageTask cache) */
    userPrefInfo pref;
    if (!storage_get_prefs(&pref)) {
        pref = LoadUserPreferences();
    }
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
            pref.temp_unit = 0; /* celsius/default */
    }

    if (memcmp(&pref, &backup_pref, sizeof(pref)) != 0) {
        (void)storage_set_prefs(&pref);
        vTaskDelay(pdMS_TO_TICKS(10));
        net_beat();
    }

    /* 204 No Content (then reboot) */
    {
        static const char resp[] = "HTTP/1.1 204 No Content\r\n"
                                   "Access-Control-Allow-Origin: *\r\n"
                                   "Cache-Control: no-cache\r\n"
                                   "Connection: close\r\n"
                                   "\r\n";
        send(sock, (uint8_t *)resp, sizeof(resp) - 1);
        net_beat();
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    net_beat();
    Health_RebootNow("HTTP settings applied");
}
