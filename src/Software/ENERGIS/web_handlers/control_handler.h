/**
 * @file control_handler.h
 * @author DvidMakesThings - David Sipos
 * @brief Handler for the page control.html
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CONTROL_HANDLER_H
#define CONTROL_HANDLER_H

#include "../CONFIG.h"
#include <stdint.h>

#define CONTROL_HANDLER_TAG "<Control Handler>"

/**
 * @brief Handles the HTTP request for the control page.
 * @param sock The socket number.
 * @param body The request body.
 * @note This function is called when the user interacts with the control page.
 */
void handle_control_request(uint8_t sock, char *body);

#endif // CONTROL_HANDLER_H
