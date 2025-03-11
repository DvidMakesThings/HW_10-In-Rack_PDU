#ifndef W5500_RP2040_HTTP_H
#define W5500_RP2040_HTTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Network Configuration
#define SERVER_PORT 8081
#define IP_ADDR {192, 168, 0, 21}
#define SUBNET_MASK {255, 255, 255, 0}
#define GATEWAY {192, 168, 0, 1}
#define MAC_ADDR {0x00, 0x08, 0xDC, 0xBE, 0xEF, 0x91}

// W5500 Register Addresses
#define W5500_MODE         0x0000   // Mode Register
#define W5500_PHYCFGR      0x002E   // PHY Configuration Register
#define W5500_VERSIONR     0x0039   // Version Register
#define W5500_SHAR         0x0009   // Source Hardware Address Register (MAC)
#define W5500_SIPR         0x000F   // Source IP Address Register
#define W5500_GAR          0x0001   // Gateway Address Register
#define W5500_SUBR         0x0005   // Subnet Mask Register

// W5500 Socket Registers (relative to socket register base)
#define W5500_SOCKET_REG_BASE(socket) (0x0000 + (socket) * 0x0100)
#define W5500_Sn_MR(socket)           (W5500_SOCKET_REG_BASE(socket) + 0x0000) // Mode Register
#define W5500_Sn_CR(socket)           (W5500_SOCKET_REG_BASE(socket) + 0x0001) // Command Register
#define W5500_Sn_IR(socket)           (W5500_SOCKET_REG_BASE(socket) + 0x0002) // Interrupt Register
#define W5500_Sn_SR(socket)           (W5500_SOCKET_REG_BASE(socket) + 0x0003) // Status Register
#define W5500_Sn_PORT(socket)         (W5500_SOCKET_REG_BASE(socket) + 0x0004) // Source Port
#define W5500_Sn_DIPR(socket)         (W5500_SOCKET_REG_BASE(socket) + 0x000C) // Destination IP Address
#define W5500_Sn_DPORT(socket)        (W5500_SOCKET_REG_BASE(socket) + 0x0010) // Destination Port
#define W5500_Sn_TX_FSR(socket)       (W5500_SOCKET_REG_BASE(socket) + 0x0020) // TX Free Size
#define W5500_Sn_TX_RD(socket)        (W5500_SOCKET_REG_BASE(socket) + 0x0022) // TX Read Pointer
#define W5500_Sn_TX_WR(socket)        (W5500_SOCKET_REG_BASE(socket) + 0x0024) // TX Write Pointer
#define W5500_Sn_RX_RSR(socket)       (W5500_SOCKET_REG_BASE(socket) + 0x0026) // RX Received Size
#define W5500_Sn_RX_RD(socket)        (W5500_SOCKET_REG_BASE(socket) + 0x0028) // RX Read Pointer
#define W5500_Sn_RX_WR(socket)        (W5500_SOCKET_REG_BASE(socket) + 0x002A) // RX Write Pointer
#define W5500_Sn_TX_BASE(socket)      (W5500_SOCKET_REG_BASE(socket) + 0x0018) // TX base address
#define W5500_Sn_RX_BASE(socket)      (W5500_SOCKET_REG_BASE(socket) + 0x001C) // RX base address

// W5500 Socket Commands
#define W5500_CMD_CLEAR     0x00    // Clear command 
#define W5500_CMD_OPEN      0x01    // Initialize socket
#define W5500_CMD_LISTEN    0x02    // Wait for TCP connection request in TCP mode (Server mode)
#define W5500_CMD_CONNECT   0x04    // Send connection request in TCP mode (Client mode)
#define W5500_CMD_DISCON    0x08    // Send disconnect request
#define W5500_CMD_CLOSE     0x10    // Close socket
#define W5500_CMD_SEND      0x20    // Update TX buffer pointer and send data
#define W5500_CMD_SEND_MAC  0x21    // Send data with MAC address
#define W5500_CMD_SEND_KEEP 0x22    // Send keep alive message
#define W5500_CMD_RECV      0x40    // Update RX buffer pointer and receive data
#define W5500_CMD_RESET     0x80    // Reset socket

// W5500 Socket Status
#define W5500_SOCK_CLOSED      0x00  // Closed
#define W5500_SOCK_INIT        0x13  // Init state
#define W5500_SOCK_LISTEN      0x14  // Listen state
#define W5500_SOCK_ESTABLISHED 0x17  // Connection established
#define W5500_SOCK_CLOSE_WAIT  0x1C  // CLOSE_WAIT state
#define W5500_SOCK_UDP         0x22  // UDP socket
#define W5500_SOCK_MACRAW      0x42  // MACRAW socket

// W5500 Socket Mode
#define W5500_SOCK_MR_TCP      0x01  // TCP
#define W5500_SOCK_MR_UDP      0x02  // UDP
#define W5500_SOCK_MR_MACRAW   0x04  // MAC RAW
#define W5500_SOCK_MR_IPRAW    0x03  // IP RAW

