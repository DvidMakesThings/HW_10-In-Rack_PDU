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

/**
 * @brief Handles the HTTP request for the control page.
 * @param sock The socket number.
 * @param body The request body.
 * @note This function is called when the user interacts with the control page.
 */
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
