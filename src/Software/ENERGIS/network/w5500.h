#ifndef W5500_RP2040_HTTP_H
#define W5500_RP2040_HTTP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "../CONFIG.h"

/*-------------------------------------------------------------------------*/
/* Network Configuration (set these in your application as needed)         */
#define SERVER_PORT 80

/*-------------------------------------------------------------------------*/
/* W5500 Register Map */
#define W5500_REG_MODE         (0x0000)
#define W5500_REG_GAR          (0x0001)
#define W5500_REG_SUBR         (0x0005)
#define W5500_REG_SHAR         (0x0009)
#define W5500_REG_SIPR         (0x000F)
#define W5500_REG_INTLEVEL     (0x0013)
#define W5500_REG_IR           (0x0015)
#define W5500_REG_IMR          (0x0016)
#define W5500_REG_SIR          (0x0017)
#define W5500_REG_SIMR         (0x0018)
#define W5500_REG_RTR          (0x0019)
#define W5500_REG_RCR          (0x001B)
#define W5500_REG_PTIMER       (0x001C)
#define W5500_REG_PMAGIC       (0x001D)
#define W5500_REG_PHAR         (0x001E)
#define W5500_REG_PSID         (0x0024)
#define W5500_REG_PMRU         (0x0026)
#define W5500_REG_UIPR         (0x0028)
#define W5500_REG_UPORTR       (0x002C)
#define W5500_REG_PHYCFGR      (0x002E)
#define W5500_REG_VERSIONR     (0x0039)

// Define socket register offsets:
#define W5500_SREG_MR_OFFSET       (0x0000)
#define W5500_SREG_CR_OFFSET       (0x0001)
#define W5500_SREG_IR_OFFSET       (0x0002)
#define W5500_SREG_SR_OFFSET       (0x0003)
#define W5500_SREG_PORT_OFFSET     (0x0004)

/*-------------------------------------------------------------------------*/
/* Socket Registers (each socket occupies 0x0100 bytes) */
#define W5500_SOCKET_REG_BASE(s)   (0x0000 + ((s) * 0x0100))
#define W5500_BLOCK_S_REG(s)       (0x08 + ((s) * 0x20))
#define W5500_Sn_MR(s)             (W5500_SOCKET_REG_BASE(s) + W5500_SREG_MR_OFFSET)
#define W5500_Sn_CR(s)             (W5500_SOCKET_REG_BASE(s) + W5500_SREG_CR_OFFSET)
#define W5500_Sn_IR(s)             (W5500_SOCKET_REG_BASE(s) + W5500_SREG_IR_OFFSET)
#define W5500_Sn_SR(s)             (W5500_SOCKET_REG_BASE(s) + W5500_SREG_SR_OFFSET)
#define W5500_Sn_PORT(s)           (W5500_SOCKET_REG_BASE(s) + W5500_SREG_PORT_OFFSET)
#define W5500_Sn_DIPR(s)           (W5500_SOCKET_REG_BASE(s) + 0x000C)
#define W5500_Sn_DPORT(s)          (W5500_SOCKET_REG_BASE(s) + 0x0010)
#define W5500_Sn_TX_FSR(s)         (W5500_SOCKET_REG_BASE(s) + 0x0020)
#define W5500_Sn_TX_RD(s)          (W5500_SOCKET_REG_BASE(s) + 0x0022)
#define W5500_Sn_TX_WR(s)          (W5500_SOCKET_REG_BASE(s) + 0x0024)
#define W5500_Sn_RX_RSR(s)         (W5500_SOCKET_REG_BASE(s) + 0x0026)
#define W5500_Sn_RX_RD(s)          (W5500_SOCKET_REG_BASE(s) + 0x0028)
#define W5500_Sn_RX_WR(s)          (W5500_SOCKET_REG_BASE(s) + 0x002A)
#define W5500_Sn_TX_BASE(s)        (W5500_SOCKET_REG_BASE(s) + 0x0018)
#define W5500_Sn_RX_BASE(s)        (W5500_SOCKET_REG_BASE(s) + 0x001C)


/* For setting the memory configuration block we use the start of the RX buffer
   configuration block (14 bytes starting at offset 0x001E) */
#define W5500_SREG_RXBUF_SIZE     (0x001E)
#define W5500_SREG_TXBUF_SIZE     (0x001F)

/*-------------------------------------------------------------------------*/
/* Socket Commands */
#define W5500_CR_OPEN            (0x01)
#define W5500_CR_LISTEN          (0x02)
#define W5500_CR_CONNECT         (0x04)
#define W5500_CR_DISCON          (0x08)
#define W5500_CR_CLOSE           (0x10)
#define W5500_CR_SEND            (0x20)
#define W5500_CR_SEND_MAC        (0x21)
#define W5500_CR_SEND_KEEP       (0x22)
#define W5500_CR_RECV            (0x40)

