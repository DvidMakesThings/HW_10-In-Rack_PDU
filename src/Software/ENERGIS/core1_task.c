#include "core1_task.h"

void core1_task(void) {
    char header[128];
    int header_len;
    uint16_t port = 80;
    char request_buffer[256];

    uint8_t server_socket = 0;

    // Initialize socket
    while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
        ERROR_PRINT("Socket creation failed. Retrying...\n");
        sleep_ms(500);
    }

    if (listen(server_socket) != SOCK_OK) {
        ERROR_PRINT("Listen failed\n");
        close(server_socket);
        return;
    }

    INFO_PRINT("Ethernet server started on port %d\n", port);

    uint8_t last_status = 0xFF;

    while (1) {
        uint8_t status = getSn_SR(server_socket);

        if (status != last_status) {
            DEBUG_PRINT("Socket Status Changed: 0x%02X\n", status);
            last_status = status;
        }

        switch (status) {
        case SOCK_ESTABLISHED:
            INFO_PRINT("Client connected.\n");

            if (getSn_IR(server_socket) & Sn_IR_CON) {
                setSn_IR(server_socket, Sn_IR_CON);
                DEBUG_PRINT("Connection established flag cleared.\n");
            }

            int32_t recv_len =
                recv(server_socket, (uint8_t *)request_buffer, sizeof(request_buffer) - 1);
            if (recv_len > 0) {
                request_buffer[recv_len] = '\0'; // Null-terminate request
                DEBUG_PRINT("HTTP Request: %s\n", request_buffer);

                const char *page_content = get_page_content(request_buffer);
                int page_size = strlen(page_content);

                header_len = snprintf(header, sizeof(header),
                                      "HTTP/1.1 200 OK\r\n"
                                      "Content-Type: text/html\r\n"
                                      "Content-Length: %d\r\n"
                                      "Connection: close\r\n"
                                      "\r\n",
                                      page_size);

                INFO_PRINT("Sending HTTP response. Page size: %d bytes\n", page_size);

                send(server_socket, (uint8_t *)header, header_len);
                send(server_socket, (uint8_t *)page_content, page_size);
                close(server_socket);
                INFO_PRINT("Connection closed. Waiting for next request...\n");

                // Ensure socket is released before reinitialization
                sleep_ms(50);

                // Reinitialize socket
                while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
                    ERROR_PRINT("Socket re-initialization failed. Retrying...\n");
                    sleep_ms(500);
                }

                listen(server_socket);
            }
            break;

        case SOCK_CLOSE_WAIT:
            INFO_PRINT("Client disconnected, waiting for final data...\n");

            // Ensure all data is received before closing
            while (recv(server_socket, (uint8_t *)request_buffer, sizeof(request_buffer)) > 0)
                ;

            INFO_PRINT("Closing socket...\n");
            close(server_socket);

            sleep_ms(50); // Short delay to ensure socket is fully released

            // Reinitialize socket
            while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
                ERROR_PRINT("Socket re-initialization failed. Retrying...\n");
                sleep_ms(500);
            }

            listen(server_socket);
            break;

        case SOCK_CLOSED:
            WARNING_PRINT("Socket unexpectedly closed, restarting...\n");

            // Ensure socket is fully closed before reinitializing
            sleep_ms(50);

            while (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
                ERROR_PRINT("Socket re-initialization failed. Retrying...\n");
                sleep_ms(500);
            }

            listen(server_socket);
            break;

        default:
            break; // Avoid log spam
        }

        sleep_ms(10);
    }
}
