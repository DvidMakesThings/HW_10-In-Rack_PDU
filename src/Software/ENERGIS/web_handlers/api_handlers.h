/**
 * @file api_handlers.h
 * @author DvidMakesThings - David Sipos
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

/**
 * @brief Handles the HTTP request for the given socket.
 * @param sock The socket number.
 */
void handle_status_request(uint8_t sock);

/**
 * @brief Handles the HTTP request for the settings page.
 * @param sock The socket number.
 */
void handle_settings_request(uint8_t sock);

/**
 * @brief Handles the HTTP request for the control page.
 * @param sock The socket number.
 * @param body The request body.
 * @note This function is called when the user interacts with the control page.
 */
void handle_settings_post(uint8_t sock, char *body);

/**
 * @brief Handles the HTTP request for the control page.
 * @param sock The socket number.
 * @param body The request body.
 * @note This function is called when the user interacts with the control page.
 */
void handle_control_request(uint8_t sock, char *body);

#endif // API_HANDLERS_H
