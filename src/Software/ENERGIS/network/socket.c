#include "socket.h"
#include "w5500.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include "../CONFIG.h"

#define SOCKET_OPEN_RETRY_LIMIT  10
#define SOCKET_OPEN_DELAY_MS     500

// Internal array to track socket usage (0 = available, 1 = open)
static uint8_t W5500_socketStatus[W5500_MAX_SOCKETS] = { W5500_SOCKET_AVAILALE };

/**
 * Open a socket with the specified protocol.
 *
 * This implements the following logic:
 *
 *   START:
 *      set Sn_MR = protocol;          // e.g., 0x01 for TCP
 *      set Sn_PORT = SERVER_PORT;       // source port
 *      set Sn_CR = OPEN;
 *      wait until Sn_CR becomes 0 and Sn_SR becomes SOCK_INIT.
 *      if not, then set Sn_CR = CLOSE and go back to START (with a retry limit).
 *
 * Returns socket number on success, or -1 on error.
 */
int w5500_socket_open(uint8_t protocol) {
    // Check that a valid protocol is passed.
    if (protocol == 0) {
        printf("[SOCKET] ERROR: Protocol value is 0. Please pass a valid protocol constant (e.g., W5500_SOCK_MR_TCP).\n");
        return -1;
    }

    int sock = -1;
    uint8_t sr, cr;
    uint16_t port;
    const int max_attempts = 3;

    // For TCP/UDP, socket 0 is reserved for MACRAW.
    // Try sockets 1 to W5500_MAX_SOCKETS - 1.
    for (int i = 1; i < W5500_MAX_SOCKETS; i++) {
        sr = w5500_read_reg(W5500_Sn_SR(i)); // Using your function for common register reads is fine.
        if (sr == W5500_SR_CLOSED) {
            sock = i;
            // Mark the socket as open in the internal status array.
            W5500_socketStatus[sock] = W5500_SOCKET_OPEN;
            // Clear any pending interrupts.
            {
                uint8_t clear_val = 0xFF;
                w5500_Send(W5500_SREG_IR_OFFSET, W5500_BLOCK_S_REG(sock), 1, &clear_val, 1);
            }
            // Set the socket mode (Sn_MR) using w5500_Send.
            {
                uint8_t proto = protocol;
                w5500_Send(W5500_SREG_MR_OFFSET, W5500_BLOCK_S_REG(sock), 1, &proto, 1);
            }
            // Set the source port (Sn_PORT) using w5500_Send.
            {
                port = SERVER_PORT;
                // Ensure the port is in network order (big-endian) if required.
                uint8_t port_buf[2];
                port_buf[0] = (port >> 8) & 0xFF;
                port_buf[1] = port & 0xFF;
                w5500_Send(W5500_SREG_PORT_OFFSET, W5500_BLOCK_S_REG(sock), 1, port_buf, 2);
            }
            // Read back MR and PORT for debugging.
            {
                uint8_t read_proto;
                uint8_t port_buf[2];
                w5500_Send(W5500_SREG_MR_OFFSET, W5500_BLOCK_S_REG(sock), 0, &read_proto, 1);
                w5500_Send(W5500_SREG_PORT_OFFSET, W5500_BLOCK_S_REG(sock), 0, port_buf, 2);
                printf("\n[SOCKET] Socket %d: Set MR=0x%02X, PORT=0x%02X%02X\n", sock, read_proto, port_buf[0], port_buf[1]);
            }
            
            int attempts = 0;
            while (attempts < max_attempts) {
                // Issue the OPEN command (which is 0x01).
                uint8_t open_cmd = 0x01;
                if (w5500_ExecuteSocketCommand(sock, open_cmd) == 0) {
                    sleep_ms(10);  // Allow brief time for state update.
                    // Read back CR and SR using w5500_Send.
                    w5500_Send(W5500_SREG_CR_OFFSET, W5500_BLOCK_S_REG(sock), 0, &cr, 1);
                    w5500_Send(W5500_SREG_SR_OFFSET, W5500_BLOCK_S_REG(sock), 0, &sr, 1);
                    printf("[SOCKET] Socket %d, attempt %d: CR = 0x%02X, SR = 0x%02X\n", sock, attempts + 1, cr, sr);
                    if (sr == W5500_SR_INIT) {
                        printf("[SOCKET] Socket %d opened successfully after %d attempt(s)\n", sock, attempts + 1);
                        return sock;
                    }
                }
                printf("[SOCKET] Attempt %d to open socket %d failed, retrying...\n", attempts + 1, sock);
                attempts++;
                sleep_ms(10);  // Delay between attempts.
            }
            printf("[SOCKET] Failed to open socket %d after %d attempts, trying next...\n", sock, max_attempts);
            W5500_socketStatus[sock] = W5500_SOCKET_AVAILALE;
        }
    }
    printf("[SOCKET] No available sockets for TCP/UDP\n");
    return -1;
}

