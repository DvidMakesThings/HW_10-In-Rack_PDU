/**
 * W5500 Ethernet HTTP Server Library for RP2040
 * 
 * This library provides a complete interface for the W5500 Ethernet controller
 * when connected to an RP2040 microcontroller, with focus on HTTP server functionality.
 *
 * Copyright (c) 2025
 */

#ifndef W5500_RP2040_HTTP_H
#define W5500_RP2040_HTTP_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

// Configuration Constants
#define W5500_MAX_SOCKETS           8
#define W5500_MAX_BUFFER_SIZE       2048
#define W5500_DEFAULT_MAC           {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}
#define HTTP_SERVER_PORT            80
#define HTTP_MAX_CLIENTS            4
#define HTTP_MAX_REQUEST_SIZE       1024
#define HTTP_MAX_ENDPOINTS          10
#define HTTP_MAX_RESPONSE_SIZE      2048

// W5500 Register Addresses
#define W5500_COMMON_REG_BASE       0x0000
#define W5500_MR                    0x0000  // Mode Register
#define W5500_GAR                   0x0001  // Gateway Address Register
#define W5500_SUBR                  0x0005  // Subnet Mask Register
#define W5500_SHAR                  0x0009  // Source Hardware Address Register (MAC)
#define W5500_SIPR                  0x000F  // Source IP Address Register
#define W5500_INTLEVEL              0x0013  // Interrupt Low Level Timer Register
#define W5500_IR                    0x0015  // Interrupt Register
#define W5500_IMR                   0x0016  // Interrupt Mask Register
#define W5500_SIR                   0x0017  // Socket Interrupt Register
#define W5500_SIMR                  0x0018  // Socket Interrupt Mask Register
#define W5500_RTR                   0x0019  // Retry Time Register
#define W5500_RCR                   0x001B  // Retry Count Register
#define W5500_PHYCFGR               0x002E  // PHY Configuration Register

// Socket Register Base Addresses
#define W5500_SOCKET_REG_BASE(n)    (0x0400 + (n * 0x0100))
#define W5500_Sn_MR(n)              (W5500_SOCKET_REG_BASE(n) + 0x0000)  // Socket n Mode Register
#define W5500_Sn_CR(n)              (W5500_SOCKET_REG_BASE(n) + 0x0001)  // Socket n Command Register
#define W5500_Sn_IR(n)              (W5500_SOCKET_REG_BASE(n) + 0x0002)  // Socket n Interrupt Register
#define W5500_Sn_SR(n)              (W5500_SOCKET_REG_BASE(n) + 0x0003)  // Socket n Status Register
#define W5500_Sn_PORT(n)            (W5500_SOCKET_REG_BASE(n) + 0x0004)  // Socket n Source Port Register
#define W5500_Sn_DHAR(n)            (W5500_SOCKET_REG_BASE(n) + 0x0006)  // Socket n Destination Hardware Address Register
#define W5500_Sn_DIPR(n)            (W5500_SOCKET_REG_BASE(n) + 0x000C)  // Socket n Destination IP Address Register
#define W5500_Sn_DPORT(n)           (W5500_SOCKET_REG_BASE(n) + 0x0010)  // Socket n Destination Port Register
#define W5500_Sn_MSSR(n)            (W5500_SOCKET_REG_BASE(n) + 0x0012)  // Socket n Maximum Segment Size Register
#define W5500_Sn_PROTO(n)           (W5500_SOCKET_REG_BASE(n) + 0x0014)  // Socket n IP Protocol Register
#define W5500_Sn_TOS(n)             (W5500_SOCKET_REG_BASE(n) + 0x0015)  // Socket n IP TOS Register
#define W5500_Sn_TTL(n)             (W5500_SOCKET_REG_BASE(n) + 0x0016)  // Socket n IP TTL Register
#define W5500_Sn_RXBUF_SIZE(n)      (W5500_SOCKET_REG_BASE(n) + 0x001E)  // Socket n RX Buffer Size Register
#define W5500_Sn_TXBUF_SIZE(n)      (W5500_SOCKET_REG_BASE(n) + 0x001F)  // Socket n TX Buffer Size Register
#define W5500_Sn_TX_FSR(n)          (W5500_SOCKET_REG_BASE(n) + 0x0020)  // Socket n TX Free Size Register
#define W5500_Sn_TX_RD(n)           (W5500_SOCKET_REG_BASE(n) + 0x0022)  // Socket n TX Read Pointer Register
#define W5500_Sn_TX_WR(n)           (W5500_SOCKET_REG_BASE(n) + 0x0024)  // Socket n TX Write Pointer Register
#define W5500_Sn_RX_RSR(n)          (W5500_SOCKET_REG_BASE(n) + 0x0026)  // Socket n RX Received Size Register
#define W5500_Sn_RX_RD(n)           (W5500_SOCKET_REG_BASE(n) + 0x0028)  // Socket n RX Read Pointer Register
#define W5500_Sn_RX_WR(n)           (W5500_SOCKET_REG_BASE(n) + 0x002A)  // Socket n RX Write Pointer Register
#define W5500_Sn_IMR(n)             (W5500_SOCKET_REG_BASE(n) + 0x002C)  // Socket n Interrupt Mask Register

