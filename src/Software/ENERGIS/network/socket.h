#ifndef SOCKET_H
#define SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include "w5500.h"
#include "../CONFIG.h"

#define W5500_CMD_TIMEOUT 125

/*-------------------------------------------------------------------------*/
/* Socket-level function prototypes */

// Open a socket with the given protocol (e.g. W5500_SOCK_MR_TCP for TCP).
// Returns socket number (0 ... W5500_MAX_SOCKETS-1) on success, or -1 on error.
int w5500_socket_open(uint8_t protocol);

// Close the specified socket.
void w5500_socket_close(uint8_t socket);

// Put the socket in listen mode (for TCP server).
bool w5500_socket_listen(uint8_t socket);

// Send data on the specified socket.
void w5500_socket_send(uint8_t socket, const uint8_t* data, uint16_t len);

// Receive data from the specified socket.
// Returns the number of bytes received.
uint16_t w5500_socket_recv(uint8_t socket, uint8_t* buffer, uint16_t len);

/**
 * Execute a socket command and wait for the command register to clear.
 * Returns 0 if the command completed successfully, or 1 on timeout.
 */
int w5500_ExecuteSocketCommand(uint8_t socket, uint8_t cmd);

// Set the HTTP response status code (e.g. 200 for OK)
void http_response_set_status(http_response_t* res, int status);

// Set the HTTP response body (with a maximum length of HTTP_MAX_BODY_SIZE)
// The body is copied and null-terminated.
void http_response_set_body(http_response_t* res, const char* body, size_t len);

// Format and send the HTTP response over the given socket.
// This example builds a basic HTTP response with status line, Content-Length header, and the body.
void http_response_send(http_response_t* res, uint8_t socket);

#endif // SOCKET_H
