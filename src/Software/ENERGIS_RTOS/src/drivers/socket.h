/**
 * @file socket.h
 * @author DvidMakesThings - David Sipos
 * @brief W5500 Socket API (BSD-like interface) - Complete Header
 *
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * Complete BSD socket-like API for W5500 Ethernet controller.
 * Supports TCP, UDP, and RAW sockets with thread-safe operation.
 *
 * Key Differences from BSD Sockets:
 * - No bind() - socket() assigns port automatically
 * - No accept() - listen() handles connections automatically
 * - closesocket() closes and cleans up socket state
 * - Socket numbers are 0-7 (hardware limited)
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ETH_SOCKET_H
#define ETH_SOCKET_H

#include "../CONFIG.h"

/******************************************************************************
 *                          TYPE DEFINITIONS                                  *
 ******************************************************************************/

/** Socket type definition */
typedef uint8_t SOCKET;

/******************************************************************************
 *                          SOCKET OPERATION RESULT CODES                     *
 ******************************************************************************/

/** Socket operation result codes */
#define SOCK_OK 1        /**< Operation successful */
#define SOCK_BUSY 0      /**< Socket busy (non-blocking mode) */
#define SOCK_FATAL -1000 /**< Fatal error */

/** Socket error codes */
#define SOCK_ERROR 0
#define SOCKERR_SOCKNUM (SOCK_ERROR - 1)    /**< Invalid socket number */
#define SOCKERR_SOCKOPT (SOCK_ERROR - 2)    /**< Invalid socket option */
#define SOCKERR_SOCKINIT (SOCK_ERROR - 3)   /**< Socket not initialized */
#define SOCKERR_SOCKCLOSED (SOCK_ERROR - 4) /**< Socket closed unexpectedly */
#define SOCKERR_SOCKMODE (SOCK_ERROR - 5)   /**< Invalid socket mode */
#define SOCKERR_SOCKFLAG (SOCK_ERROR - 6)   /**< Invalid socket flag */
#define SOCKERR_SOCKSTATUS (SOCK_ERROR - 7) /**< Invalid socket status */
#define SOCKERR_ARG (SOCK_ERROR - 10)       /**< Invalid argument */
#define SOCKERR_PORTZERO (SOCK_ERROR - 11)  /**< Port number is zero */
#define SOCKERR_IPINVALID (SOCK_ERROR - 12) /**< Invalid IP address */
#define SOCKERR_TIMEOUT (SOCK_ERROR - 13)   /**< Timeout occurred */
#define SOCKERR_DATALEN (SOCK_ERROR - 14)   /**< Invalid data length */
#define SOCKERR_BUFFER (SOCK_ERROR - 15)    /**< Buffer full */
#define SOCKFATAL_PACKLEN (SOCK_FATAL - 1)  /**< Invalid packet length (fatal) */

/******************************************************************************
 *                          SOCKET FLAGS                                      *
 ******************************************************************************/

/** Socket flags (for socket() call) */
#define SF_ETHER_OWN (Sn_MR_MFEN)     /**< MACRAW: receive own packets */
#define SF_IGMP_VER2 (Sn_MR_MC)       /**< UDP: IGMP version 2 */
#define SF_TCP_NODELAY (Sn_MR_ND)     /**< TCP: disable Nagle's algorithm */
#define SF_MULTI_ENABLE (Sn_MR_MULTI) /**< UDP: enable multicast */
#define SF_IO_NONBLOCK 0x01           /**< Non-blocking I/O mode */
#define SF_IO_BLOCK 0x00              /**< Blocking I/O mode */
#define SF_UNI_BLOCK (Sn_MR_UCASTB)   /**< UDP: unicast blocking */

/******************************************************************************
 *                          UDP & MACRAW PACKET INFO                          *
 ******************************************************************************/

/** UDP & MACRAW Packet Information Flags */
#define PACK_FIRST 0x80     /**< Start receiving a packet */
#define PACK_REMAINED 0x01  /**< Packet remains to be received */
#define PACK_COMPLETED 0x00 /**< Packet receive completed */

