/**
 * @file control_handler.c
 * @author DvidMakesThings - David Sipos
 * @defgroup webui Web-UI Handlers
 * @brief HTTP/Web UI handler modules for the Energis PDU.
 * @{
 *
 * @defgroup webui1 1. Control Handler
 * @ingroup webui
 * @brief Handler for the page control.html
 * @{
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "control_handler.h"
#include "CONFIG.h"
#include "form_helpers.h" // <-- needed for get_form_value(), urldecode()

/**
 * @brief Handles the HTTP request for the control page.
 *        Uses set_relay_state_tag() for idempotent writes + logging.
 * @param sock The socket number.
 * @param body Form-encoded POST body (e.g., channel1=on&channel3=on).
 * @return void
 */
void handle_control_request(uint8_t sock, char *body) {
    NETLOG_PRINT(">> handle_control_request()\n");
    NETLOG_PRINT("Incoming body: '%s'\n", body ? body : "(null)");

    if (!body) {
        static const char resp[] = "HTTP/1.1 400 Bad Request\r\n"
                                   "Content-Type: application/json\r\n"
                                   "Content-Length: 26\r\n"
                                   "Access-Control-Allow-Origin: *\r\n"
                                   "Connection: close\r\n"
                                   "\r\n"
                                   "{\"error\":\"Missing body\"}";
        send(sock, (uint8_t *)resp, sizeof(resp) - 1);
        return;
    }

    // Decode in-place (handles '+' and %XX)
    urldecode(body);

    // Default all OFF unless present in form
    bool want_on[8] = {false};
    for (int i = 1; i <= 8; i++) {
        char key[16];
        snprintf(key, sizeof key, "channel%d", i);
        char *value = get_form_value(body, key);
        if (value && strcmp(value, "on") == 0) {
            want_on[i - 1] = true;
        }
    }

    // Apply deterministically, log per actual change, and track asymmetry
    int changed_total = 0;
    uint8_t any_asym_before = 0, any_asym_after = 0;

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t ab = 0, aa = 0;
        changed_total +=
            set_relay_state_with_tag(CONTROL_HANDLER_TAG, i, (uint8_t)want_on[i], &ab, &aa);
        any_asym_before |= ab;
        any_asym_after |= aa;
    }

    NETLOG_PRINT("Control: changed=%d\n", changed_total);

    if (any_asym_before || any_asym_after) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n",
                      any_asym_after ? "PERSISTING" : "DETECTED", (unsigned)mask,
                      CONTROL_HANDLER_TAG);
    }

    /* Channel labels */
    for (uint8_t ch = 0; ch < 8; ch++) {
        char key[16];
        snprintf(key, sizeof key, "label%u", (unsigned)(ch + 1));
        char *val = get_form_value(body, key);
        if (val) {
            EEPROM_WriteChannelLabel(ch, val);
        }
    }

    // Minimal, fast reply (your JS already handles this)
    static const char ok[] = "HTTP/1.1 204 No Content\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "Cache-Control: no-cache\r\n"
                             "Connection: close\r\n"
                             "\r\n";
    send(sock, (uint8_t *)ok, sizeof(ok) - 1);

    NETLOG_PRINT("<< handle_control_request() done\n");
}

/**
 * @}
 */