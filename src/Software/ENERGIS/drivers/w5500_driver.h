/**
 * @file w5500_driver.h
 * @author David Sipos (DvidMakesThings)
 * @brief Driver for W5500 Ethernet IC
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */


#ifndef W5500_DRIVER_H
#define W5500_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "../CONFIG.h"

// Public API

/**
 * @brief Initialize the W5500 module, SPI interface, and attach the global interrupt handler.
 * @return 0 on success, -1 on error.
 */
int w5500_init(void);

/**
 * @brief Reset the W5500 chip.
 */
void w5500_reset(void);

/**
 * @brief Set the network configuration (MAC, IP, subnet mask, gateway).
 * @param mac Pointer to 6-byte MAC address.
 * @param ip Pointer to 4-byte IP address.
 * @param subnet Pointer to 4-byte subnet mask.
 * @param gateway Pointer to 4-byte gateway.
 * @return 0 on success, -1 on error.
 */
int w5500_set_network(const uint8_t *mac, const uint8_t *ip, const uint8_t *subnet, const uint8_t *gateway);

/**
 * @brief Open a socket using the specified protocol and local port.
 * @param sock Socket index (0..MAX_SOCKETS-1).
 * @param protocol Socket protocol (e.g. SOCKET_MR_TCP).
 * @param port Local port number.
 * @return 0 on success, -1 on error.
 */
int w5500_socket_open(uint8_t sock, uint8_t protocol, uint16_t port);

/**
 * @brief Close a socket.
 * @param sock Socket index.
 * @return 0 on success, -1 on error.
 */
int w5500_socket_close(uint8_t sock);

/**
 * @brief Send data through a socket (handles TX buffer wrap–around).
 * @param sock Socket index.
 * @param data Pointer to data to send.
 * @param len Number of bytes to send.
 * @return 0 on success, -1 on error.
 */
int w5500_socket_send(uint8_t sock, const uint8_t *data, uint16_t len);

/**
 * @brief Receive data from a socket (with wrap–around logic).
 * @param sock Socket index.
 * @param buffer Buffer to store received data.
 * @param bufsize Size of the buffer.
 * @return Number of bytes read (0 if none) or -1 on error.
 */
int w5500_socket_recv(uint8_t sock, uint8_t *buffer, uint16_t bufsize);

/**
 * @brief Process any interrupt events for a given socket. This function is called
 *        from the global interrupt handler.
 * @param sock Socket index.
 */
void w5500_socket_isr(uint8_t sock);

/**
 * @brief Global interrupt handler for the W5500 (attached to the W5500_INT pin).
 *        It calls the per–socket ISR for each socket.
 * @param gpio The GPIO pin that triggered the interrupt.
 * @param events The interrupt events.
 */
void w5500_global_int_handler(unsigned int gpio, uint32_t events);

/**
 * @brief HTTP server task. This function opens the designated socket (typically HTTP_SOCKET)
 *        on port 80, listens for connections, and serves HTML files from flash.
 * @param sock Socket index to use.
 */
void w5500_http_server_task(uint8_t sock);

/**
 * @brief Process a PDU command (dummy implementation for now).
 * @param command Command code.
 */
void w5500_process_pdu_command(uint8_t command);

#endif // W5500_DRIVER_H