/** Packet info storage (used by recvfrom) */
extern uint8_t sock_pack_info[W5500_SOCKNUM];

/******************************************************************************
 *                          SOCKET INTERRUPT KINDS                            *
 ******************************************************************************/

/** Socket interrupt kinds */
typedef enum {
    SIK_CONNECTED = (1 << 0),    /**< TCP connection established */
    SIK_DISCONNECTED = (1 << 1), /**< TCP connection closed */
    SIK_RECEIVED = (1 << 2),     /**< Data received */
    SIK_TIMEOUT = (1 << 3),      /**< Timeout occurred */
    SIK_SENT = (1 << 4),         /**< Data sent completely */
    SIK_ALL = 0x1F               /**< All interrupts */
} sockint_kind;

/******************************************************************************
 *                          SOCKET CONTROL TYPES                              *
 ******************************************************************************/

/** Socket control types (for ctlsocket) */
typedef enum {
    CS_SET_IOMODE,    /**< Set I/O mode (blocking/non-blocking) */
    CS_GET_IOMODE,    /**< Get I/O mode */
    CS_GET_MAXTXBUF,  /**< Get TX buffer size */
    CS_GET_MAXRXBUF,  /**< Get RX buffer size */
    CS_CLR_INTERRUPT, /**< Clear interrupt flags */
    CS_GET_INTERRUPT, /**< Get interrupt flags */
    CS_SET_INTMASK,   /**< Set interrupt mask */
    CS_GET_INTMASK    /**< Get interrupt mask */
} ctlsock_type;

/******************************************************************************
 *                          SOCKET OPTION TYPES                               *
 ******************************************************************************/

/** Socket option types (for setsockopt/getsockopt) */
typedef enum {
    SO_FLAG,          /**< Get socket flags (read-only) */
    SO_TTL,           /**< Set/Get Time-To-Live */
    SO_TOS,           /**< Set/Get Type-Of-Service */
    SO_MSS,           /**< Set/Get Maximum Segment Size */
    SO_DESTIP,        /**< Set/Get destination IP */
    SO_DESTPORT,      /**< Set/Get destination port */
    SO_KEEPALIVESEND, /**< Send keep-alive packet (TCP) */
    SO_KEEPALIVEAUTO, /**< Set keep-alive timer (TCP) */
    SO_SENDBUF,       /**< Get free TX buffer size (read-only) */
    SO_RECVBUF,       /**< Get received data size (read-only) */
    SO_STATUS,        /**< Get socket status (read-only) */
    SO_REMAINSIZE,    /**< Get remaining packet size (UDP) */
    SO_PACKINFO       /**< Get packet info (UDP) */
} sockopt_type;

/******************************************************************************
 *                          SOCKET MANAGEMENT FUNCTIONS                       *
 ******************************************************************************/

/**
 * @brief Create and initialize a socket
 *
 * Creates a new socket with the specified protocol and port.
 * For TCP, the socket is put into SOCK_INIT state.
 * For UDP, the socket is put into SOCK_UDP state.
 *
 * @param sn Socket number (0-7)
 * @param protocol Protocol type:
 *                 - Sn_MR_TCP: TCP socket
 *                 - Sn_MR_UDP: UDP socket
 *                 - Sn_MR_IPRAW: IP raw socket
 *                 - Sn_MR_MACRAW: MAC raw socket
 * @param port Local port number (0 = auto-assign from 0xC000)
 * @param flag Socket flags (SF_xxx combinations):
 *             - TCP: SF_TCP_NODELAY, SF_IO_NONBLOCK
 *             - UDP: SF_IGMP_VER2, SF_MULTI_ENABLE, SF_UNI_BLOCK, SF_IO_NONBLOCK
 *             - MACRAW: SF_ETHER_OWN
 * @return Socket number (sn) on success, negative error code on failure
 *
 * @note TCP sockets require IP address to be configured
 * @note Port 0 triggers auto-assignment starting from 0xC000
 */
