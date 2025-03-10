/**
 * W5500 Ethernet HTTP Server Library for RP2040 - Implementation
 * 
 * This file contains the implementation of the W5500 Ethernet controller interface
 * and HTTP server functionality for the RP2040 microcontroller.
 *
 * Copyright (c) 2025
 */

#include "w5500_rp2040_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Global Variables
static w5500_config_t w5500_config;
static http_server_config_t http_config;
static bool server_running = false;
static uint8_t sockets_in_use[W5500_MAX_SOCKETS] = {0};
static uint8_t rx_buffer[W5500_MAX_BUFFER_SIZE];

//=====================================================================
// W5500 Hardware Interface Implementation
//=====================================================================

/**
 * Initialize the W5500 hardware
 */
void w5500_init(w5500_config_t *config) {
    // Store configuration
    memcpy(&w5500_config, config, sizeof(w5500_config_t));
    
    // Initialize SPI
    spi_init(config->spi, config->spi_baudrate);
    
    // Configure GPIOs
    gpio_set_function(spi_get_tx_pin(config->spi), GPIO_FUNC_SPI);
    gpio_set_function(spi_get_rx_pin(config->spi), GPIO_FUNC_SPI);
    gpio_set_function(spi_get_sck_pin(config->spi), GPIO_FUNC_SPI);
    
    // Configure CS pin
    gpio_init(config->cs_pin);
    gpio_set_dir(config->cs_pin, GPIO_OUT);
    gpio_put(config->cs_pin, 1);
    
    // Configure RST pin if defined
    if (config->rst_pin != 0xFF) {
        gpio_init(config->rst_pin);
        gpio_set_dir(config->rst_pin, GPIO_OUT);
        gpio_put(config->rst_pin, 1);
    }
    
    // Configure INT pin if defined
    if (config->int_pin != 0xFF) {
        gpio_init(config->int_pin);
        gpio_set_dir(config->int_pin, GPIO_IN);
        gpio_pull_up(config->int_pin);
    }
    
    // Reset the W5500
    w5500_reset();
    
    // Wait for W5500 to initialize
    sleep_ms(100);
    
    // Configure W5500 network settings
    
    // Set MAC address
    w5500_write_bytes(W5500_SHAR, W5500_BSB_COMMON_REG, config->mac, 6);
    
    // Set Gateway IP
    w5500_write_bytes(W5500_GAR, W5500_BSB_COMMON_REG, config->gateway, 4);
    
    // Set Subnet mask
    w5500_write_bytes(W5500_SUBR, W5500_BSB_COMMON_REG, config->subnet, 4);
    
    // Set IP address
    w5500_write_bytes(W5500_SIPR, W5500_BSB_COMMON_REG, config->ip, 4);
    
    // Set retry time and count
    w5500_write_byte(W5500_RTR + 0, W5500_BSB_COMMON_REG, 0x07); // 1.8s timeout
    w5500_write_byte(W5500_RTR + 1, W5500_BSB_COMMON_REG, 0xD0);
    w5500_write_byte(W5500_RCR, W5500_BSB_COMMON_REG, 8); // 8 retries
    
    // Set socket buffer size: 2KB for each socket
    for (int i = 0; i < W5500_MAX_SOCKETS; i++) {
        w5500_write_byte(W5500_Sn_RXBUF_SIZE(i), W5500_BSB_SOCKET_REG(i), 2); // 2KB RX buffer
        w5500_write_byte(W5500_Sn_TXBUF_SIZE(i), W5500_BSB_SOCKET_REG(i), 2); // 2KB TX buffer
    }
    
    // Initialize socket states
    for (int i = 0; i < W5500_MAX_SOCKETS; i++) {
        sockets_in_use[i] = 0;
    }
}

/**
 * Reset the W5500 chip
 */
void w5500_reset(void) {
    if (w5500_config.rst_pin != 0xFF) {
        // Hardware reset
        gpio_put(w5500_config.rst_pin, 0);
        sleep_ms(10);
        gpio_put(w5500_config.rst_pin, 1);
        sleep_ms(50);
    } else {
        // Software reset
        w5500_write_byte(W5500_MR, W5500_BSB_COMMON_REG, 0x80);
        sleep_ms(50);
    }
}

