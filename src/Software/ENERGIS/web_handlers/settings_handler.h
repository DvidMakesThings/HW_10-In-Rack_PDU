/**
 * @file settings_handler.h
 * @author David Sipos
 * @brief Handler for the page control.html
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SETTINGS_HANDLER_H
#define SETTINGS_HANDLER_H

#include "../CONFIG.h"
#include <stdint.h>

void handle_settings_request(uint8_t sock);
void handle_settings_post(uint8_t sock, char *body);

#endif // SETTINGS_HANDLER_H