int8_t socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag);

/**
 * @brief Close a socket
 *
 * Closes the socket and releases all resources.
 * Waits for socket to reach SOCK_CLOSED state.
 *
 * @param sn Socket number
 * @return SOCK_OK on success, negative error code on failure
 *
 * @note Thread-safe, blocks until socket is closed
 */
int8_t closesocket(uint8_t sn);

/**
 * @brief Disconnect a TCP socket
 *
 * Gracefully closes a TCP connection by sending FIN packet.
 * In blocking mode, waits for disconnect to complete.
 * In non-blocking mode, returns SOCK_BUSY immediately.
 *
 * @param sn Socket number
 * @return SOCK_OK on success, SOCK_BUSY if non-blocking, negative on error
 *
 * @note Only for TCP sockets in ESTABLISHED or CLOSE_WAIT state
 */
int8_t disconnect(uint8_t sn);

/**
 * @brief Control socket options
 *
 * Set or get various socket control parameters.
 *
 * @param sn Socket number
 * @param cstype Control type (CS_xxx)
 * @param arg Pointer to control argument:
 *            - CS_SET_IOMODE: uint8_t* (SOCK_IO_BLOCK or SOCK_IO_NONBLOCK)
 *            - CS_GET_IOMODE: uint8_t* (output)
 *            - CS_GET_MAXTXBUF: uint16_t* (output)
 *            - CS_GET_MAXRXBUF: uint16_t* (output)
 *            - CS_CLR_INTERRUPT: uint8_t* (SIK_xxx flags)
 *            - CS_GET_INTERRUPT: uint8_t* (output)
 *            - CS_SET_INTMASK: uint8_t* (SIK_xxx flags)
 *            - CS_GET_INTMASK: uint8_t* (output)
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t ctlsocket(uint8_t sn, ctlsock_type cstype, void *arg);

/**
 * @brief Set socket option
 *
 * Configure socket-specific parameters.
 *
 * @param sn Socket number
 * @param sotype Option type (SO_xxx)
 * @param arg Pointer to option value:
 *            - SO_TTL: uint8_t* (Time-To-Live value)
 *            - SO_TOS: uint8_t* (Type-Of-Service value)
 *            - SO_MSS: uint16_t* (Maximum Segment Size)
 *            - SO_DESTIP: uint8_t[4] (Destination IP)
 *            - SO_DESTPORT: uint16_t* (Destination port)
 *            - SO_KEEPALIVESEND: NULL (send keep-alive now)
 *            - SO_KEEPALIVEAUTO: uint8_t* (auto keep-alive timer)
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t setsockopt(uint8_t sn, sockopt_type sotype, void *arg);

/**
 * @brief Get socket option
 *
 * Retrieve socket-specific parameters.
 *
 * @param sn Socket number
 * @param sotype Option type (SO_xxx)
 * @param arg Pointer to buffer for option value:
 *            - SO_FLAG: uint8_t* (socket flags, read-only)
 *            - SO_TTL: uint8_t* (Time-To-Live value)
 *            - SO_TOS: uint8_t* (Type-Of-Service value)
 *            - SO_MSS: uint16_t* (Maximum Segment Size)
 *            - SO_DESTIP: uint8_t[4] (Destination IP)
 *            - SO_DESTPORT: uint16_t* (Destination port)
 *            - SO_KEEPALIVEAUTO: uint16_t* (auto keep-alive timer)
 *            - SO_SENDBUF: uint16_t* (free TX buffer, read-only)
 *            - SO_RECVBUF: uint16_t* (received data size, read-only)
 *            - SO_STATUS: uint8_t* (socket status, read-only)
 *            - SO_REMAINSIZE: uint16_t* (remaining packet size, read-only)
 *            - SO_PACKINFO: uint8_t* (packet info, read-only, UDP only)
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t getsockopt(uint8_t sn, sockopt_type sotype, void *arg);

/******************************************************************************
 *                          TCP FUNCTIONS                                     *
 ******************************************************************************/

