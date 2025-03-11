#include "W5500_RP2040_HTTP.h"
#include <stdio.h> 
#include <string.h> 
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "../CONFIG.h"

// Debug Print Helper
#define DEBUG 1
#define DEBUG_PRINT(...) if (DEBUG) printf(__VA_ARGS__);

// SPI Communication
void w5500_select() {
    gpio_put(W5500_CS, 0);
    //DEBUG_PRINT("\t[SPI] CS Low (Select)\n");
}

void w5500_deselect() {
    gpio_put(W5500_CS, 1);
    //DEBUG_PRINT("\t[SPI] CS High (Deselect)\n");
}

// Write to W5500 Register
void w5500_write_reg(uint16_t addr, uint8_t value) {
    uint8_t tx_buf[4] = { (addr >> 8) & 0xFF, addr & 0xFF, 0x04, value };
    w5500_select();
    spi_write_blocking(W5500_SPI_INSTANCE, tx_buf, 4);
    w5500_deselect();
    //DEBUG_PRINT("\t[SPI] Write 0x%02X to register 0x%04X\n", value, addr);
}

// Read from W5500 Register
uint8_t w5500_read_reg(uint16_t addr) {
    uint8_t tx_buf[3] = { (addr >> 8) & 0xFF, addr & 0xFF, 0x00 };
    uint8_t rx_buf[1] = {0};
    w5500_select();
    spi_write_blocking(W5500_SPI_INSTANCE, tx_buf, 3);
    spi_read_blocking(W5500_SPI_INSTANCE, 0xFF, rx_buf, 1);
    w5500_deselect();
    //DEBUG_PRINT("\t[SPI] Read 0x%02X from register 0x%04X\n", rx_buf[0], addr);
    return rx_buf[0];
}

// Write 16-bit Register
void w5500_write_reg_16(uint16_t addr, uint16_t value) {
    w5500_write_reg(addr, (value >> 8) & 0xFF);
    w5500_write_reg(addr + 1, value & 0xFF);
    //DEBUG_PRINT("\t[SPI] Write 16-bit value 0x%04X to register 0x%04X\n", value, addr);
}

// Read 16-bit Register
uint16_t w5500_read_reg_16(uint16_t addr) {
    uint16_t high = w5500_read_reg(addr);
    uint16_t low = w5500_read_reg(addr + 1);
    uint16_t value = (high << 8) | low;
    //DEBUG_PRINT("\t[SPI] Read 16-bit value 0x%04X from register 0x%04X\n", value, addr);
    return value;
}

// Send Socket Command
void w5500_send_socket_command(uint8_t socket, uint8_t command) {
    w5500_write_reg(W5500_Sn_CR(socket), command);
    DEBUG_PRINT("[SOCKET] Sent command 0x%02X to socket %d\n", command, socket);
}

// Check if PHY Link is Established
bool w5500_check_phy() {
    uint8_t phy_status = w5500_read_reg(W5500_PHYCFGR);
    DEBUG_PRINT("[PHY] W5500 PHYCFGR: 0x%02X\n", phy_status);
    if (phy_status & 0x01) {
        DEBUG_PRINT("[PHY] Link is up\n");
        return true;
    } else {
        DEBUG_PRINT("[PHY] Link is down\n");
        return false;
    }
}

// Initialize W5500
bool w5500_init() {
    DEBUG_PRINT("W5500 Init begin...\n");
    uint8_t version = w5500_read_reg(W5500_VERSIONR);
    DEBUG_PRINT("[INFO] W5500 Version: 0x%02X\n", version);

    if (version != 0x04) {
        DEBUG_PRINT("[ERROR] W5500 version mismatch!\n");
        return false;
    }

    if (!w5500_check_phy()) {
        DEBUG_PRINT("[ERROR] Ethernet cable not detected!\n");
        return false;
    }

    DEBUG_PRINT("[INFO] W5500 initialized successfully.\n");
    return true;
}

// Set MAC Address
void w5500_set_mac(const uint8_t* mac) {
    DEBUG_PRINT("[MAC] Setting MAC address: ");
    for (int i = 0; i < 6; i++) {
        w5500_write_reg(W5500_SHAR + i, mac[i]);
        DEBUG_PRINT("%02X ", mac[i]);
    }
    DEBUG_PRINT("\n");
}

