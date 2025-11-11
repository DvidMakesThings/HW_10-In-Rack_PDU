/**
 * @file src/web_handlers/control_handler.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Handles POST requests to /control endpoint for relay control.
 *          Processes form-encoded channel states and labels, applies changes
 *          using idempotent relay control functions with logging and dual
 *          asymmetry detection.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define CONTROL_HANDLER_TAG "<Control Handler>"

/**
 * @brief Handles the HTTP request for the control page
 * @param sock The socket number
 * @param body Form-encoded POST body (e.g., channel1=on&channel3=on)
 * @return None
 * @note Uses set_relay_state_with_tag() for idempotent writes with logging.
 *       Decodes URL-encoded body in-place, so body must be writable.
 *       Tracks and warns about dual asymmetry conditions.
 */
void handle_control_request(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_control_request()\n");
    net_beat();
    NETLOG_PRINT("Incoming body: '%s'\n", body ? body : "(null)");
    net_beat();

    if (!body) {
        static const char resp[] = "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Type: application/json\r\n"
                                   "Content-Length: 26\r\n"
                                   "Access-Control-Allow-Origin: *\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "{\"error\":\"Missing body\"}";
        send(sock, (uint8_t *)resp, sizeof(resp) - 1);
        net_beat();
        return;
    }

    /* Decode in-place (handles '+' and %XX) */
    urldecode(body);
    net_beat();

    /* Default all OFF unless present in form */
    bool want_on[8] = {false};
    for (int i = 1; i <= 8; i++) {
        char key[16];
        snprintf(key, sizeof(key), "channel%d", i);
        char *value = get_form_value(body, key);
        if (value && strcmp(value, "on") == 0) {
            want_on[i - 1] = true;
        }
        if ((i & 3) == 0)
            net_beat();
    }

    /* Apply relay states using mcp_set_channel_state() */
    int changed_total = 0;
    for (uint8_t i = 0; i < 8; i++) {
        bool current = mcp_read_pin(mcp_relay(), i);
        if (current != want_on[i]) {
            mcp_set_channel_state(i, (uint8_t)want_on[i]);
            changed_total++;
            NETLOG_PRINT("  CH%u: %s -> %s\n", (unsigned)(i + 1), current ? "ON" : "OFF",
                         want_on[i] ? "ON" : "OFF");
        }
        if ((i & 1) == 0)
            net_beat();
    }

    NETLOG_PRINT("Control: changed=%d\n", changed_total);
    net_beat();

    /* Update channel labels if provided */
    for (uint8_t ch = 0; ch < 8; ch++) {
        char key[16];
        snprintf(key, sizeof(key), "label%u", (unsigned)(ch + 1));
        char *val = get_form_value(body, key);
        if (val) {
            EEPROM_WriteChannelLabel(ch, val);
        }
        if ((ch & 1) == 0)
            net_beat();
    }

    /* Send minimal 204 No Content response */
    static const char ok[] = "HTTP/1.1 204 No Content\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "\r\n";
    send(sock, (uint8_t *)ok, sizeof(ok) - 1);
    net_beat();

    NETLOG_PRINT("<< handle_control_request() done\n");
}
