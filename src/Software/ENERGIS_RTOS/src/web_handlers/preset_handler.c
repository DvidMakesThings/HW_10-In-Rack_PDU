/**
 * @file src/web_handlers/preset_handler.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-01-01
 *
 * @details
 * HTTP handlers for configuration preset management. Provides REST-like
 * API endpoints for saving, loading, and applying relay configuration presets.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define PRESET_HANDLER_TAG "<Preset Handler>"

/* ==================== Helper: JSON Escape ==================== */

/**
 * @brief Escape a string for safe JSON output.
 *
 * Replaces special characters with JSON escape sequences (\" \\ \n \r \t).
 * Copies only printable characters >= 0x20; truncates to fit destination.
 *
 * @param src Source C-string to escape (null-terminated).
 * @param dst Destination buffer to receive escaped string.
 * @param dst_size Size of destination buffer in bytes (including terminator).
 */
static void json_escape(const char *src, char *dst, size_t dst_size) {
    if (dst_size == 0)
        return;

    size_t si = 0, di = 0;
    while (src[si] && di < dst_size - 1) {
        char c = src[si];
        if (c == '"' || c == '\\') {
            if (di + 2 >= dst_size)
                break;
            dst[di++] = '\\';
            dst[di++] = c;
        } else if (c == '\n') {
            if (di + 2 >= dst_size)
                break;
            dst[di++] = '\\';
            dst[di++] = 'n';
        } else if (c == '\r') {
            if (di + 2 >= dst_size)
                break;
            dst[di++] = '\\';
            dst[di++] = 'r';
        } else if (c == '\t') {
            if (di + 2 >= dst_size)
                break;
            dst[di++] = '\\';
            dst[di++] = 't';
        } else if ((unsigned char)c >= 0x20) {
            dst[di++] = c;
        }
        si++;
    }
    dst[di] = '\0';
}

/* ==================== GET /api/config-presets ==================== */

/**
 * @brief Handle GET /api/config-presets
 *
 * Returns JSON with the list of valid presets and the current startup selection.
 * Uses `UserOutput_GetAllPresets()` and `UserOutput_GetStartupPreset()` from RAM cache.
 *
 * Response shape:
 * {"presets":[{"id":N,"name":"...","mask":M},...],"startup":null|0..4}
 *
 * @param sock W5500 socket ID.
 */
void handle_config_presets_get(uint8_t sock) {
    NETLOG_PRINT(">> handle_config_presets_get()\n");
    net_beat();

    /* Get all presets from cache. */
    user_output_preset_t presets[USER_OUTPUT_MAX_PRESETS];
    if (!UserOutput_GetAllPresets(presets)) {
        static const char err_resp[] = "HTTP/1.1 500 Internal Server Error\r\n"
                                       "Content-Type: application/json\r\n"
                                       "Content-Length: 30\r\n"
                                       "Access-Control-Allow-Origin: *\r\n"
                                       "Connection: close\r\n"
                                       "\r\n"
                                       "{\"error\":\"Failed to load presets\"}";
        send(sock, (uint8_t *)err_resp, sizeof(err_resp) - 1);
        net_beat();
        return;
    }

    uint8_t startup_id = UserOutput_GetStartupPreset();
    net_beat();

    /* Build JSON response. */
    char json[1024];
    int len = 0;

    len += snprintf(json + len, sizeof(json) - len, "{\"presets\":[");

    bool first = true;
    for (uint8_t i = 0; i < USER_OUTPUT_MAX_PRESETS; i++) {
        if (presets[i].valid != USER_OUTPUT_PRESET_VALID) {
            continue;
        }

        if (!first) {
            len += snprintf(json + len, sizeof(json) - len, ",");
        }
        first = false;

        char escaped_name[64];
        json_escape(presets[i].name, escaped_name, sizeof(escaped_name));

        len += snprintf(json + len, sizeof(json) - len, "{\"id\":%u,\"name\":\"%s\",\"mask\":%u}",
                        i, escaped_name, presets[i].relay_mask);

        if ((i & 1) == 0)
            net_beat();
    }

    len += snprintf(json + len, sizeof(json) - len, "],\"startup\":");
    if (startup_id == USER_OUTPUT_STARTUP_NONE) {
        len += snprintf(json + len, sizeof(json) - len, "null}");
    } else {
        len += snprintf(json + len, sizeof(json) - len, "%u}", startup_id);
    }

    if (len < 0)
        len = 0;
    if (len > (int)sizeof(json))
        len = (int)sizeof(json);

    /* Send response. */
    char hdr[192];
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

    NETLOG_PRINT("<< handle_config_presets_get() done\n");
}

/* ==================== POST /api/config-presets ==================== */

