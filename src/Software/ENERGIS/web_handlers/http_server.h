/**
 * @file http_server.h
 * @author David Sipos
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

void http_server_init(void);
void http_server_process(void);

#endif // HTTP_SERVER_H