/**
 * Select the W5500 chip (CS low)
 */
void w5500_select(void) {
    gpio_put(w5500_config.cs_pin, 0);
}

/**
 * Deselect the W5500 chip (CS high)
 */
void w5500_deselect(void) {
    gpio_put(w5500_config.cs_pin, 1);
}

/**
 * Write a single byte to the W5500
 */
void w5500_write_byte(uint16_t address, uint8_t block, uint8_t data) {
    uint8_t control[3];
    
    // Prepare control bytes
    control[0] = (address >> 8) & 0xFF;
    control[1] = address & 0xFF;
    control[2] = (block << 3) | W5500_CTRL_WRITE | W5500_CTRL_VDM;
    
    w5500_select();
    spi_write_blocking(w5500_config.spi, control, 3);
    spi_write_blocking(w5500_config.spi, &data, 1);
    w5500_deselect();
}

/**
 * Read a single byte from the W5500
 */
uint8_t w5500_read_byte(uint16_t address, uint8_t block) {
    uint8_t control[3];
    uint8_t data;
    
    // Prepare control bytes
    control[0] = (address >> 8) & 0xFF;
    control[1] = address & 0xFF;
    control[2] = (block << 3) | W5500_CTRL_READ | W5500_CTRL_VDM;
    
    w5500_select();
    spi_write_blocking(w5500_config.spi, control, 3);
    spi_read_blocking(w5500_config.spi, 0xFF, &data, 1);
    w5500_deselect();
    
    return data;
}

/**
 * Write multiple bytes to the W5500
 */
void w5500_write_bytes(uint16_t address, uint8_t block, uint8_t *data, uint16_t len) {
    uint8_t control[3];
    
    // Prepare control bytes
    control[0] = (address >> 8) & 0xFF;
    control[1] = address & 0xFF;
    control[2] = (block << 3) | W5500_CTRL_WRITE | W5500_CTRL_VDM;
    
    w5500_select();
    spi_write_blocking(w5500_config.spi, control, 3);
    spi_write_blocking(w5500_config.spi, data, len);
    w5500_deselect();
}

/**
 * Read multiple bytes from the W5500
 */
void w5500_read_bytes(uint16_t address, uint8_t block, uint8_t *data, uint16_t len) {
    uint8_t control[3];
    
    // Prepare control bytes
    control[0] = (address >> 8) & 0xFF;
    control[1] = address & 0xFF;
    control[2] = (block << 3) | W5500_CTRL_READ | W5500_CTRL_VDM;
    
    w5500_select();
    spi_write_blocking(w5500_config.spi, control, 3);
    spi_read_blocking(w5500_config.spi, 0xFF, data, len);
    w5500_deselect();
}

/**
 * Write to a socket register (one byte)
 */
void w5500_write_socket_register(uint8_t socket, uint16_t address, uint8_t data) {
    if (socket >= W5500_MAX_SOCKETS) return;
    w5500_write_byte(address, W5500_BSB_SOCKET_REG(socket), data);
}

/**
 * Read from a socket register (one byte)
 */
uint8_t w5500_read_socket_register(uint8_t socket, uint16_t address) {
    if (socket >= W5500_MAX_SOCKETS) return 0;
    return w5500_read_byte(address, W5500_BSB_SOCKET_REG(socket));
}

/**
 * Read from a socket register (16-bit value)
 */
uint16_t w5500_read_socket_register_16(uint8_t socket, uint16_t address) {
    uint16_t high, low;
    
    high = w5500_read_socket_register(socket, address);
    low = w5500_read_socket_register(socket, address + 1);
    
    return (high << 8) | low;
}

/**
 * Write to a socket register (16-bit value)
 */
void w5500_write_socket_register_16(uint8_t socket, uint16_t address, uint16_t data) {
    w5500_write_socket_register(socket, address, data >> 8);
    w5500_write_socket_register(socket, address + 1, data & 0xFF);
}

/**
 * Write data to a socket's TX buffer
 */
