/**
 * @file http_server.h
 * @author DvidMakesThings - David Sipos
 * @brief Header file for the HTTP server functionality
 * @version 1.0
 * @date 2025-05-19
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "../CONFIG.h"
#include <stdint.h>

/**
 * @brief Initializes the HTTP server.
 *
 * This function sets up the HTTP server by creating a socket and listening for incoming
 * connections. It also allocates memory for the HTTP buffer.
 */
void http_server_init(void);

/**
 * @brief Processes incoming HTTP requests.
 *
 * This function checks the status of the HTTP socket and handles incoming requests.
 * It responds to various API endpoints and serves static content.
 */
void http_server_process(void);

#endif // HTTP_SERVER_H
