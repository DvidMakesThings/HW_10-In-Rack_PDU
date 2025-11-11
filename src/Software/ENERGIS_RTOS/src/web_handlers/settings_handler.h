/**
 * @file src/web_handlers/settings_handler.h
 * @author
 *
 * @defgroup webui3 3. Settings Handler
 * @ingroup webhandlers
 * @brief Handler for the page settings.html
 * @{
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

#ifndef SETTINGS_HANDLER_H
#define SETTINGS_HANDLER_H

#include "../CONFIG.h"

/**
 * @brief Handles the HTTP request for the settings page (HTML)
 * @param sock The socket number
 * @note This function is called when the user requests the settings page
 */
void handle_settings_request(uint8_t sock);

/**
 * @brief Handles the HTTP request for the settings API (JSON)
 * @param sock The socket number
 * @note This function is called when JavaScript requests settings data
 */
void handle_settings_api(uint8_t sock);

/**
 * @brief Handles the HTTP POST request for the settings page
 * @param sock The socket number
 * @param body The request body (form-encoded)
 * @note This function updates settings and triggers a reboot
 */
void handle_settings_post(uint8_t sock, char *body);

#endif // SETTINGS_HANDLER_H

/** @} */