// Set IP Address
void w5500_set_ip(const uint8_t* ip) {
    DEBUG_PRINT("[IP] Setting IP address: ");
    for (int i = 0; i < 4; i++) {
        w5500_write_reg(W5500_SIPR + i, ip[i]);
        DEBUG_PRINT("%d.", ip[i]);
    }
    DEBUG_PRINT("\n");
}

// Set Subnet Mask
void w5500_set_subnet(const uint8_t* subnet) {
    DEBUG_PRINT("[SUBNET] Setting Subnet mask: ");
    for (int i = 0; i < 4; i++) {
        w5500_write_reg(W5500_SUBR + i, subnet[i]);
        DEBUG_PRINT("%d.", subnet[i]);
    }
    DEBUG_PRINT("\n");
}

// Set Gateway Address
void w5500_set_gateway(const uint8_t* gateway) {
    DEBUG_PRINT("[GATEWAY] Setting Gateway address: ");
    for (int i = 0; i < 4; i++) {
        w5500_write_reg(W5500_GAR + i, gateway[i]);
        DEBUG_PRINT("%d.", gateway[i]);
    }
    DEBUG_PRINT("\n");
}

// Set memory for each socket
void w5500_socket_memory_setup(uint8_t socket) {
    // Set the RX and TX memory sizes (2KB each)
    w5500_write_reg_16(W5500_Sn_RX_RSR(socket), 0x0200);  // Set 2KB for RX buffer
    w5500_write_reg_16(W5500_Sn_TX_FSR(socket), 0x0200);  // Set 2KB for TX buffer
    
    // No need to manually set base addresses unless explicitly required
    // The W5500 handles this automatically, so no need to write to Sn_TX_BASE or Sn_RX_BASE
    DEBUG_PRINT("[SOCKET] Set 2KB RX and TX memory for socket %d\n", socket);
}

// Define the critical section functions for disabling/enabling interrupts
void SPI_CRITICAL_ENTER(void) {
    // This function disables interrupts to protect the SPI transaction
    // Depending on your MCU or platform, you can disable interrupts like this:
    // __disable_irq(); // For ARM Cortex-M or other microcontrollers with IRQs
    __asm volatile ("cpsid i");  // For ARM Cortex-M, this disables interrupts
}

void SPI_CRITICAL_EXIT(void) {
    // This function re-enables interrupts after the SPI transaction
    // Depending on your MCU or platform, you can enable interrupts like this:
    // __enable_irq(); // For ARM Cortex-M or other microcontrollers with IRQs
    __asm volatile ("cpsie i");  // For ARM Cortex-M, this enables interrupts
}

