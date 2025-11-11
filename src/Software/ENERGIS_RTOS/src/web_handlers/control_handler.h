/**
 * @file src/web_handlers/control_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui2 2. Control Handler
 * @ingroup webhandlers
 * @brief Handler for the page control.html
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Handles POST requests to /api/control endpoint for relay control.
 * Processes form-encoded channel states and labels, applies changes
 * using idempotent relay control functions with logging and dual
 * asymmetry detection.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CONTROL_HANDLER_H
#define CONTROL_HANDLER_H

#include "../CONFIG.h"

/**
 * @brief Handles the HTTP request for the control page
 * @param sock The socket number
 * @param body The request body (form-encoded)
 * @note This function is called when the user interacts with the control page
 */
void handle_control_request(uint8_t sock, char *body);

#endif // CONTROL_HANDLER_H

/** @} */