/**
 * Execute a socket command and wait for the command register to clear.
 * Returns 0 if the command completed successfully, or 1 on timeout.
 */
int w5500_ExecuteSocketCommand(uint8_t socket, uint8_t cmd) {
    // Write the command to the socket's CR register
    w5500_Send(W5500_SREG_CR_OFFSET, W5500_BLOCK_S_REG(socket), 1, &cmd, 1);

    // Wait for the command register to clear
    uint8_t cr;
    uint32_t timeout = 125; // Adjust this if necessary
    while (timeout--) {
        sleep_ms(1);  // Small delay for W5500 to process
        w5500_Send(W5500_SREG_CR_OFFSET, W5500_BLOCK_S_REG(socket), 0, &cr, 1);
        if (cr == 0) {
            return 0; // Success
        }
    }

    // If CR doesn't clear, return error
    printf("[SOCKET] ExecuteSocketCommand: Socket %d, command 0x%02X did not clear CR after 125 ms (CR = 0x%02X)\n",
           socket, cmd, cr);
    return -1;
}

/**
 * Close the specified socket.
 */
void w5500_socket_close(uint8_t socket) {
    if (socket >= W5500_MAX_SOCKETS) return;
    w5500_send_socket_command(socket, W5500_CR_CLOSE);
    // Wait until command register is cleared
    while (w5500_read_reg(W5500_Sn_CR(socket)) != 0)
        sleep_ms(1);
    // Clear interrupts
    w5500_write_reg(W5500_Sn_IR(socket), 0xFF);
    W5500_socketStatus[socket] = W5500_SOCKET_AVAILALE;
    printf("[SOCKET] Socket %d closed\n", socket);
}

/**
 * Set the socket to listen mode (for TCP server operation).
 */
bool w5500_socket_listen(uint8_t socket) {
    if (socket >= W5500_MAX_SOCKETS)
        return false;

    const uint32_t timeout_ms = 1000; // Total timeout period
    uint32_t elapsed = 0;
    uint8_t status = 0;
    
    // Repeatedly issue the LISTEN command until the socket status changes to LISTEN (0x14)
    while (elapsed < timeout_ms) {
        // Use ExecuteSocketCommand() to issue LISTEN command
        if (w5500_ExecuteSocketCommand(socket, W5500_CR_LISTEN) == 0) {
            sleep_ms(10);  // Allow time for the status to update
            elapsed += 10;
            w5500_Send(W5500_SREG_SR_OFFSET, W5500_BLOCK_S_REG(socket), 0, &status, 1);
            if (status == W5500_SR_LISTEN) {
                printf("[SOCKET] Socket %d entered LISTEN state (0x%02X) after %lu ms.\n", socket, status, elapsed);
                return true;
            }
        }
        sleep_ms(10);
        elapsed += 10;
    }
    printf("[SOCKET] Socket %d did not enter LISTEN state (final status: 0x%02X) after %lu ms, closing socket.\n", socket, status, elapsed);
    w5500_send_socket_command(socket, W5500_CR_CLOSE);
    return false;
}

