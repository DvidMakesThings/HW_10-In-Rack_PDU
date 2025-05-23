/**
 * @file status_handler.h
 * @author David Sipos
 * @brief Handler for building JSON for /api/status
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STATUS_HANDLER_H
#define STATUS_HANDLER_H

#include <stdint.h>

/**
 * @brief Handles the HTTP request for the status page.
 * @param sock The socket number.
 * @note This function is called when the user interacts with the status page.
 */
void handle_status_request(uint8_t sock);

#endif // STATUS_HANDLER_H
