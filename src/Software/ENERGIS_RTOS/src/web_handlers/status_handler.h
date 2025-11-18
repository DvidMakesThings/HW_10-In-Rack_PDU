/**
 * @file src/web_handlers/status_handler.h
 * @author
 *
 * @defgroup webui4 4. Status Handler
 * @ingroup webhandlers
 * @brief Handler for building JSON for /api/status
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Handles GET requests to /api/status endpoint. Returns JSON with
 * channel states (live from MCP), voltage/current/power (cached from
 * MeterTask), internal temperature, and system status. Decouples UI
 * immediacy from measurement cadence.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STATUS_HANDLER_H
#define STATUS_HANDLER_H

#include "../CONFIG.h"

/**
 * @brief Handles the HTTP request for the status page
 * @param sock The socket number
 * @note This function returns JSON with current system status
 */
void handle_status_request(uint8_t sock);

#endif // STATUS_HANDLER_H

/** @} */