/**
 * @file api_handlers.h
 * @author David Sipos
 * @brief API endpoint handlers for the ENERGIS PDU web interface
 * @version 1.0
 * @date 2025-05-19
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <stdint.h>

void handle_status_request(uint8_t sock);
void handle_settings_request(uint8_t sock);
void handle_settings_post(uint8_t sock, char *body);
void handle_control_request(uint8_t sock, char *body);

#endif // API_HANDLERS_H
