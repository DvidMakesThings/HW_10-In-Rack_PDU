/**
 * @file src/web_handlers/http_server.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webhandlers Web-UI Handlers
 * @brief HTTP server functionality for the ENERGIS PDU project.
 * @{
 *
 * @defgroup webui1 1. HTTP Server
 * @ingroup webhandlers
 * @brief Main HTTP server implementation
 * @{
 *
 * @version 1.0.0
 * @date 2025-05-24
 *
 * @details Main HTTP server implementation using W5500 Ethernet controller.
 *          Handles all HTTP requests and routes to appropriate handlers.
 *          Uses spiMtx for thread-safe W5500 access.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "../CONFIG.h"

/**
 * @brief Initializes the HTTP server
 * @return true on success, false on failure
 * @note Opens listening socket on port 80 and allocates buffers
 */
bool http_server_init(void);

/**
 * @brief Processes incoming HTTP requests
 * @return None
 * @note Should be called regularly from NetTask to handle connections
 */
void http_server_process(void);

#endif // HTTP_SERVER_H

/** @} */