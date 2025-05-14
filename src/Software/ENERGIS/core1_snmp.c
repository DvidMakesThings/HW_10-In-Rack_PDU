/**
 * @file core1_task.c
 * @brief Ethernet server task for ENERGIS PDU.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 *
 * This file implements the Ethernet server task for the ENERGIS PDU. It
 * initializes the W5500 Ethernet controller and runs an HTTP server on
 * port 80 to serve the PDU UI pages embedded via `html_files.h`.
 */

#include "core1_task.h"
#include "network/snmp.h"
#include "network/snmp_custom.h"
#include "socket.h"

void core1_snmp_task(void);

// Main task function for handling Ethernet server operations
void core1_task(void) {
    char header[128];         // Buffer for HTTP response header
    int header_len;           // Length of the HTTP response header
    uint16_t port = 80;       // Port number for the server
    char request_buffer[256]; // Buffer for storing incoming HTTP requests

    uint8_t server_socket = 0; // Socket identifier

    // Initialize the server socket
    while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
        ERROR_PRINT("Socket creation failed. Retrying...\n");
        sleep_ms(500); // Retry after a short delay
    }

    // Start listening for incoming connections
    if (listen(server_socket) != SOCK_OK) {
        ERROR_PRINT("Listen failed\n");
        close(server_socket); // Close the socket if listening fails
        return;
    }

    INFO_PRINT("Ethernet server started on port %d\n", port);

    uint8_t last_status = 0xFF; // Variable to track the last socket status

    // Main server loop
    while (1) {
        uint8_t status = getSn_SR(server_socket); // Get the current socket status

        // Log status changes for debugging
        if (status != last_status) {
            DEBUG_PRINT("Socket Status Changed: 0x%02X\n", status);
            last_status = status;
        }

        // Handle different socket states
        switch (status) {
        case SOCK_ESTABLISHED: // Client connected
            INFO_PRINT("Client connected.\n");

            // Clear the connection established flag
            if (getSn_IR(server_socket) & Sn_IR_CON) {
                setSn_IR(server_socket, Sn_IR_CON);
                DEBUG_PRINT("Connection established flag cleared.\n");
            }

            // Receive data from the client
            int32_t recv_len =
                recv(server_socket, (uint8_t *)request_buffer, sizeof(request_buffer) - 1);
            if (recv_len > 0) {
                request_buffer[recv_len] = '\0'; // Null-terminate the received data
                DEBUG_PRINT("HTTP Request: %s\n", request_buffer);

                // Generate the HTTP response
                const char *page_content = get_page_content(request_buffer); // Get the page content
                int page_size = strlen(page_content);

                // Create the HTTP response header
                header_len = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n"
                                      "Content-Length: %d\r\n"
                                      "Connection: close\r\n"
                                      "\r\n",
                                      page_size);

                INFO_PRINT("Sending HTTP response. Page size: %d bytes\n", page_size);

                // Send the HTTP response header and content
                send(server_socket, (uint8_t *)header, header_len);
                send(server_socket, (uint8_t *)page_content, page_size);

                // Close the connection after sending the response
                close(server_socket);
                INFO_PRINT("Connection closed. Waiting for next request...\n");

                // Ensure the socket is released before reinitialization
                sleep_ms(50);

                // Reinitialize the socket for the next connection
                while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
                    ERROR_PRINT("Socket re-initialization failed. Retrying...\n");
                    sleep_ms(500);
                }

                listen(server_socket); // Start listening again
            }
            break;

        case SOCK_CLOSE_WAIT: // Client disconnected
            INFO_PRINT("Client disconnected, waiting for final data...\n");

            // Ensure all data is received before closing the socket
            while (recv(server_socket, (uint8_t *)request_buffer, sizeof(request_buffer)) > 0)
                ;

            INFO_PRINT("Closing socket...\n");
            close(server_socket); // Close the socket

            sleep_ms(50); // Short delay to ensure the socket is fully released

            // Reinitialize the socket for the next connection
            while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
                ERROR_PRINT("Socket re-initialization failed. Retrying...\n");
                sleep_ms(500);
            }

            listen(server_socket); // Start listening again
            break;

        case SOCK_CLOSED: // Socket unexpectedly closed
            WARNING_PRINT("Socket unexpectedly closed, restarting...\n");

            // Ensure the socket is fully closed before reinitializing
            sleep_ms(50);

            // Reinitialize the socket
            while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
                ERROR_PRINT("Socket re-initialization failed. Retrying...\n");
                sleep_ms(500);
            }

            listen(server_socket); // Start listening again
            break;

        default:
            break; // Avoid log spam for unhandled states
        }

        sleep_ms(10); // Short delay to prevent high CPU usage
    }

    core1_snmp_task(); // Call the SNMP task function
}

void core1_snmp_task(void) {
    uint8_t managerIP[4] = {192, 168, 0, 100}; // Example Manager IP
    uint8_t agentIP[4] = {192, 168, 0, 101};   // Example Agent IP
    uint8_t snmp_agent_socket = 1;             // SNMP Agent socket identifier
    uint8_t snmp_trap_socket = 2;              // SNMP Trap socket identifier

    // Initialize SNMP Daemon
    snmpd_init(managerIP, agentIP, snmp_agent_socket, snmp_trap_socket);

    // Run the SNMP daemon to handle incoming SNMP requests
    snmpd_run();

    // Handle SNMP time updates
    SNMP_time_handler();

    // Short delay to prevent high CPU usage
    sleep_ms(10);
}