/**
 * @brief Put socket into listen mode (TCP server)
 *
 * Transitions a TCP socket from SOCK_INIT to SOCK_LISTEN state.
 * After calling listen(), the socket can accept incoming connections.
 *
 * @param sn Socket number
 * @return SOCK_OK on success, negative error code on failure
 *
 * @note Socket must be in SOCK_INIT state (after socket() call)
 * @note Socket will remain in LISTEN state until connection or close
 */
int8_t listen(uint8_t sn);

/**
 * @brief Connect to remote server (TCP client)
 *
 * Initiates a TCP connection to the specified remote address and port.
 * In blocking mode, waits for connection to be established.
 * In non-blocking mode, returns SOCK_BUSY immediately.
 *
 * @param sn Socket number
 * @param addr Remote IP address (4 bytes)
 * @param port Remote port number
 * @return SOCK_OK on success, SOCK_BUSY if non-blocking, negative on error
 *
 * @note Socket must be in SOCK_INIT state
 * @note Connection timeout handled by W5500 hardware (configurable)
 */
int8_t connect(uint8_t sn, uint8_t *addr, uint16_t port);

/**
 * @brief Send data over TCP socket
 *
 * Sends data over an established TCP connection.
 * In blocking mode, waits for TX buffer space if needed.
 * In non-blocking mode, returns SOCK_BUSY if buffer full.
 *
 * @param sn Socket number
 * @param buf Pointer to data buffer
 * @param len Number of bytes to send (limited by TX buffer size)
 * @return Number of bytes queued for sending, or negative error code
 *
 * @note Socket must be in SOCK_ESTABLISHED or SOCK_CLOSE_WAIT state
 * @note Actual transmission handled by W5500 hardware
 * @note Check Sn_IR_SENDOK interrupt to confirm transmission completion
 */
int32_t send(uint8_t sn, uint8_t *buf, uint16_t len);

/**
 * @brief Send all data over TCP socket in blocking mode
 *
 * @param sn Socket number
 * @param buf Pointer to data buffer
 * @param len Number of bytes to send
 * @return Total number of bytes sent
 */
int send_all_blocking(uint8_t sn, const uint8_t *buf, int len);

/**
 * @brief Receive data from TCP socket
 *
 * Receives data from an established TCP connection.
 * In blocking mode, waits for data if RX buffer empty.
 * In non-blocking mode, returns SOCK_BUSY if no data.
 *
 * @param sn Socket number
 * @param buf Pointer to receive buffer
 * @param len Maximum number of bytes to receive
 * @return Number of bytes received, SOCK_BUSY, or negative error code
 *
 * @note Socket must be in SOCK_ESTABLISHED or SOCK_CLOSE_WAIT state
 * @note Returns SOCKERR_SOCKCLOSED if remote closed connection
 */
int32_t recv(uint8_t sn, uint8_t *buf, uint16_t len);

/******************************************************************************
 *                          UDP FUNCTIONS                                     *
 ******************************************************************************/

/**
 * @brief Send datagram over UDP socket
 *
 * Sends a UDP datagram to the specified destination.
 * In blocking mode, waits for TX buffer space if needed.
 * In non-blocking mode, returns SOCK_BUSY if buffer full.
 *
 * @param sn Socket number
 * @param buf Pointer to data buffer
 * @param len Number of bytes to send (limited by TX buffer size)
 * @param addr Destination IP address (4 bytes)
 * @param port Destination port number
 * @return Number of bytes sent, or negative error code
 *
 * @note Socket must be in SOCK_UDP state
 * @note Maximum datagram size depends on MTU (typically 1472 bytes)
 */
int32_t sendto(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t port);