int w5500_socket_open(uint8_t protocol) {
    for (int i = 0; i < 8; i++) {
        uint8_t sock_state = w5500_read_reg(W5500_Sn_SR(i));  // Check socket state
        w5500_write_reg(W5500_Sn_IR(i), 0xFF);  // Clear all interrupt0

        if (sock_state == W5500_SOCK_CLOSED) {
            while (1) {
                // Set TCP mode (Sn_MR = 0x01)
                SPI_CRITICAL_ENTER();
                w5500_write_reg(W5500_Sn_MR(i), protocol);  // Set the socket mode (TCP)
                DEBUG_PRINT("[SOCKET] Setting socket mode TCP for socket %d\n", i);
                SPI_CRITICAL_EXIT();

                // Set source port (Sn_PORT0)
                SPI_CRITICAL_ENTER();
                w5500_write_reg_16(W5500_Sn_PORT(i), SERVER_PORT);  // Set the source port (8081)
                DEBUG_PRINT("[SOCKET] Setting source port (%d) for socket %d\n", SERVER_PORT, i);
                SPI_CRITICAL_EXIT();

                // Send OPEN command (Sn_CR = OPEN)
                SPI_CRITICAL_ENTER();
                w5500_send_socket_command(i, W5500_CMD_OPEN);  // Open the socket
                DEBUG_PRINT("[SOCKET] Sent OPEN command (0x01) to socket %d\n", i);
                SPI_CRITICAL_EXIT();

                // Wait for Sn_SR to change to SOCK_INIT
                uint8_t cr_status = w5500_read_reg(W5500_Sn_CR(i));  // Read Sn_CR status
                uint8_t ir_status;
                uint8_t sock_state_check;

                // Loop to check if Sn_CR is cleared or Sn_SR is SOCK_INIT
                while (cr_status != 0x00 || (sock_state_check = w5500_read_reg(W5500_Sn_SR(i))) != W5500_SOCK_INIT) {
                    if (cr_status != 0x00) {
                        DEBUG_PRINT("[SOCKET] Sn_CR not cleared, retrying...\n");
                    }

                    // Check interrupt status to clear flags
                    SPI_CRITICAL_ENTER();
                    ir_status = w5500_read_reg(W5500_Sn_IR(i));  // Read interrupt register
                    if (ir_status & 0x01) {  // SEND_OK interrupt
                        w5500_write_reg(W5500_Sn_IR(i), 0x01);  // Clear SEND_OK interrupt
                        DEBUG_PRINT("[SOCKET] Cleared SEND_OK interrupt\n");
                    }
                    if (ir_status & 0x02) {  // TIMEOUT interrupt
                        w5500_write_reg(W5500_Sn_IR(i), 0x02);  // Clear TIMEOUT interrupt
                        DEBUG_PRINT("[SOCKET] Cleared TIMEOUT interrupt\n");
                    }
                    SPI_CRITICAL_EXIT();

                    // If the state isn't SOCK_INIT, retry by closing and reopening the socket
                    if (sock_state_check != W5500_SOCK_INIT) {
                        SPI_CRITICAL_ENTER();
                        w5500_send_socket_command(i, W5500_CMD_CLOSE);  // Close the socket to reset
                        DEBUG_PRINT("[SOCKET] Retrying socket %d (CLOSE -> OPEN) with state: %02X\n\n", i, sock_state_check);
                        SPI_CRITICAL_EXIT();
                        sleep_ms(500);  // Wait a bit before retrying

                        // Reopen the socket
                        SPI_CRITICAL_ENTER();
                        w5500_send_socket_command(i, W5500_CMD_OPEN);  // Open the socket again
                        DEBUG_PRINT("[SOCKET] Sent OPEN command (0x01) to socket %d\n", i);
                        SPI_CRITICAL_EXIT();
                    }

                    cr_status = w5500_read_reg(W5500_Sn_CR(i));  // Re-read Sn_CR status
                    sock_state_check = w5500_read_reg(W5500_Sn_SR(i));  // Re-check socket state

                    DEBUG_PRINT("[SOCKET] Sn_CR status: %02X, Sock state: %02X, IR status: %02X\n", cr_status, sock_state_check, ir_status);
                    sleep_ms(500);  // Wait 500ms before retrying
                }

                if (sock_state_check == W5500_SOCK_INIT) {
                    DEBUG_PRINT("[SOCKET] Socket %d opened successfully\n", i);
                    return i;  // Successfully opened the socket
                }
            }
        }
    }
    DEBUG_PRINT("[SOCKET] No available sockets\n");
    return -1;  // No available socket
}

// Close Socket
void w5500_socket_close(uint8_t socket) {
    w5500_send_socket_command(socket, W5500_CMD_CLOSE);
    DEBUG_PRINT("[SOCKET] Socket %d closed\n", socket);
}

// Set Socket to Listen Mode
void w5500_socket_listen(uint8_t socket) {
    w5500_send_socket_command(socket, W5500_CMD_LISTEN);
    uint8_t sock_state = w5500_read_reg(W5500_Sn_SR(socket));
    if (sock_state != W5500_SOCK_LISTEN) {
        printf("Socket %d failed to enter LISTEN mode. State: %02X\n", socket, sock_state);
    } else {
        printf("Socket %d set to LISTEN mode\n", socket);
    }
}
// Send Data
void w5500_socket_send(uint8_t socket, const uint8_t* data, uint16_t len) {
    uint16_t free_size = w5500_read_reg_16(W5500_Sn_TX_FSR(socket));
    DEBUG_PRINT("[SOCKET] Free size for socket %d: %d bytes\n", socket, free_size);
    if (free_size < len) {
        DEBUG_PRINT("[SOCKET] Not enough space in TX buffer\n");
        return;
    }

    uint16_t write_ptr = w5500_read_reg_16(W5500_Sn_TX_WR(socket));
    DEBUG_PRINT("[SOCKET] TX write pointer: 0x%04X\n", write_ptr);
    for (uint16_t i = 0; i < len; i++) {
        w5500_write_reg_16(write_ptr + i, data[i]);
        DEBUG_PRINT("[SOCKET] Writing data byte 0x%02X to TX buffer at offset 0x%04X\n", data[i], write_ptr + i);
    }

    write_ptr += len;
    w5500_write_reg_16(W5500_Sn_TX_WR(socket), write_ptr);
    w5500_send_socket_command(socket, W5500_CMD_SEND);
    DEBUG_PRINT("[SOCKET] Data sent, updated TX write pointer to 0x%04X\n", write_ptr);
}