// Socket n TX/RX Memory Addresses
#define W5500_TX_BASE(n)            (0x0800 + (n * 0x0800))
#define W5500_RX_BASE(n)            (0x1000 + (n * 0x0800))

// W5500 SPI Control Byte Definitions
#define W5500_CTRL_READ             0x00
#define W5500_CTRL_WRITE            0x04
#define W5500_CTRL_VDM              0x00    // Variable Data Length Mode
#define W5500_CTRL_FDM_1B           0x01    // Fixed Data Length Mode, 1 Byte
#define W5500_CTRL_FDM_2B           0x02    // Fixed Data Length Mode, 2 Bytes
#define W5500_CTRL_FDM_4B           0x03    // Fixed Data Length Mode, 4 Bytes

// Control Phase Block Selection Bits (BSB)
#define W5500_BSB_COMMON_REG        0x00
#define W5500_BSB_SOCKET_REG(n)     (0x08 + (n << 5))
#define W5500_BSB_SOCKET_TX_BUF(n)  (0x10 + (n << 5))
#define W5500_BSB_SOCKET_RX_BUF(n)  (0x18 + (n << 5))

// Socket Commands (CR)
#define W5500_CMD_OPEN              0x01
#define W5500_CMD_LISTEN            0x02
#define W5500_CMD_CONNECT           0x04
#define W5500_CMD_DISCON            0x08
#define W5500_CMD_CLOSE             0x10
#define W5500_CMD_SEND              0x20
#define W5500_CMD_SEND_MAC          0x21
#define W5500_CMD_SEND_KEEP         0x22
#define W5500_CMD_RECV              0x40

// Socket Status (SR)
#define W5500_SOCK_CLOSED           0x00
#define W5500_SOCK_INIT             0x13
#define W5500_SOCK_LISTEN           0x14
#define W5500_SOCK_ESTABLISHED      0x17
#define W5500_SOCK_CLOSE_WAIT       0x1C
#define W5500_SOCK_UDP              0x22
#define W5500_SOCK_MACRAW           0x42

// Socket Mode Register Values (MR)
#define W5500_MR_CLOSED             0x00
#define W5500_MR_TCP                0x01
#define W5500_MR_UDP                0x02
#define W5500_MR_MACRAW             0x04
#define W5500_MR_MULTI              0x80    // Multicasting

// Socket Interrupt Register Values (IR)
#define W5500_IR_CON                0x01
#define W5500_IR_DISCON             0x02
#define W5500_IR_RECV               0x04
#define W5500_IR_TIMEOUT            0x08
#define W5500_IR_SEND_OK            0x10

// HTTP Method Types
typedef enum {
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_UNKNOWN
} http_method_t;

// HTTP Response Codes
typedef enum {
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_NO_CONTENT = 204,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_UNAUTHORIZED = 401,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
    HTTP_STATUS_METHOD_NOT_ALLOWED = 405,
    HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
    HTTP_STATUS_NOT_IMPLEMENTED = 501
} http_status_t;

