/**
 * @file api_handlers.h
 * @author David Sipos
 * @brief API endpoint handlers for the ENERGIS PDU web interface
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <stdint.h>

// JSON/status endpoint
void handle_status_request(uint8_t sock);

// Render & process /settings.html GET
void handle_settings_request(uint8_t sock);
// Handle POST /settings
void handle_settings_post(uint8_t sock, char *body);

// Handle POST /control
void handle_control_request(uint8_t sock, char *body);

#endif // API_HANDLERS_H
