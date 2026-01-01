/**
 * @file src/web_handlers/status_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui3 3. Status Handler
 * @ingroup webhandlers
 * @brief Handler for building JSON for /api/status
 * @{
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details Handles GET requests to /api/status endpoint. Returns JSON with
 * channel states (cached from SwitchTask), voltage/current/power (cached from
 * MeterTask), channel labels, internal temperature, and system status.
 * Decouples UI immediacy from measurement cadence.
 *
 * JSON Response Structure:
 * @code{.json}
 * {
 *   "channels": [
 *     {
 *       "voltage": 230.5,
 *       "current": 0.45,
 *       "uptime": 3600,
 *       "power": 103.7,
 *       "state": true,
 *       "label": "Web Server"
 *     },
 *     ...
 *   ],
 *   "internalTemperature": 45.2,
 *   "temperatureUnit": "°C",
 *   "systemStatus": "OK",
 *   "overcurrent": {
 *     "state": "NORMAL",
 *     "total_current_a": 3.6,
 *     "limit_a": 10.0,
 *     "switching_allowed": true,
 *     ...
 *   }
 * }
 * @endcode
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STATUS_HANDLER_H
#define STATUS_HANDLER_H

#include "../CONFIG.h"

/**
 * @brief Handles the HTTP request for the status page
 *
 * Returns JSON with complete system status including channel states,
 * power metrics, labels, temperature, and overcurrent protection status.
 *
 * @param sock The socket number
 *
 * @http
 * - 200 OK: Success with JSON body
 */
void handle_status_request(uint8_t sock);

#endif // STATUS_HANDLER_H

/** @} */