/*-------------------------------------------------------------------------*/
/* W5500 Interrupt Flags */
#define W5500_IR_CON              (0x01)
#define W5500_IR_DISCON           (0x02)
#define W5500_IR_RECV             (0x04)
#define W5500_IR_TIMEOUT          (0x08)
#define W5500_IR_SEND_OK          (0x10)

/*-------------------------------------------------------------------------*/
/* Socket Status */
#define W5500_SR_CLOSED         (0x00)
#define W5500_SR_INIT           (0x13)
#define W5500_SR_LISTEN         (0x14)
#define W5500_SR_ESTABLISHED    (0x17)
#define W5500_SR_CLOSE_WAIT     (0x1C)
#define W5500_SR_UDP            (0x22)
#define W5500_SR_MACRAW         (0x42)

/*-------------------------------------------------------------------------*/
/* Socket Mode values (to be written into Sn_MR) */
#define W5500_SOCK_MR_TCP      (0x01)
#define W5500_SOCK_MR_UDP      (0x02)
#define W5500_SOCK_MR_MACRAW   (0x04)
#define W5500_SOCK_MR_IPRAW    (0x03)

/*-------------------------------------------------------------------------*/
/* Control Phase Bits */
#define W5500_CTRL_DATA        (0x00)
#define W5500_CTRL_CONFIG      (0x01)
#define W5500_CTRL_SOCKET_REG  (0x02)
#define W5500_CTRL_SOCKET_TX   (0x04)
#define W5500_CTRL_SOCKET_RX   (0x06)

/*-------------------------------------------------------------------------*/
/* Miscellaneous configuration macros */
#define W5500_SOCKET_MEM         (2)   // 2 KB per socket
#define W5500_CHIP_MEM           (16)  // 16 KB total for socket data
#define W5500_CHIP_SOCKETS       (8)
#define W5500_MAX_SOCKETS        (W5500_CHIP_MEM / W5500_SOCKET_MEM)
#define W5500_SOCKET_BAD(x)      (((x) >= W5500_MAX_SOCKETS) || (W5500_socketStatus[x] == W5500_SOCKET_AVAILALE))
#define W5500_SOCKET_OPEN        (1)
#define W5500_SOCKET_AVAILALE    (0)

/*-------------------------------------------------------------------------*/
/* HTTP Protocol Definitions */
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

/*-------------------------------------------------------------------------*/
/* HTTP Structures */
typedef struct {
    int method;
    char url[HTTP_MAX_URL_SIZE];
    uint8_t* body;
    uint16_t body_length;
    uint8_t socket;
} http_request_t;

typedef struct {
    int status;
    char body[HTTP_MAX_BODY_SIZE];
    int body_length;
    char headers[HTTP_MAX_HEADERS][2][HTTP_MAX_HEADER_VALUE];
    int header_count;
} http_response_t;

typedef struct {
    char path[HTTP_MAX_URL_SIZE];
    int methods;
    void (*handler)(http_request_t* request, http_response_t* response);
} http_route_t;

typedef struct {
    uint8_t socket;
    uint16_t port;
    http_route_t routes[HTTP_MAX_ROUTES];
    int route_count;
} http_server_t;

/*-------------------------------------------------------------------------*/
/* Low-level SPI functions (to be implemented by your application) */
void w5500_select(void);
void w5500_deselect(void);

/*-------------------------------------------------------------------------*/
/* Low-level W5500 register access functions */
void w5500_write_reg(uint16_t addr, uint8_t value);
uint8_t w5500_read_reg(uint16_t addr);
void w5500_write_16(uint16_t addr, uint16_t value);
uint16_t w5500_read_16(uint16_t addr);
void w5500_Send(uint16_t offset, uint8_t block_select, uint8_t write, uint8_t *buffer, uint16_t len);
void w5500_send_socket_command(uint8_t socket, uint8_t command);


/*-------------------------------------------------------------------------*/
/* Network initialization and configuration */
bool w5500_check_phy(void);
bool w5500_basic_register_setup(void);
int w5500_init(uint8_t *gateway, uint8_t *subnet, uint8_t *mac, uint8_t *ip);
void w5500_set_mac(const uint8_t* mac);
void w5500_set_ip(const uint8_t* ip);
void w5500_set_subnet(const uint8_t* subnet);
void w5500_set_gateway(const uint8_t* gateway);

/*-------------------------------------------------------------------------*/
/* Socket memory configuration (if needed) */
bool w5500_socket_memory_setup(uint8_t socket);

#endif // W5500_RP2040_HTTP_H