/**
 * @brief Handle POST /api/config-presets
 *
 * Accepts form-urlencoded body to save or delete a preset.
 * - action=save&id=N&name=...&mask=M
 * - action=delete&id=N
 *
 * Notes:
 * - `get_form_value()` returns a pointer to a static buffer; values are copied
 *   to local arrays immediately to avoid aliasing across multiple calls.
 * - Calls StorageTask wrappers (`storage_save_preset`, `storage_delete_preset`) so
 *   all EEPROM writes occur on the storage thread under `eepromMtx`.
 *
 * @param sock W5500 socket ID.
 * @param body Pointer to form-urlencoded POST body (mutable; decoded in-place).
 */
void handle_config_presets_post(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_config_presets_post()\n");
    net_beat();

    if (!body) {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 26\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Missing body\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    urldecode(body);
    net_beat();

    /* get_form_value returns a pointer to a static buffer.
       Copy values immediately into local buffers to avoid aliasing
       across multiple calls. */
    char action[16] = {0};
    char id_str[8] = {0};
    char *tmp;

    tmp = get_form_value(body, "action");
    if (tmp) {
        strncpy(action, tmp, sizeof(action) - 1);
        action[sizeof(action) - 1] = '\0';
    }

    tmp = get_form_value(body, "id");
    if (tmp) {
        strncpy(id_str, tmp, sizeof(id_str) - 1);
        id_str[sizeof(id_str) - 1] = '\0';
    }

    if (action[0] == '\0' || id_str[0] == '\0') {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 36\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Missing action or id\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    int id = atoi(id_str);
    if (id < 0 || id >= USER_OUTPUT_MAX_PRESETS) {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 24\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Invalid id\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    bool success = false;

    if (strcmp(action, "save") == 0) {
        /* Save preset. */
        /* get_form_value returns a pointer to a static buffer; copy immediately. */
        char name_buf[USER_OUTPUT_NAME_MAX_LEN + 1];
        name_buf[0] = '\0';
        char *tmp_name = get_form_value(body, "name");
        if (tmp_name) {
            strncpy(name_buf, tmp_name, USER_OUTPUT_NAME_MAX_LEN);
            name_buf[USER_OUTPUT_NAME_MAX_LEN] = '\0';
        }
        /* Support both 'mask' and compatibility alias 'relay_mask'. */
        char *mask_str = get_form_value(body, "mask");
        if (!mask_str) {
            mask_str = get_form_value(body, "relay_mask");
        }

        if (!mask_str) {
            static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                      "Content-Type: application/json\r\n"
                                      "Content-Length: 21\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Connection: close\r\n"
                                      "\r\n"
                                      "{\"error\":\"Missing mask\"}";
            send(sock, (uint8_t *)bad, sizeof(bad) - 1);
            net_beat();
            return;
        }

        /* Parse mask as 0-255 */
        unsigned long m = strtoul(mask_str, NULL, 10);
        if (m > 255UL)
            m = 255UL;
        uint8_t mask = (uint8_t)m;

        /* Enforce name length constraint (<= USER_OUTPUT_NAME_MAX_LEN) */
        if (name_buf[0] != '\0' && strlen(name_buf) > USER_OUTPUT_NAME_MAX_LEN) {
            static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                      "Content-Type: application/json\r\n"
                                      "Content-Length: 33\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Connection: close\r\n"
                                      "\r\n"
                                      "{\"error\":\"Name too long (max 25)\"}";
            send(sock, (uint8_t *)bad, sizeof(bad) - 1);
            net_beat();
            return;
        }

        success = storage_save_preset((uint8_t)id, name_buf, mask);
        net_beat();

        if (success) {
            NETLOG_PRINT("Preset %d saved: name='%s' mask=0x%02X\n", id, name_buf, mask);
        }
    } else if (strcmp(action, "delete") == 0) {
        /* Delete preset. */
        success = storage_delete_preset((uint8_t)id);
        net_beat();

        if (success) {
            NETLOG_PRINT("Preset %d deleted\n", id);
        }
    } else {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 28\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Unknown action\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    if (success) {
        static const char ok[] = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 15\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Cache-Control: no-cache\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "{\"success\":true}";
        send(sock, (uint8_t *)ok, sizeof(ok) - 1);
    } else {
        static const char err[] = "HTTP/1.1 500 Internal Server Error\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 62\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Save failed (EEPROM busy or write error)\"}";
        send(sock, (uint8_t *)err, sizeof(err) - 1);
    }
    net_beat();

    NETLOG_PRINT("<< handle_config_presets_post() done\n");
}

/* ==================== POST /api/apply-config ==================== */

/**
 * @brief Handle POST /api/apply-config
 *
 * Applies a saved preset immediately to relay outputs.
 * Body: id=N
 *
 * Guards:
 * - Validates `id` range.
 * - Checks `Overcurrent_IsSwitchingAllowed()` before switching.
 *
 * @param sock W5500 socket ID.
 * @param body Pointer to form-urlencoded POST body.
 */
void handle_apply_config_post(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_apply_config_post()\n");
    net_beat();

    if (!body) {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 26\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Missing body\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    urldecode(body);
    net_beat();

    char *id_str = get_form_value(body, "id");
    if (!id_str) {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 22\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Missing id\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    int id = atoi(id_str);
    if (id < 0 || id >= USER_OUTPUT_MAX_PRESETS) {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 24\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Invalid id\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    /* Check overcurrent before applying. */
    if (!Overcurrent_IsSwitchingAllowed()) {
        static const char oc[] = "HTTP/1.1 503 Service Unavailable\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 45\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "{\"error\":\"Overcurrent lockout active\"}";
        send(sock, (uint8_t *)oc, sizeof(oc) - 1);
        net_beat();
        return;
    }

    bool success = UserOutput_ApplyPreset((uint8_t)id);
    net_beat();

    if (success) {
        NETLOG_PRINT("Applied preset %d\n", id);
        static const char ok[] = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 15\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Cache-Control: no-cache\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "{\"success\":true}";
        send(sock, (uint8_t *)ok, sizeof(ok) - 1);
    } else {
        static const char err[] = "HTTP/1.1 500 Internal Server Error\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 27\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Apply failed\"}";
        send(sock, (uint8_t *)err, sizeof(err) - 1);
    }
    net_beat();

    NETLOG_PRINT("<< handle_apply_config_post() done\n");
}

/* ==================== POST /api/startup-config ==================== */

/**
 * @brief Handle POST /api/startup-config
 *
 * Sets or clears the apply-on-startup preset.
 * Body: id=N  (set)  |  action=clear  (clear)
 *
 * Notes:
 * - Copies `action` and `id` locally to avoid `get_form_value()` static buffer aliasing.
 * - Routes to StorageTask wrappers (`storage_set_startup_preset`, `storage_clear_startup_preset`).
 *
 * @param sock W5500 socket ID.
 * @param body Pointer to form-urlencoded POST body.
 */
void handle_startup_config_post(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_startup_config_post()\n");
    net_beat();

    if (!body) {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 26\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Missing body\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    urldecode(body);
    net_beat();

    /* Copy values locally to avoid get_form_value static buffer aliasing */
    char action[16] = {0};
    char id_str[8] = {0};
    char *tmp;
    tmp = get_form_value(body, "action");
    if (tmp) {
        strncpy(action, tmp, sizeof(action) - 1);
        action[sizeof(action) - 1] = '\0';
    }
    tmp = get_form_value(body, "id");
    if (tmp) {
        strncpy(id_str, tmp, sizeof(id_str) - 1);
        id_str[sizeof(id_str) - 1] = '\0';
    }

    bool success = false;

    if (action[0] != '\0' && strcmp(action, "clear") == 0) {
        /* Clear startup preset. */
        success = storage_clear_startup_preset();
        net_beat();

        if (success) {
            NETLOG_PRINT("Startup preset cleared\n");
        }
    } else if (id_str[0] != '\0') {
        /* Set startup preset. */
        int id = atoi(id_str);
        if (id < 0 || id >= USER_OUTPUT_MAX_PRESETS) {
            static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                      "Content-Type: application/json\r\n"
                                      "Content-Length: 24\r\n"
                                      "Access-Control-Allow-Origin: *\r\n"
                                      "Connection: close\r\n"
                                      "\r\n"
                                      "{\"error\":\"Invalid id\"}";
            send(sock, (uint8_t *)bad, sizeof(bad) - 1);
            net_beat();
            return;
        }

        success = storage_set_startup_preset((uint8_t)id);
        net_beat();

        if (success) {
            NETLOG_PRINT("Startup preset set to %d\n", id);
        }
    } else {
        static const char bad[] = "HTTP/1.1 400 Bad Request\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 36\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Missing action or id\"}";
        send(sock, (uint8_t *)bad, sizeof(bad) - 1);
        net_beat();
        return;
    }

    if (success) {
        static const char ok[] = "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: 15\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Cache-Control: no-cache\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "{\"success\":true}";
        send(sock, (uint8_t *)ok, sizeof(ok) - 1);
    } else {
        static const char err[] = "HTTP/1.1 500 Internal Server Error\r\n"
                                  "Content-Type: application/json\r\n"
                                  "Content-Length: 29\r\n"
                                  "Access-Control-Allow-Origin: *\r\n"
                                  "Connection: close\r\n"
                                  "\r\n"
                                  "{\"error\":\"Operation failed\"}";
        send(sock, (uint8_t *)err, sizeof(err) - 1);
    }
    net_beat();

    NETLOG_PRINT("<< handle_startup_config_post() done\n");
}