void w5500_write_tx_buffer(uint8_t socket, uint8_t *data, uint16_t len) {
    uint16_t tx_write_ptr, offset;
    
    if (socket >= W5500_MAX_SOCKETS) return;
    
    // Get TX write pointer
    tx_write_ptr = w5500_read_socket_register_16(socket, W5500_Sn_TX_WR(0) - W5500_SOCKET_REG_BASE(0));
    
    // Calculate offset in TX buffer
    offset = tx_write_ptr & 0x07FF; // Mask off higher bits for offset in 2KB buffer
    
    // Write data to TX buffer
    w5500_write_bytes(offset, W5500_BSB_SOCKET_TX_BUF(socket), data, len);
    
    // Update TX write pointer
    tx_write_ptr += len;
    w5500_write_socket_register_16(socket, W5500_Sn_TX_WR(0) - W5500_SOCKET_REG_BASE(0), tx_write_ptr);
}

/**
 * Read data from a socket's RX buffer
 */
void w5500_read_rx_buffer(uint8_t socket, uint8_t *data, uint16_t len) {
    uint16_t rx_read_ptr, offset;
    
    if (socket >= W5500_MAX_SOCKETS) return;
    
    // Get RX read pointer
    rx_read_ptr = w5500_read_socket_register_16(socket, W5500_Sn_RX_RD(0) - W5500_SOCKET_REG_BASE(0));
    
    // Calculate offset in RX buffer
    offset = rx_read_ptr & 0x07FF; // Mask off higher bits for offset in 2KB buffer
    
    // Read data from RX buffer
    w5500_read_bytes(offset, W5500_BSB_SOCKET_RX_BUF(socket), data, len);
    
    // Update RX read pointer
    rx_read_ptr += len;
    w5500_write_socket_register_16(socket, W5500_Sn_RX_RD(0) - W5500_SOCKET_REG_BASE(0), rx_read_ptr);
}

//=====================================================================
// W5500 Socket Management Implementation
//=====================================================================

/**
 * Initialize a socket with specified protocol and port
 */
bool w5500_socket_init(uint8_t socket, uint8_t protocol, uint16_t port) {
    uint8_t status;
    
    if (socket >= W5500_MAX_SOCKETS) return false;
    
    // Close the socket first if it's already in use
    if (!w5500_socket_close(socket)) return false;
    
    // Set socket mode
    w5500_write_socket_register(socket, W5500_Sn_MR(0) - W5500_SOCKET_REG_BASE(0), protocol);
    
    // Set socket port
    w5500_write_socket_register_16(socket, W5500_Sn_PORT(0) - W5500_SOCKET_REG_BASE(0), port);
    
    // Open the socket
    w5500_write_socket_register(socket, W5500_Sn_CR(0) - W5500_SOCKET_REG_BASE(0), W5500_CMD_OPEN);
    
    // Wait for socket to initialize
    do {
        status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
        if (status == W5500_SOCK_INIT) {
            sockets_in_use[socket] = 1;
            return true;
        }
        sleep_ms(1);
    } while (status != W5500_SOCK_CLOSED);
    
    return false;
}

/**
 * Put a socket in listen mode
 */
/**
 * Put a socket in listen mode
 */
bool w5500_socket_listen(uint8_t socket) {
    uint8_t status;
    
    if (socket >= W5500_MAX_SOCKETS || !sockets_in_use[socket])
        return false;
    
    status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    if (status != W5500_SOCK_INIT)
        return false;
    
    // Send listen command
    w5500_write_socket_register(socket, W5500_Sn_CR(0) - W5500_SOCKET_REG_BASE(0), W5500_CMD_LISTEN);
    
    // Wait for socket to enter listen state
    do {
        status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    } while (status == W5500_SOCK_INIT);
    
    // Check if socket entered LISTEN state
    if (status != W5500_SOCK_LISTEN) {
        return false;
    }
    
    return true;
}

/**
 * Connect a socket to a remote host
 */