/**
 * Send data on the specified socket.
 */
void w5500_socket_send(uint8_t socket, const uint8_t* data, uint16_t len) {
    // Get free TX buffer space (16-bit value)
    uint16_t freeSize = w5500_read_16(W5500_Sn_TX_FSR(socket));
    if (freeSize < len) {
        printf("[SOCKET] Not enough TX buffer space on socket %d (free: %d, needed: %d)\n", socket, freeSize, len);
        return;
    }
    // Get the current TX write pointer
    uint16_t ptr = w5500_read_16(W5500_Sn_TX_WR(socket));
    // Calculate the absolute address in the TX buffer:
    // (Assume the hardware maps the socket TX base at an offset computed as follows)
    uint32_t addr = ((uint32_t)ptr << 8) + (W5500_Sn_TX_BASE(socket));
    for (uint16_t i = 0; i < len; i++) {
        w5500_write_reg(addr + i, data[i]);
    }
    ptr += len;
    w5500_write_16(W5500_Sn_TX_WR(socket), ptr);
    w5500_send_socket_command(socket, W5500_CR_SEND);
    printf("[SOCKET] Sent %d bytes on socket %d\n", len, socket);
}

/**
 * Receive data from the specified socket.
 * Returns the number of bytes received.
 */
uint16_t w5500_socket_recv(uint8_t socket, uint8_t* buffer, uint16_t len) {
    uint16_t rxSize = w5500_read_16(W5500_Sn_RX_RSR(socket));
    if (rxSize == 0)
        return 0;
    if (rxSize > len)
        rxSize = len;
    uint16_t ptr = w5500_read_16(W5500_Sn_RX_RD(socket));
    uint32_t addr = ((uint32_t)ptr << 8) + (W5500_Sn_RX_BASE(socket));
    for (uint16_t i = 0; i < rxSize; i++) {
        buffer[i] = w5500_read_reg(addr + i);
    }
    ptr += rxSize;
    w5500_write_16(W5500_Sn_RX_RD(socket), ptr);
    w5500_send_socket_command(socket, W5500_CR_RECV);
    printf("[SOCKET] Received %d bytes on socket %d\n", rxSize, socket);
    return rxSize;
}

// Set the HTTP response status code (e.g. 200 for OK)
void http_response_set_status(http_response_t* res, int status) {
    res->status = status;
    DEBUG_PRINT("[HTTP] Response status set to %d\n", status);
}

// Set the HTTP response body (with a maximum length of HTTP_MAX_BODY_SIZE)
// The body is copied and null-terminated.
void http_response_set_body(http_response_t* res, const char* body, size_t len) {
    if (len > HTTP_MAX_BODY_SIZE) {
        len = HTTP_MAX_BODY_SIZE;
    }
    memcpy(res->body, body, len);
    res->body[len] = '\0';
    res->body_length = (int)len;
    DEBUG_PRINT("[HTTP] Response body set, length %zu\n", len);
}

// Format and send the HTTP response over the given socket.
// This example builds a basic HTTP response with status line, Content-Length header, and the body.
void http_response_send(http_response_t* res, uint8_t socket) {
    char response[HTTP_MAX_BODY_SIZE + 256]; // Extra space for headers
    int n = snprintf(response, sizeof(response),
                     "HTTP/1.1 %d OK\r\n"
                     "Content-Length: %d\r\n"
                     "Content-Type: text/html\r\n"
                     "\r\n"
                     "%s",
                     res->status, res->body_length, res->body);
    DEBUG_PRINT("[HTTP] Sending response on socket %d, total length %d\n", socket, n);
    // Use your socket send function to send the response.
    // For example, if you have a function w5500_socket_send:
    w5500_socket_send(socket, (uint8_t*)response, n);
}