// HTTP Request Structure
typedef struct {
    http_method_t method;
    char path[64];
    char query_string[128];
    char headers[512];
    char body[HTTP_MAX_REQUEST_SIZE];
    uint16_t body_length;
    uint8_t socket;
    uint8_t client_ip[4];
    uint16_t client_port;
} http_request_t;

// HTTP Response Structure
typedef struct {
    http_status_t status;
    char headers[512];
    char body[HTTP_MAX_RESPONSE_SIZE];
    uint16_t body_length;
    bool keep_alive;
} http_response_t;

// HTTP Route Handler Function Type
typedef void (*http_handler_t)(http_request_t *req, http_response_t *res);

// HTTP Endpoint Definition
typedef struct {
    char path[64];
    http_method_t method;
    http_handler_t handler;
} http_endpoint_t;

// HTTP Server Configuration
typedef struct {
    uint16_t port;
    uint8_t max_connections;
    http_endpoint_t endpoints[HTTP_MAX_ENDPOINTS];
    uint8_t endpoint_count;
} http_server_config_t;

// W5500 Configuration
typedef struct {
    spi_inst_t *spi;
    uint8_t cs_pin;
    uint8_t rst_pin;
    uint8_t int_pin;
    uint32_t spi_baudrate;
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t subnet[4];
    uint8_t gateway[4];
    bool dhcp_enabled;
} w5500_config_t;

// W5500 Hardware Interface Functions
void w5500_init(w5500_config_t *config);
void w5500_reset(void);
void w5500_select(void);
void w5500_deselect(void);
void w5500_write_byte(uint16_t address, uint8_t block, uint8_t data);
uint8_t w5500_read_byte(uint16_t address, uint8_t block);
void w5500_write_bytes(uint16_t address, uint8_t block, uint8_t *data, uint16_t len);
void w5500_read_bytes(uint16_t address, uint8_t block, uint8_t *data, uint16_t len);
void w5500_write_socket_register(uint8_t socket, uint16_t address, uint8_t data);
uint8_t w5500_read_socket_register(uint8_t socket, uint16_t address);
uint16_t w5500_read_socket_register_16(uint8_t socket, uint16_t address);
void w5500_write_socket_register_16(uint8_t socket, uint16_t address, uint16_t data);
void w5500_write_tx_buffer(uint8_t socket, uint8_t *data, uint16_t len);
void w5500_read_rx_buffer(uint8_t socket, uint8_t *data, uint16_t len);

// W5500 Socket Management Functions
bool w5500_socket_init(uint8_t socket, uint8_t protocol, uint16_t port);
bool w5500_socket_listen(uint8_t socket);
bool w5500_socket_connect(uint8_t socket, uint8_t *ip, uint16_t port);
bool w5500_socket_disconnect(uint8_t socket);
bool w5500_socket_close(uint8_t socket);
uint16_t w5500_socket_send(uint8_t socket, uint8_t *data, uint16_t len);
uint16_t w5500_socket_recv(uint8_t socket, uint8_t *data, uint16_t len);
bool w5500_socket_is_established(uint8_t socket);
bool w5500_socket_is_closed(uint8_t socket);
uint16_t w5500_socket_available(uint8_t socket);

// HTTP Server Functions
void http_server_init(http_server_config_t *config);
void http_server_add_endpoint(const char *path, http_method_t method, http_handler_t handler);
void http_server_start(void);
void http_server_stop(void);
void http_server_process(void);
void http_send_response(uint8_t socket, http_response_t *response);
void http_parse_request(uint8_t socket, uint8_t *data, uint16_t len, http_request_t *request);
void http_set_status(http_response_t *response, http_status_t status);
void http_set_content_type(http_response_t *response, const char *content_type);
void http_set_body(http_response_t *response, const char *body);
void http_set_header(http_response_t *response, const char *name, const char *value);

// Utility Functions
http_method_t http_parse_method(const char *method_str);
void http_parse_url(const char *url, char *path, char *query);
const char *http_status_message(http_status_t status);
const char *http_content_type_from_extension(const char *filename);

#endif // W5500_RP2040_HTTP_H