bool w5500_socket_connect(uint8_t socket, uint8_t *ip, uint16_t port) {
    uint8_t status;
    
    if (socket >= W5500_MAX_SOCKETS || !sockets_in_use[socket]) return false;
    
    status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    if (status != W5500_SOCK_INIT) return false;
    
    // Set destination IP and port
    w5500_write_bytes(W5500_Sn_DIPR(0) - W5500_SOCKET_REG_BASE(0), W5500_BSB_SOCKET_REG(socket), ip, 4);
    w5500_write_socket_register_16(socket, W5500_Sn_DPORT(0) - W5500_SOCKET_REG_BASE(0), port);
    
    // Send connect command
    w5500_write_socket_register(socket, W5500_Sn_CR(0) - W5500_SOCKET_REG_BASE(0), W5500_CMD_CONNECT);
    
    // Wait for socket to connect (with timeout)
    uint32_t start_time = time_us_32() / 1000;
    uint32_t timeout = 5000; // 5 second timeout
    
    do {
        status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
        if (status == W5500_SOCK_ESTABLISHED) return true;
        sleep_ms(1);
        
        // Check timeout
        if ((time_us_32() / 1000) - start_time > timeout) {
            w5500_socket_close(socket);
            return false;
        }
    } while (status != W5500_SOCK_CLOSED);
    
    return false;
}

/**
 * Disconnect a socket
 */
bool w5500_socket_disconnect(uint8_t socket) {
    uint8_t status;
    
    if (socket >= W5500_MAX_SOCKETS || !sockets_in_use[socket]) return false;
    
    status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    if (status != W5500_SOCK_ESTABLISHED && status != W5500_SOCK_CLOSE_WAIT) return false;
    
    // Send disconnect command
    w5500_write_socket_register(socket, W5500_Sn_CR(0) - W5500_SOCKET_REG_BASE(0), W5500_CMD_DISCON);
    
    // Wait for disconnection (with timeout)
    uint32_t start_time = time_us_32() / 1000;
    uint32_t timeout = 1000; // 1 second timeout
    
    do {
        status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
        if (status == W5500_SOCK_CLOSED || status == W5500_SOCK_INIT) return true;
        sleep_ms(1);
        
        // Check timeout
        if ((time_us_32() / 1000) - start_time > timeout) {
            return false;
        }
    } while (1);
}

/**
 * Close a socket
 */
bool w5500_socket_close(uint8_t socket) {
    uint8_t status;
    
    if (socket >= W5500_MAX_SOCKETS) return false;
    
    // Send close command
    w5500_write_socket_register(socket, W5500_Sn_CR(0) - W5500_SOCKET_REG_BASE(0), W5500_CMD_CLOSE);
    
    // Wait for socket to close (with timeout)
    uint32_t start_time = time_us_32() / 1000;
    uint32_t timeout = 1000; // 1 second timeout
    
    do {
        status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
        if (status == W5500_SOCK_CLOSED) {
            sockets_in_use[socket] = 0;
            return true;
        }
        sleep_ms(1);
        
        // Check timeout
        if ((time_us_32() / 1000) - start_time > timeout) {
            sockets_in_use[socket] = 0;
            return false;
        }
    } while (1);
}

/**
 * Send data through a socket
 */
uint16_t w5500_socket_send(uint8_t socket, uint8_t *data, uint16_t len) {
    uint8_t status;
    uint16_t free_size, ret_val = 0;
    
    if (socket >= W5500_MAX_SOCKETS || !sockets_in_use[socket]) return 0;
    
    status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    if (status != W5500_SOCK_ESTABLISHED && status != W5500_SOCK_CLOSE_WAIT) return 0;
    
    // Check free space in TX buffer
    free_size = w5500_read_socket_register_16(socket, W5500_Sn_TX_FSR(0) - W5500_SOCKET_REG_BASE(0));
    
    // If not enough space, return 0
    if (free_size < len) return 0;
    
    // Write data to TX buffer
    w5500_write_tx_buffer(socket, data, len);
    
    // Send the data
    w5500_write_socket_register(socket, W5500_Sn_CR(0) - W5500_SOCKET_REG_BASE(0), W5500_CMD_SEND);
    
    // Wait for send complete (with timeout)
    uint32_t start_time = time_us_32() / 1000;
    uint32_t timeout = 5000; // 5 second timeout
    
    while (1) {
        uint8_t ir = w5500_read_socket_register(socket, W5500_Sn_IR(0) - W5500_SOCKET_REG_BASE(0));
        
        if (ir & W5500_IR_SEND_OK) {
            // Clear interrupt
            w5500_write_socket_register(socket, W5500_Sn_IR(0) - W5500_SOCKET_REG_BASE(0), W5500_IR_SEND_OK);
            ret_val = len;
            break;
        } else if (ir & W5500_IR_TIMEOUT) {
            // Clear interrupt
            w5500_write_socket_register(socket, W5500_Sn_IR(0) - W5500_SOCKET_REG_BASE(0), W5500_IR_TIMEOUT);
            ret_val = 0;
            break;
        }
        
        // Check timeout
        if ((time_us_32() / 1000) - start_time > timeout) {
            ret_val = 0;
            break;
        }
        
        // Check socket status
        status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
        if (status != W5500_SOCK_ESTABLISHED && status != W5500_SOCK_CLOSE_WAIT) {
            ret_val = 0;
            break;
        }
        
        sleep_ms(1);
    }
    
    return ret_val;
}