// W5500 Control Phase Bits
#define W5500_CTRL_DATA        0x00
#define W5500_CTRL_CONFIG      0x01
#define W5500_CTRL_SOCKET_REG  0x02
#define W5500_CTRL_SOCKET_TX   0x04
#define W5500_CTRL_SOCKET_RX   0x06

// W5500 Misc
#define W5500_MAX_SOCKETS      8
#define W5500_BUF_SIZE         2048
#define W5500_MAX_PACKET_SIZE  1460

// HTTP Protocol Definitions
#define HTTP_METHOD_GET       0x01
#define HTTP_METHOD_POST      0x02
#define HTTP_METHOD_PUT       0x04
#define HTTP_METHOD_DELETE    0x08
#define HTTP_METHOD_OPTIONS   0x10
#define HTTP_METHOD_HEAD      0x20

#define HTTP_MAX_ROUTES       10
#define HTTP_MAX_HEADER_SIZE  1024
#define HTTP_MAX_BODY_SIZE    1024
#define HTTP_MAX_URL_SIZE     128
#define HTTP_MAX_METHOD_SIZE  8
#define HTTP_MAX_PARAMS       10
#define HTTP_MAX_PARAM_SIZE   64
#define HTTP_MAX_HEADERS      10
#define HTTP_MAX_HEADER_NAME  32
#define HTTP_MAX_HEADER_VALUE 128

#define HTTP_METHOD_GET  0x01
#define HTTP_METHOD_POST 0x02

// HTTP Request and Response Structures
typedef struct {
    int method;         // HTTP method (GET, POST, etc.)
    char url[128];      // Requested URL
    uint8_t* body;      // Request body (if any)
    uint16_t body_length;
    uint8_t socket;     // The socket from which this request came
} http_request_t;

typedef struct {
    int status;                             // HTTP response status
    char body[1024];                        // Response body
    int body_length;                        // Length of the body
    char headers[10][2][128];               // Headers
    int header_count;                       // Number of headers
} http_response_t;

// HTTP Route Structure
typedef struct {
    char path[128];                        // Path for the route
    int methods;                           // HTTP methods for this route
    void (*handler)(http_request_t* request, http_response_t* response);  // Handler function for the route
} http_route_t;

// HTTP Server Structure
typedef struct {
    uint8_t socket;                        // Socket for the server
    uint16_t port;                         // Port number
    http_route_t routes[10];               // Registered routes
    int route_count;                       // Count of registered routes
} http_server_t;

// SPI Communication
void w5500_select();
void w5500_deselect();

// W5500 Initialization and Configuration Functions
bool w5500_init();
void w5500_set_mac(const uint8_t* mac);
void w5500_set_ip(const uint8_t* ip);
void w5500_set_subnet(const uint8_t* subnet);
void w5500_set_gateway(const uint8_t* gateway);

// Socket Handling Functions
int w5500_socket_open(uint8_t protocol);
void w5500_socket_close(uint8_t socket);
void w5500_socket_listen(uint8_t socket);
void w5500_socket_send(uint8_t socket, const uint8_t* data, uint16_t len);
uint16_t w5500_socket_recv(uint8_t socket, uint8_t* buffer, uint16_t len);

// Helper Functions
void w5500_write_reg(uint16_t addr, uint8_t value);
uint8_t w5500_read_reg(uint16_t addr);
void w5500_write_reg_16(uint16_t addr, uint16_t value);
uint16_t w5500_read_reg_16(uint16_t addr);
void w5500_send_socket_command(uint8_t socket, uint8_t command);
bool w5500_check_phy();

// HTTP Server Functions
void http_server_init(http_server_t* server, uint16_t port);
void http_server_register_route(http_server_t* server, const char* path, int methods, void (*handler)(http_request_t* request, http_response_t* response));
bool http_server_start(http_server_t* server);
uint8_t w5500_socket_state(uint8_t socket);
void http_server_process(http_server_t* server);
void http_request_parse(http_request_t* req, const uint8_t* data, size_t len);
int http_method_from_string(const char* method_str);
void http_response_set_status(http_response_t* res, int status);
void http_response_set_body(http_response_t* res, const char* body, size_t len);
void http_response_send(http_response_t* res, uint8_t socket);
uint16_t w5500_socket_available(uint8_t socket);
void http_server_init(http_server_t* server, uint16_t port);
void http_server_register_route(http_server_t* server, const char* path, int methods, void (*handler)(http_request_t* request, http_response_t* response));
bool http_server_start(http_server_t* server);
void http_server_process(http_server_t* server);
bool path_matches(const char* route, const char* path);
void w5500_socket_memory_setup(uint8_t socket);
void w5500_socket_process(uint8_t socket);
void SPI_CRITICAL_ENTER(void);
void SPI_CRITICAL_EXIT(void);
#endif // W5500_RP2040_HTTP_H