// Receive Data
uint16_t w5500_socket_recv(uint8_t socket, uint8_t* buffer, uint16_t len) {
    uint16_t available = w5500_read_reg_16(W5500_Sn_RX_RSR(socket));
    DEBUG_PRINT("[SOCKET] Available data for socket %d: %d bytes\n", socket, available);
    if (available > 0) {
        uint16_t size_to_read = (available > len) ? len : available;
        uint16_t read_ptr = w5500_read_reg_16(W5500_Sn_RX_RD(socket));
        DEBUG_PRINT("[SOCKET] RX read pointer: 0x%04X\n", read_ptr);

        for (uint16_t i = 0; i < size_to_read; i++) {
            buffer[i] = w5500_read_reg_16(read_ptr + i);
            //DEBUG_PRINT("[SOCKET] Reading data byte 0x%02X from RX buffer at offset 0x%04X\n", buffer[i], read_ptr + i);
        }

        w5500_write_reg_16(W5500_Sn_RX_RD(socket), read_ptr + size_to_read);
        w5500_send_socket_command(socket, W5500_CMD_RECV);
        DEBUG_PRINT("[SOCKET] Data received, updated RX read pointer to 0x%04X\n", read_ptr + size_to_read);
        return size_to_read;
    }

    DEBUG_PRINT("[SOCKET] No data available to receive\n");
    return 0;
}

// Initialize HTTP Server
void http_server_init(http_server_t* server, uint16_t port) {
    memset(server, 0, sizeof(http_server_t));  // Initialize the server structure
    server->port = port;

    DEBUG_PRINT("[INFO] HTTP Server Initialized on port %d\n", port);
}

// Register Route for HTTP Server
void http_server_register_route(http_server_t* server, const char* path, int methods, void (*handler)(http_request_t* request, http_response_t* response)) {
    if (server->route_count < HTTP_MAX_ROUTES) {
        http_route_t* route = &server->routes[server->route_count++];
        strncpy(route->path, path, HTTP_MAX_URL_SIZE);
        route->methods = methods;
        route->handler = handler;

        DEBUG_PRINT("[INFO] Registered route: %s with methods: %d\n", path, methods);
    } else {
        DEBUG_PRINT("[ERROR] Maximum route count reached, cannot register route: %s\n", path);
    }
}

// Start HTTP Server (open socket and listen)
bool http_server_start(http_server_t* server) {
    int socket = w5500_socket_open(W5500_SOCK_MR_TCP);  // Open socket for TCP mode
    if (socket < 0) {
        printf("Failed to open socket\n");
        return false;
    }

    server->socket = socket;

    // Set the socket port to the server's port
    w5500_write_reg_16(W5500_Sn_PORT(socket), SERVER_PORT);
    w5500_send_socket_command(socket, W5500_CMD_OPEN);  // Open socket

    // Wait for the socket to be in INIT state
    uint8_t sock_state = w5500_read_reg(W5500_Sn_SR(socket));
    if (sock_state != W5500_SOCK_INIT) {
        printf("Socket %d failed to initialize. State: 0x%02X\n", socket, sock_state);
        return false;
    }

    // Set socket to LISTEN mode
    w5500_send_socket_command(socket, W5500_CMD_LISTEN);

    // Check if the socket is in LISTEN state
    sock_state = w5500_read_reg(W5500_Sn_SR(socket));
    if (sock_state != W5500_SOCK_LISTEN) {
        printf("Socket %d failed to enter LISTEN mode. State: 0x%02X\n", socket, sock_state);
        return false;
    }

    printf("Socket %d opened and set to LISTEN mode\n", socket);
    return true;
}