/**
 * Receive data from a socket
 */
uint16_t w5500_socket_recv(uint8_t socket, uint8_t *data, uint16_t len) {
    uint8_t status;
    uint16_t rx_size, read_size;
    
    if (socket >= W5500_MAX_SOCKETS || !sockets_in_use[socket]) return 0;
    
    status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    if (status != W5500_SOCK_ESTABLISHED && status != W5500_SOCK_CLOSE_WAIT) return 0;
    
    // Check received data size
    rx_size = w5500_read_socket_register_16(socket, W5500_Sn_RX_RSR(0) - W5500_SOCKET_REG_BASE(0));
    
    if (rx_size == 0) return 0;
    
    // Adjust read size if necessary
    read_size = (rx_size > len) ? len : rx_size;
    
    // Read data from RX buffer
    w5500_read_rx_buffer(socket, data, read_size);
    
    // Send RECV command to update RX buffer
    w5500_write_socket_register(socket, W5500_Sn_CR(0) - W5500_SOCKET_REG_BASE(0), W5500_CMD_RECV);
    
    return read_size;
}

/**
 * Check if socket is established
 */
bool w5500_socket_is_established(uint8_t socket) {
    uint8_t status;
    
    if (socket >= W5500_MAX_SOCKETS) return false;
    
    status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    return (status == W5500_SOCK_ESTABLISHED);
}

/**
 * Check if socket is closed
 */