/**
 * @brief Receive datagram from UDP socket
 *
 * Receives a UDP datagram with source address and port.
 * In blocking mode, waits for data if RX buffer empty.
 * In non-blocking mode, returns SOCK_BUSY if no data.
 *
 * Supports fragmented packet reception:
 * - PACK_FIRST: First call for a new packet
 * - PACK_REMAINED: More data remains in current packet
 * - PACK_COMPLETED: All packet data received
 *
 * @param sn Socket number
 * @param buf Pointer to receive buffer
 * @param len Maximum number of bytes to receive
 * @param addr Pointer to buffer for source IP address (4 bytes, output)
 * @param port Pointer to buffer for source port number (output)
 * @return Number of bytes received, SOCK_BUSY, or negative error code
 *
 * @note Socket must be in SOCK_UDP, SOCK_IPRAW, or SOCK_MACRAW state
 * @note For UDP: includes 8-byte header (IP[4] + port[2] + length[2])
 * @note For IPRAW: includes 6-byte header (IP[4] + length[2])
 * @note For MACRAW: includes 2-byte header (length[2])
 * @note Use SO_PACKINFO to check packet completion status
 */
int32_t recvfrom(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t *port);

/******************************************************************************
 *                          USAGE EXAMPLES                                    *
 ******************************************************************************/

/**
 * @example TCP Server Example
 * @code
 * // Create TCP server socket on port 80
 * int8_t sn = socket(0, Sn_MR_TCP, 80, SF_IO_NONBLOCK);
 * if (sn < 0) {
 *     // Error handling
 *     return;
 * }
 *
 * // Put socket into listen mode
 * if (listen(sn) != SOCK_OK) {
 *     closesocket(sn);
 *     return;
 * }
 *
 * // Check for connection
 * uint8_t status;
 * getsockopt(sn, SO_STATUS, &status);
 * if (status == SOCK_ESTABLISHED) {
 *     // Receive data
 *     uint8_t buf[1024];
 *     int32_t len = recv(sn, buf, sizeof(buf));
 *     if (len > 0) {
 *         // Process received data
 *         send(sn, buf, len); // Echo back
 *     }
 * }
 * @endcode
 */

/**
 * @example TCP Client Example
 * @code
 * // Create TCP client socket
 * int8_t sn = socket(0, Sn_MR_TCP, 0, 0); // Auto-assign port
 * if (sn < 0) {
 *     return;
 * }
 *
 * // Connect to server
 * uint8_t server_ip[] = {192, 168, 1, 100};
 * if (connect(sn, server_ip, 80) != SOCK_OK) {
 *     closesocket(sn);
 *     return;
 * }
 *
 * // Send HTTP request
 * const char *request = "GET / HTTP/1.1\r\n\r\n";
 * send(sn, (uint8_t*)request, strlen(request));
 *
 * // Receive response
 * uint8_t buf[512];
 * int32_t len = recv(sn, buf, sizeof(buf));
 * if (len > 0) {
 *     // Process response
 * }
 *
 * // Close connection
 * disconnect(sn);
 * closesocket(sn);
 * @endcode
 */

/**
 * @example UDP Example
 * @code
 * // Create UDP socket on port 5000
 * int8_t sn = socket(0, Sn_MR_UDP, 5000, 0);
 * if (sn < 0) {
 *     return;
 * }
 *
 * // Send UDP datagram
 * uint8_t dest_ip[] = {192, 168, 1, 255}; // Broadcast
 * const char *msg = "Hello, World!";
 * sendto(sn, (uint8_t*)msg, strlen(msg), dest_ip, 5000);
 *
 * // Receive UDP datagram
 * uint8_t buf[512];
 * uint8_t src_ip[4];
 * uint16_t src_port;
 * int32_t len = recvfrom(sn, buf, sizeof(buf), src_ip, &src_port);
 * if (len > 0) {
 *     // Process received datagram
 *     log_printf("Received %ld bytes from %d.%d.%d.%d:%d\n",
 *            len, src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port);
 * }
 *
 * closesocket(sn);
 * @endcode
 */

#endif /* ETH_SOCKET_H */