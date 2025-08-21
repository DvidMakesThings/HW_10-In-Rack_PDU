/**
 * @file control_handler.c
 * @author DvidMakesThings - David Sipos
 * @brief Handler for the page control.html
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "control_handler.h"
#include "CONFIG.h"
#include "form_helpers.h"   // <-- needed for get_form_value(), urldecode()

// explicit setter: only toggles if needed
static inline void set_relay_state(int ch1_based, bool want_on) {
    bool is_on = mcp_relay_read_pin(ch1_based - 1);
    if (is_on != want_on) {
        PDU_Display_ToggleRelay(ch1_based);
    }
}

/**
 * @brief Handles the HTTP request for the control page.
 * @param sock The socket number.
 * @param body Form-encoded POST body (e.g., channel1=on&channel3=on).
 */
void handle_control_request(uint8_t sock, char *body) {
    INFO_PRINT(">> handle_control_request()\n");
    INFO_PRINT("Incoming body: '%s'\n", body ? body : "(null)");

    if (!body) {
        static const char resp[] =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 26\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "\r\n"
            "{\"error\":\"Missing body\"}";
        send(sock, (uint8_t*)resp, sizeof(resp)-1);
        return;
    }

    // Decode in-place (handles '+' and %XX even if not strictly needed for "on")
    urldecode(body);

    // Default all OFF unless present in form
    bool want_on[8] = {false};
    for (int i = 1; i <= 8; i++) {
        char key[16];
        snprintf(key, sizeof key, "channel%d", i);
        char *value = get_form_value(body, key);
        if (value && strcmp(value, "on") == 0) {
            want_on[i-1] = true;
        }
    }

    // Apply deterministically
    for (int i = 1; i <= 8; i++) {
        set_relay_state(i, want_on[i-1]);
    }

    // Minimal, fast reply (your JS already handles this)
    static const char ok[] =
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n";
    send(sock, (uint8_t*)ok, sizeof(ok)-1);

    INFO_PRINT("<< handle_control_request() done\n");
}