bool w5500_socket_is_closed(uint8_t socket) {
    uint8_t status;
    
    if (socket >= W5500_MAX_SOCKETS) return false;
    
    status = w5500_read_socket_register(socket, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    return (status == W5500_SOCK_CLOSED);
}

/**
 * Get the amount of data available in a socket's RX buffer
 */
uint16_t w5500_socket_available(uint8_t socket) {
    if (socket >= W5500_MAX_SOCKETS) return 0;
    
    return w5500_read_socket_register_16(socket, W5500_Sn_RX_RSR(0) - W5500_SOCKET_REG_BASE(0));
}

//=====================================================================
// HTTP Server Implementation
//=====================================================================

/**
 * Initialize the HTTP server
 */
void http_server_init(http_server_config_t *config) {
    memcpy(&http_config, config, sizeof(http_server_config_t));
    
    // Initialize endpoint count
    if (config->endpoint_count == 0) {
        http_config.endpoint_count = 0;
    }
    
    // Set default port if not specified
    if (config->port == 0) {
        http_config.port = HTTP_SERVER_PORT;
    }
    
    // Set default max connections if not specified
    if (config->max_connections == 0 || config->max_connections > HTTP_MAX_CLIENTS) {
        http_config.max_connections = HTTP_MAX_CLIENTS;
    }
}

/**
 * Add an endpoint to the HTTP server
 */
void http_server_add_endpoint(const char *path, http_method_t method, http_handler_t handler) {
    if (http_config.endpoint_count >= HTTP_MAX_ENDPOINTS) {
        return;
    }
    
    strncpy(http_config.endpoints[http_config.endpoint_count].path, path, 63);
    http_config.endpoints[http_config.endpoint_count].path[63] = '\0';
    http_config.endpoints[http_config.endpoint_count].method = method;
    http_config.endpoints[http_config.endpoint_count].handler = handler;
    
    http_config.endpoint_count++;
}

/**
 * Start the HTTP server
 */
void http_server_start(void) {
    if (server_running) return;
    
    // Initialize and open server socket
    w5500_socket_init(0, W5500_MR_TCP, http_config.port);
    
    // Start listening
    if (w5500_socket_listen(0)) {
        server_running = true;
    }
}

/**
 * Stop the HTTP server
 */
void http_server_stop(void) {
    if (!server_running) return;
    
    // Close all active client connections
    for (int i = 1; i < http_config.max_connections + 1; i++) {
        if (sockets_in_use[i]) {
            w5500_socket_close(i);
        }
    }
    
    // Close server socket
    w5500_socket_close(0);
    
    server_running = false;
}

/**
 * Process incoming HTTP requests
 */
void http_server_process(void) {
    uint8_t status;
    
    if (!server_running) return;
    
    // Check for new connections on server socket
    status = w5500_read_socket_register(0, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
    
    if (status == W5500_SOCK_LISTEN) {
        // Check socket interrupt for connection
        uint8_t ir = w5500_read_socket_register(0, W5500_Sn_IR(0) - W5500_SOCKET_REG_BASE(0));
        
        if (ir & W5500_IR_CON) {
            // Clear interrupt
            w5500_write_socket_register(0, W5500_Sn_IR(0) - W5500_SOCKET_REG_BASE(0), W5500_IR_CON);
            
            // Find available client socket
            for (int i = 1; i < http_config.max_connections + 1; i++) {
                if (!sockets_in_use[i]) {
                    // Initialize client socket
                    w5500_socket_init(i, W5500_MR_TCP, 0);
                    break;
                }
            }
        }
    }
    
    // Process client sockets
    for (int i = 1; i < http_config.max_connections + 1; i++) {
        if (!sockets_in_use[i]) continue;
        
        status = w5500_read_socket_register(i, W5500_Sn_SR(0) - W5500_SOCKET_REG_BASE(0));
        
        if (status == W5500_SOCK_ESTABLISHED) {
            // Check for received data
            if (w5500_socket_available(i) > 0) {
                uint16_t recv_size = w5500_socket_available(i);
                uint16_t read_size = (recv_size > W5500_MAX_BUFFER_SIZE) ? W5500_MAX_BUFFER_SIZE : recv_size;
                
                // Read data
                read_size = w5500_socket_recv(i, rx_buffer, read_size);
                
                if (read_size > 0) {
                    // Parse HTTP request
                    http_request_t request;
                    memset(&request, 0, sizeof(http_request_t));
                    request.socket = i;
                    
                    http_parse_request(i, rx_buffer, read_size, &request);
                    
                    // Create response structure
                    http_response_t response;
                    memset(&response, 0, sizeof(http_response_t));
                    response.status = HTTP_STATUS_OK;
                    
                    // Set default headers
                    http_set_header(&response, "Server", "W5500-RP2040/1.0");
                    http_set_header(&response, "Connection", "close");
                    
                    // Find matching handler
                    bool handler_found = false;
                    for (int j = 0; j < http_config.endpoint_count; j++) {
                        if (strcmp(http_config.endpoints[j].path, request.path) == 0) {
                            if (http_config.endpoints[j].method == request.method ||
                                http_config.endpoints[j].method == HTTP_METHOD_UNKNOWN) {
                                // Found matching handler
                                if (http_config.endpoints[j].handler) {
                                    http_config.endpoints[j].handler(&request, &response);
                                    handler_found = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    // Set 404 if no handler found
                    if (!handler_found) {
                        response.status = HTTP_STATUS_NOT_FOUND;
                        const char *not_found = "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>";
                        http_set_content_type(&response, "text/html");
                        http_set_body(&response, not_found);
                    }
                    
                    // Send response
                    http_send_response(i, &response);
                    
                    // Close connection if not keep-alive
                    if (!response.keep_alive) {
                        w5500_socket_disconnect(i);
                        w5500_socket_close(i);
                    }
                }
            }
        } else if (status == W5500_SOCK_CLOSE_WAIT) {
            // Close connection gracefully
            w5500_socket_disconnect(i);
            w5500_socket_close(i);
        } else if (status == W5500_SOCK_CLOSED) {
            // Socket already closed
            sockets_in_use[i] = 0;
        }
    }
}

/**
 * Parse an HTTP request
 */
void http_parse_request(uint8_t socket, uint8_t *data, uint16_t len, http_request_t *request) {
    char *method_str, *path_str, *protocol;
    char *header_start, *header_end;
    char *body_start;
    
    // Null-terminate the request data
    data[len] = '\0';
    
    // Get client IP and port
    uint8_t src_ip[4];
    uint16_t src_port;
    
    // The source IP and port would be obtained from W5500's socket registers
    // We'll just provide a placeholder implementation here
    w5500_read_bytes(W5500_Sn_DIPR(socket), W5500_BSB_SOCKET_REG(socket), src_ip, 4);
    src_port = w5500_read_socket_register_16(socket, W5500_Sn_DPORT(socket) - W5500_SOCKET_REG_BASE(0));
    
    memcpy(request->client_ip, src_ip, 4);
    request->client_port = src_port;
    
    // Extract first line (request line)
    char *line = strtok((char *)data, "\r\n");
    if (!line) return;
    
    // Parse request line
    method_str = strtok(line, " ");
    if (!method_str) return;
    
    path_str = strtok(NULL, " ");
    if (!path_str) return;
    
    protocol = strtok(NULL, " ");
    if (!protocol) return;
    
    // Parse method
    request->method = http_parse_method(method_str);
    
    // Parse URL
    http_parse_url(path_str, request->path, request->query_string);
    
    // Parse headers
    header_start = line + strlen(line) + 2; // Skip past the first line and CRLF
    
    // Find the end of headers (double CRLF)
    body_start = strstr(header_start, "\r\n\r\n");
    
    if (body_start) {
        header_end = body_start;
        
        // Extract body
        body_start += 4; // Skip past the double CRLF
        
        // Calculate body length
        uint16_t body_length = len - (body_start - (char *)data);
        
        if (body_length > 0) {
            // Copy body (up to max size)
            uint16_t copy_len = (body_length < HTTP_MAX_REQUEST_SIZE) ? body_length : HTTP_MAX_REQUEST_SIZE;
            memcpy(request->body, body_start, copy_len);
            request->body[copy_len] = '\0';
            request->body_length = copy_len;
        }
        
        // Null-terminate the headers section
        *header_end = '\0';
        
        // Extract headers (simplified)
        if (strlen(header_start) < sizeof(request->headers)) {
            strcpy(request->headers, header_start);
        } else {
            strncpy(request->headers, header_start, sizeof(request->headers) - 1);
            request->headers[sizeof(request->headers) - 1] = '\0';
        }
    }
}

/**
 * Send an HTTP response
 */
void http_send_response(uint8_t socket, http_response_t *response) {
    char header_buffer[512];
    uint16_t header_len;
    
    // Build response header
    snprintf(header_buffer, sizeof(header_buffer),
        "HTTP/1.1 %d %s\r\n%s\r\nContent-Length: %d\r\n\r\n",
        response->status,
        http_status_message(response->status),
        response->headers,
        response->body_length
    );
    
    header_len = strlen(header_buffer);
    
    // Send headers
    w5500_socket_send(socket, (uint8_t *)header_buffer, header_len);
    
    // Send body if present
    if (response->body_length > 0) {
        w5500_socket_send(socket, (uint8_t *)response->body, response->body_length);
    }
}

/**
 * Set HTTP response status
 */
void http_set_status(http_response_t *response, http_status_t status) {
    response->status = status;
}

/**
 * Set HTTP content type
 */
void http_set_content_type(http_response_t *response, const char *content_type) {
    http_set_header(response, "Content-Type", content_type);
}

/**
 * Set HTTP response body
 */
void http_set_body(http_response_t *response, const char *body) {
    uint16_t body_len = strlen(body);
    uint16_t copy_len = (body_len < HTTP_MAX_RESPONSE_SIZE) ? body_len : HTTP_MAX_RESPONSE_SIZE;
    
    memcpy(response->body, body, copy_len);
    response->body[copy_len] = '\0';
    response->body_length = copy_len;
}

/**
 * Set HTTP response header
 */
void http_set_header(http_response_t *response, const char *name, const char *value) {
    char header[128];
    size_t current_len = strlen(response->headers);
    
    // Build header string
    snprintf(header, sizeof(header), "%s: %s\r\n", name, value);
    
    // Check if it fits
    if (current_len + strlen(header) < sizeof(response->headers)) {
        strcat(response->headers, header);
    }
}

//=====================================================================
// Utility Functions
//=====================================================================

/**
 * Parse HTTP method string
 */
http_method_t http_parse_method(const char *method_str) {
    if (strcmp(method_str, "GET") == 0) return HTTP_METHOD_GET;
    if (strcmp(method_str, "POST") == 0) return HTTP_METHOD_POST;
    if (strcmp(method_str, "PUT") == 0) return HTTP_METHOD_PUT;
    if (strcmp(method_str, "DELETE") == 0) return HTTP_METHOD_DELETE;
    if (strcmp(method_str, "HEAD") == 0) return HTTP_METHOD_HEAD;
    if (strcmp(method_str, "OPTIONS") == 0) return HTTP_METHOD_OPTIONS;
    
    return HTTP_METHOD_UNKNOWN;
}

/**
 * Parse URL into path and query string
 */
void http_parse_url(const char *url, char *path, char *query) {
    const char *query_start = strchr(url, '?');
    
    if (query_start) {
        // Copy path part
        size_t path_len = query_start - url;
        if (path_len >= 64) path_len = 63;
        strncpy(path, url, path_len);
        path[path_len] = '\0';
        
        // Copy query part
        strcpy(query, query_start + 1);
    } else {
        // No query string
        strncpy(path, url, 63);
        path[63] = '\0';
        query[0] = '\0';
    }
}

/**
 * Get HTTP status message
 */
const char *http_status_message(http_status_t status) {
    switch (status) {
        case HTTP_STATUS_OK: return "OK";
        case HTTP_STATUS_CREATED: return "Created";
        case HTTP_STATUS_NO_CONTENT: return "No Content";
        case HTTP_STATUS_BAD_REQUEST: return "Bad Request";
        case HTTP_STATUS_UNAUTHORIZED: return "Unauthorized";
        case HTTP_STATUS_FORBIDDEN: return "Forbidden";
        case HTTP_STATUS_NOT_FOUND: return "Not Found";
        case HTTP_STATUS_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_STATUS_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_STATUS_NOT_IMPLEMENTED: return "Not Implemented";
        default: return "Unknown";
    }
}

/**
 * Get content type from file extension
 */
const char *http_content_type_from_extension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    
    if (!ext) return "application/octet-stream";
    
    // Convert to lowercase
    char ext_lower[10];
    size_t i;
    ext++; // Skip the dot
    
    for (i = 0; i < sizeof(ext_lower) - 1 && ext[i]; i++) {
        ext_lower[i] = tolower(ext[i]);
    }
    ext_lower[i] = '\0';
    
    // Compare with known extensions
    if (strcmp(ext_lower, "html") == 0 || strcmp(ext_lower, "htm") == 0) {
        return "text/html";
    } else if (strcmp(ext_lower, "css") == 0) {
        return "text/css";
    } else if (strcmp(ext_lower, "js") == 0) {
        return "application/javascript";
    } else if (strcmp(ext_lower, "json") == 0) {
        return "application/json";
    } else if (strcmp(ext_lower, "txt") == 0) {
        return "text/plain";
    } else if (strcmp(ext_lower, "jpg") == 0 || strcmp(ext_lower, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(ext_lower, "png") == 0) {
        return "image/png";
    } else if (strcmp(ext_lower, "gif") == 0) {
        return "image/gif";
    } else if (strcmp(ext_lower, "ico") == 0) {
        return "image/x-icon";
    } else if (strcmp(ext_lower, "xml") == 0) {
        return "application/xml";
    } else if (strcmp(ext_lower, "pdf") == 0) {
        return "application/pdf";
    }
    
    return "application/octet-stream";
}