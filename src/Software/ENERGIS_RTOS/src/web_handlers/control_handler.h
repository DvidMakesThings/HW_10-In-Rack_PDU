/**
 * @file src/web_handlers/control_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui2 2. Control Handler
 * @ingroup webhandlers
 * @brief Handler for the page control.html
 * @{
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details Handles POST requests to /api/control endpoint for relay control.
 * Processes form-encoded channel states and labels, applies changes
 * using idempotent relay control functions with logging. Label updates
 * are performed via storage queue for thread safety.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CONTROL_HANDLER_H
#define CONTROL_HANDLER_H

#include "../CONFIG.h"

/**
 * @brief Handles the HTTP request for the control page
 *
 * Processes POST requests to /api/control with form-encoded body:
 * - channelN (1-8): "on" or "off" for relay state
 * - labelN (1-8): Channel label string (max 25 chars)
 *
 * @param sock The socket number
 * @param body The request body (form-encoded)
 *
 * @http
 * - 400 Bad Request: Missing body
 * - 503 Service Unavailable: Overcurrent lockout prevents switching ON
 * - 200 OK: Success with "OK\n" body
 */
void handle_control_request(uint8_t sock, char *body);

#endif // CONTROL_HANDLER_H

/** @} */