void w5500_socket_process(uint8_t socket) {
    uint8_t sock_state = w5500_read_reg(W5500_Sn_SR(socket));
    if (sock_state == W5500_SOCK_ESTABLISHED) {
        // Data can be received and sent
        uint8_t buffer[2048];  // Adjust buffer size as needed
        uint16_t data_len = w5500_socket_recv(socket, buffer, sizeof(buffer));

        if (data_len > 0) {
            // Process received data (e.g., handle HTTP request)
        }

        // If there is data to send, send it
        uint8_t response[] = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
        w5500_socket_send(socket, response, sizeof(response) - 1);
    } else if (sock_state == W5500_SOCK_CLOSE_WAIT) {
        // Handle disconnection (e.g., by sending a FIN packet)
        w5500_send_socket_command(socket, W5500_CMD_CLOSE);
    }
}

// Process HTTP Server Requests
void http_server_process(http_server_t* server) {
    if (w5500_socket_available(server->socket)) {
        uint8_t buffer[1024];  // Adjust buffer size as necessary
        uint16_t bytes_received = w5500_socket_recv(server->socket, buffer, sizeof(buffer));

        DEBUG_PRINT("[DEBUG] Received %d bytes\n", bytes_received);  // Add debug print

        if (bytes_received > 0) {
            http_request_t request;
            http_response_t response;

            // Parse the request and respond
            http_request_parse(&request, (char*)buffer, bytes_received);
            for (int i = 0; i < server->route_count; i++) {
                if (path_matches(server->routes[i].path, request.url) && (server->routes[i].methods & request.method)) {
                    server->routes[i].handler(&request, &response);
                    break;
                }
            }

            // Send response
            http_response_send(&response, server->socket);
        }
    }
}

void http_request_parse(http_request_t* req, const uint8_t* data, size_t len) {
    DEBUG_PRINT("[DEBUG] Raw data received: %.*s\n", (int)len, data);

    // Directly parse the HTTP method and URL from the data
    int num = sscanf((const char*)data, "%s %s", req->url, &req->method);  

    if (num == 2) {
        // Convert the HTTP method string into the corresponding integer value
        req->method = http_method_from_string((const char*)data);  

        DEBUG_PRINT("[DEBUG] Parsed method: %d, URL: %s\n", req->method, req->url);
    } else {
        DEBUG_PRINT("[ERROR] Invalid HTTP request format\n");
    }
}

// Convert HTTP method string to the corresponding method enum value
int http_method_from_string(const char* method_str) {
    if (strcmp(method_str, "GET") == 0) {
        return HTTP_METHOD_GET;
    } else if (strcmp(method_str, "POST") == 0) {
        return HTTP_METHOD_POST;
    } else if (strcmp(method_str, "PUT") == 0) {
        return HTTP_METHOD_PUT;
    } else if (strcmp(method_str, "DELETE") == 0) {
        return HTTP_METHOD_DELETE;
    } else if (strcmp(method_str, "OPTIONS") == 0) {
        return HTTP_METHOD_OPTIONS;
    } else if (strcmp(method_str, "HEAD") == 0) {
        return HTTP_METHOD_HEAD;
    } else {
        return -1;  // Invalid method
    }
}

void http_response_set_status(http_response_t* response, int status_code) {
    response->status = status_code;  // Set status (e.g., 200 OK)
}

void http_response_set_body(http_response_t* response, const char* body, size_t length) {
    memset(response->body, 0, HTTP_MAX_BODY_SIZE);  // Clear any previous data
    strncpy(response->body, body, length);  // Copy new body into response
    response->body_length = length;  // Set the body length
}

void http_response_send(http_response_t* response, uint8_t socket) {
    // Send headers (status, content-length, etc.)
    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d OK\r\n"
             "Content-Length: %d\r\n"
             "Content-Type: text/html\r\n"
             "\r\n", 
             response->status, response->body_length);
    
    // Send the header and body
    w5500_socket_send(socket, (uint8_t*)header, strlen(header));  // Send the header
    w5500_socket_send(socket, (uint8_t*)response->body, response->body_length);  // Send the body
}

uint16_t w5500_socket_available(uint8_t socket) {
    return w5500_read_reg_16(W5500_Sn_RX_RSR(socket));
}

// Simple path matching function (can be expanded for more complex matching)
bool path_matches(const char* route, const char* path) {
    return strcmp(route, path) == 0;
}


