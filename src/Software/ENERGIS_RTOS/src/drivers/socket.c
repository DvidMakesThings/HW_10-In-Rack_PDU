/**
 * @file socket.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers05 5. Ethernet Socket Implementation
 * @ingroup drivers
 * @brief BSD-like socket API implementation for W5500 Ethernet controller
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 * @details
 * Thread-safe BSD socket-like API for W5500.
 *
 * Key Features:
 * - Thread-safe operation via w5500_spi_mutex
 * - Blocking and non-blocking I/O modes
 * - TCP, UDP, IPRAW, MACRAW protocol support
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/******************************************************************************
 *                          PRIVATE DEFINITIONS                               *
 ******************************************************************************/

/** Starting port number for auto-assigned ports */
#define SOCK_ANY_PORT_NUM 0xC000

/** Socket state tracking */
static uint16_t sock_any_port = SOCK_ANY_PORT_NUM;
static uint16_t sock_io_mode = 0;
static uint16_t sock_is_sending = 0;
static uint16_t sock_remained_size[W5500_SOCKET_COUNT] = {0};
uint8_t sock_pack_info[W5500_SOCKET_COUNT] = {0};

/******************************************************************************
 *                          VALIDATION MACROS                                 *
 ******************************************************************************/

/**
 * @brief Check if socket number is valid
 */
#define CHECK_SOCKNUM()                                                                            \
    do {                                                                                           \
        if (sn >= W5500_SOCKET_COUNT)                                                              \
            return SOCKERR_SOCKNUM;                                                                \
    } while (0)

/**
 * @brief Check if socket mode matches expected mode
 */
#define CHECK_SOCKMODE(mode)                                                                       \
    do {                                                                                           \
        if ((getSn_MR(sn) & 0x0F) != mode)                                                         \
            return SOCKERR_SOCKMODE;                                                               \
    } while (0)

/**
 * @brief Check if socket is initialized (SOCK_INIT state for TCP)
 */
#define CHECK_SOCKINIT()                                                                           \
    do {                                                                                           \
        if ((getSn_SR(sn) != SOCK_INIT))                                                           \
            return SOCKERR_SOCKINIT;                                                               \
    } while (0)

/**
 * @brief Check if data length is valid
 */
#define CHECK_SOCKDATA()                                                                           \
    do {                                                                                           \
        if (len == 0)                                                                              \
            return SOCKERR_DATALEN;                                                                \
    } while (0)

/******************************************************************************
 *                          SOCKET MANAGEMENT                                 *
 ******************************************************************************/

/**
 * @brief Create and initialize a socket
 *
 * @param sn Socket number (0-7)
 * @param protocol Protocol type (Sn_MR_TCP, Sn_MR_UDP, Sn_MR_IPRAW, Sn_MR_MACRAW)
 * @param port Local port number (0 = auto-assign)
 * @param flag Socket flags (SF_xxx)
 * @return Socket number (sn) on success, negative error code on failure
 */
int8_t socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag) {
    CHECK_SOCKNUM();

    /* Validate protocol */
    switch (protocol) {
    case Sn_MR_TCP: {
        /* Check if IP address is configured for TCP */
        uint32_t taddr;
        getSIPR((uint8_t *)&taddr);
        if (taddr == 0)
            return SOCKERR_SOCKINIT;
        break;
    }
    case Sn_MR_UDP:
    case Sn_MR_MACRAW:
    case Sn_MR_IPRAW:
        break;
    default:
        return SOCKERR_SOCKMODE;
    }

    /* Validate socket flags */
    if ((flag & 0x04) != 0)
        return SOCKERR_SOCKFLAG;

    if (flag != 0) {
        switch (protocol) {
        case Sn_MR_TCP:
            if ((flag & (SF_TCP_NODELAY | SF_IO_NONBLOCK)) == 0)
                return SOCKERR_SOCKFLAG;
            break;
        case Sn_MR_UDP:
            if (flag & SF_IGMP_VER2) {
                if ((flag & SF_MULTI_ENABLE) == 0)
                    return SOCKERR_SOCKFLAG;
            }
            if (flag & SF_UNI_BLOCK) {
                if ((flag & SF_MULTI_ENABLE) == 0)
                    return SOCKERR_SOCKFLAG;
            }
            break;
        default:
            break;
        }
    }

    /* Close socket if already open */
    closesocket(sn);

    /* Set socket mode register (protocol + flags) */
    setSn_MR(sn, (protocol | (flag & 0xF0)));

    /* Auto-assign port if not specified */
    if (!port) {
        port = sock_any_port++;
        if (sock_any_port == 0xFFF0)
            sock_any_port = SOCK_ANY_PORT_NUM;
    }

    /* Configure socket port */
    setSn_PORT(sn, port);

    /* Issue OPEN command */
    setSn_CR(sn, Sn_CR_OPEN);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    /* Clear socket state */
    sock_io_mode &= ~(1 << sn);
    sock_io_mode |= ((flag & SF_IO_NONBLOCK) << sn);
    sock_is_sending &= ~(1 << sn);
    sock_remained_size[sn] = 0;
    sock_pack_info[sn] = PACK_COMPLETED;

    /* Wait for socket to open */
    while (getSn_SR(sn) == SOCK_CLOSED)
        vTaskDelay(pdMS_TO_TICKS(1));

    W5500_SOCK_DBG("[Socket %u] Opened (protocol=0x%02X, port=%u)\r\n", sn, protocol, port);
    return (int8_t)sn;
}

/**
 * @brief Close a socket
 *
 * @param sn Socket number
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t closesocket(uint8_t sn) {
    CHECK_SOCKNUM();

    /* Issue CLOSE command */
    setSn_CR(sn, Sn_CR_CLOSE);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    /* Clear all interrupts */
    setSn_IR(sn, 0xFF);

    /* Clear socket state */
    sock_io_mode &= ~(1 << sn);
    sock_is_sending &= ~(1 << sn);
    sock_remained_size[sn] = 0;
    sock_pack_info[sn] = 0;

    /* Wait for socket to close */
    while (getSn_SR(sn) != SOCK_CLOSED)
        vTaskDelay(pdMS_TO_TICKS(1));

    W5500_SOCK_DBG("[Socket %u] Closed\r\n", sn);
    return SOCK_OK;
}

/**
 * @brief Disconnect a TCP socket
 *
 * @param sn Socket number
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t disconnect(uint8_t sn) {
    CHECK_SOCKNUM();
    CHECK_SOCKMODE(Sn_MR_TCP);

    /* Issue DISCON command */
    setSn_CR(sn, Sn_CR_DISCON);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    sock_is_sending &= ~(1 << sn);

    /* Non-blocking mode: return immediately */
    if (sock_io_mode & (1 << sn))
        return SOCK_BUSY;

    /* Blocking mode: wait for disconnect */
    while (getSn_SR(sn) != SOCK_CLOSED) {
        if (getSn_IR(sn) & Sn_IR_TIMEOUT) {
            closesocket(sn);
            return SOCKERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    W5500_SOCK_DBG("[Socket %u] Disconnected\r\n", sn);
    return SOCK_OK;
}

/******************************************************************************
 *                          TCP FUNCTIONS                                     *
 ******************************************************************************/

/**
 * @brief Put socket into listen mode (TCP server)
 *
 * @param sn Socket number
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t listen(uint8_t sn) {
    CHECK_SOCKNUM();
    CHECK_SOCKMODE(Sn_MR_TCP);
    CHECK_SOCKINIT();

    /* Issue LISTEN command */
    setSn_CR(sn, Sn_CR_LISTEN);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    /* Wait for LISTEN with a bounded timeout */
    const TickType_t t_start = xTaskGetTickCount();
    for (;;) {
        uint8_t sr = getSn_SR(sn);
        if (sr == SOCK_LISTEN) {
            W5500_SOCK_DBG("[Socket %u] Listening\n", sn);
            return SOCK_OK;
        }
        /* If it moved to a clearly wrong state, abort */
        if (sr == SOCK_CLOSED || sr == SOCK_INIT || sr == SOCK_ESTABLISHED ||
            sr == SOCK_CLOSE_WAIT) {
            closesocket(sn);
            return SOCKERR_SOCKCLOSED;
        }
        /* Timeout ~500 ms */
        if ((xTaskGetTickCount() - t_start) > pdMS_TO_TICKS(500)) {
            closesocket(sn);
            return SOCKERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/**
 * @brief Connect to remote server (TCP client)
 *
 * @param sn Socket number
 * @param addr Remote IP address (4 bytes)
 * @param port Remote port number
 * @return SOCK_OK on success, SOCK_BUSY if in progress, negative on error
 */
int8_t connect(uint8_t sn, uint8_t *addr, uint16_t port) {
    CHECK_SOCKNUM();
    CHECK_SOCKMODE(Sn_MR_TCP);
    CHECK_SOCKINIT();

    /* Validate IP address */
    uint32_t taddr;
    taddr = ((uint32_t)addr[0] & 0x000000FF);
    taddr = (taddr << 8) + ((uint32_t)addr[1] & 0x000000FF);
    taddr = (taddr << 8) + ((uint32_t)addr[2] & 0x000000FF);
    taddr = (taddr << 8) + ((uint32_t)addr[3] & 0x000000FF);
    if (taddr == 0xFFFFFFFF || taddr == 0)
        return SOCKERR_IPINVALID;

    /* Validate port */
    if (port == 0)
        return SOCKERR_PORTZERO;

    /* Set destination IP and port */
    setSn_DIPR(sn, addr);
    setSn_DPORT(sn, port);

    /* Issue CONNECT command */
    setSn_CR(sn, Sn_CR_CONNECT);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    /* Non-blocking mode: return immediately */
    if (sock_io_mode & (1 << sn))
        return SOCK_BUSY;

    /* Blocking mode: wait for connection */
    while (getSn_SR(sn) != SOCK_ESTABLISHED) {
        if (getSn_IR(sn) & Sn_IR_TIMEOUT) {
            setSn_IR(sn, Sn_IR_TIMEOUT);
            return SOCKERR_TIMEOUT;
        }
        if (getSn_SR(sn) == SOCK_CLOSED) {
            return SOCKERR_SOCKCLOSED;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    W5500_SOCK_DBG("[Socket %u] Connected to %u.%u.%u.%u:%u\r\n", sn, addr[0], addr[1], addr[2],
                   addr[3], port);
    return SOCK_OK;
}

/**
 * @brief Send data over TCP socket
 *
 * @param sn Socket number
 * @param buf Pointer to data buffer
 * @param len Number of bytes to send
 * @return Number of bytes sent, or negative error code
 */
int32_t send(uint8_t sn, uint8_t *buf, uint16_t len) {
    uint8_t tmp = 0;
    uint16_t freesize = 0;

    CHECK_SOCKNUM();
    CHECK_SOCKMODE(Sn_MR_TCP);
    CHECK_SOCKDATA();

    /* Check socket state */
    tmp = getSn_SR(sn);
    if (tmp != SOCK_ESTABLISHED && tmp != SOCK_CLOSE_WAIT)
        return SOCKERR_SOCKSTATUS;

    /* Check if previous send completed */
    if (sock_is_sending & (1 << sn)) {
        tmp = getSn_IR(sn);
        if (tmp & Sn_IR_SENDOK) {
            setSn_IR(sn, Sn_IR_SENDOK);
            sock_is_sending &= ~(1 << sn);
        } else if (tmp & Sn_IR_TIMEOUT) {
            closesocket(sn);
            return SOCKERR_TIMEOUT;
        } else {
            return SOCK_BUSY;
        }
    }

    /* Limit to max TX buffer size */
    freesize = getSn_TxMAX(sn);
    if (len > freesize)
        len = freesize;

    /* Wait for TX buffer space */
    while (1) {
        freesize = getSn_TX_FSR(sn);
        tmp = getSn_SR(sn);

        if ((tmp != SOCK_ESTABLISHED) && (tmp != SOCK_CLOSE_WAIT)) {
            closesocket(sn);
            return SOCKERR_SOCKSTATUS;
        }

        if ((sock_io_mode & (1 << sn)) && (len > freesize))
            return SOCK_BUSY;

        if (len <= freesize)
            break;

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Write data to TX buffer */
    eth_send_data(sn, buf, len);

    /* Issue SEND command */
    setSn_CR(sn, Sn_CR_SEND);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    sock_is_sending |= (1 << sn);

    W5500_SOCK_DBG("[Socket %u] Sent %u bytes\r\n", sn, len);
    return (int32_t)len;
}

/**
 * @brief Send all data over TCP socket in blocking mode
 *
 * @param sn Socket number
 * @param buf Pointer to data buffer
 * @param len Number of bytes to send
 * @return Total number of bytes sent
 */
int send_all_blocking(uint8_t sn, const uint8_t *buf, int len) {
    if (!buf || len <= 0)
        return 0;

    const uint16_t tx_max = getSn_TxMAX(sn);
    int total = 0;

    while (total < len) {
        /* Wait for some free space */
        uint16_t freesz = 0;
        TickType_t t0 = xTaskGetTickCount();
        do {
            freesz = getSn_TX_FSR(sn);
            if (getSn_SR(sn) != SOCK_ESTABLISHED && getSn_SR(sn) != SOCK_CLOSE_WAIT)
                return total; /* peer closed or bad state */
            if ((xTaskGetTickCount() - t0) > pdMS_TO_TICKS(5000))
                return total; /* TX stalled */
            vTaskDelay(pdMS_TO_TICKS(1));
        } while (freesz == 0);

        /* Limit write to what fits and to tx_max to avoid wrap surprises */
        uint16_t chunk = (uint16_t)(len - total);
        if (chunk > freesz)
            chunk = freesz;
        if (chunk > tx_max)
            chunk = tx_max;

        /* Copy payload into TX ring with wrap handling */
        eth_send_data(sn, (uint8_t *)buf + total, chunk);

        /* Kick SEND and wait for SENDOK (or TIMEOUT) */
        setSn_CR(sn, Sn_CR_SEND);
        while (getSn_CR(sn))
            vTaskDelay(pdMS_TO_TICKS(1));

        TickType_t ts = xTaskGetTickCount();
        for (;;) {
            uint8_t ir = getSn_IR(sn);
            if (ir & Sn_IR_SENDOK) {
                setSn_IR(sn, Sn_IR_SENDOK);
                break;
            }
            if (ir & Sn_IR_TIMEOUT) {
                setSn_IR(sn, Sn_IR_TIMEOUT);
                return total; /* give up on timeout */
            }
            if ((xTaskGetTickCount() - ts) > pdMS_TO_TICKS(5000))
                return total; /* safety timeout */
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        total += chunk;
    }
    return total;
}

/**
 * @brief Receive data from TCP socket
 *
 * @param sn Socket number
 * @param buf Pointer to receive buffer
 * @param len Maximum number of bytes to receive
 * @return Number of bytes received, or negative error code
 */
int32_t recv(uint8_t sn, uint8_t *buf, uint16_t len) {
    uint8_t tmp = 0;
    uint16_t recvsize = 0;

    CHECK_SOCKNUM();
    CHECK_SOCKMODE(Sn_MR_TCP);
    CHECK_SOCKDATA();

    /* Check RX received size */
    recvsize = getSn_RX_RSR(sn);

    if (recvsize == 0) {
        /* Check socket state */
        tmp = getSn_SR(sn);
        if (tmp == SOCK_CLOSED)
            return SOCKERR_SOCKCLOSED;
        if (tmp == SOCK_CLOSE_WAIT) {
            if (recvsize != 0)
                return recvsize; /* Read remaining data */
            else if (getSn_TX_FSR(sn) == getSn_TxMAX(sn)) {
                /* Close wait and all data sent */
                closesocket(sn);
                return SOCKERR_SOCKCLOSED;
            }
        }
        return SOCK_BUSY;
    }

    /* Limit to requested length */
    if (recvsize > len)
        recvsize = len;

    /* Read data from RX buffer */
    eth_recv_data(sn, buf, recvsize);

    /* Issue RECV command to update pointers */
    setSn_CR(sn, Sn_CR_RECV);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    W5500_SOCK_DBG("[Socket %u] Received %u bytes\r\n", sn, recvsize);
    return (int32_t)recvsize;
}

/******************************************************************************
 *                          UDP FUNCTIONS                                     *
 ******************************************************************************/

/**
 * @brief Send datagram over UDP socket
 *
 * @param sn Socket number
 * @param buf Pointer to data buffer
 * @param len Number of bytes to send
 * @param addr Destination IP address (4 bytes)
 * @param port Destination port number
 * @return Number of bytes sent, or negative error code
 */
int32_t sendto(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t port) {
    uint8_t tmp = 0;
    uint16_t freesize = 0;

    CHECK_SOCKNUM();
    CHECK_SOCKMODE(Sn_MR_UDP);
    CHECK_SOCKDATA();

    /* Validate IP and port */
    if (addr[0] == 0x00 && addr[1] == 0x00 && addr[2] == 0x00 && addr[3] == 0x00)
        return SOCKERR_IPINVALID;
    if (port == 0)
        return SOCKERR_PORTZERO;

    /* Check socket state */
    tmp = getSn_SR(sn);
    if (tmp != SOCK_UDP)
        return SOCKERR_SOCKSTATUS;

    /* Set destination IP and port */
    setSn_DIPR(sn, addr);
    setSn_DPORT(sn, port);

    /* Limit to max TX buffer size */
    freesize = getSn_TxMAX(sn);
    if (len > freesize)
        len = freesize;

    /* Wait for TX buffer space */
    while (1) {
        freesize = getSn_TX_FSR(sn);
        tmp = getSn_SR(sn);

        if (tmp != SOCK_UDP)
            return SOCKERR_SOCKSTATUS;

        if ((sock_io_mode & (1 << sn)) && (len > freesize))
            return SOCK_BUSY;

        if (len <= freesize)
            break;

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    /* Write data to TX buffer */
    eth_send_data(sn, buf, len);

    /* Issue SEND command */
    setSn_CR(sn, Sn_CR_SEND);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    W5500_SOCK_DBG("[Socket %u] Sent %u bytes to %u.%u.%u.%u:%u\r\n", sn, len, addr[0], addr[1],
                   addr[2], addr[3], port);
    return (int32_t)len;
}

/**
 * @brief Receive datagram from UDP socket
 *
 * @param sn Socket number
 * @param buf Pointer to receive buffer
 * @param len Maximum number of bytes to receive
 * @param addr Pointer to buffer for source IP address (4 bytes)
 * @param port Pointer to buffer for source port number
 * @return Number of bytes received, or negative error code
 */
int32_t recvfrom(uint8_t sn, uint8_t *buf, uint16_t len, uint8_t *addr, uint16_t *port) {
    uint8_t head[8];
    uint16_t pack_len = 0;
    uint8_t mr = 0;

    CHECK_SOCKNUM();
    CHECK_SOCKDATA();

    /* Get socket mode */
    mr = getSn_MR(sn);

    /* Check if data available */
    if (sock_remained_size[sn] == 0) {
        /* Wait for data */
        while (1) {
            pack_len = getSn_RX_RSR(sn);
            if (pack_len != 0)
                break;
            if ((sock_io_mode & (1 << sn)))
                return SOCK_BUSY;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    /* Process based on protocol */
    switch (mr & 0x07) {
    case Sn_MR_UDP:
        if (sock_remained_size[sn] == 0) {
            /* Read UDP header (8 bytes: IP[4] + port[2] + length[2]) */
            eth_recv_data(sn, head, 8);
            setSn_CR(sn, Sn_CR_RECV);
            while (getSn_CR(sn))
                vTaskDelay(pdMS_TO_TICKS(1));

            /* Parse header */
            addr[0] = head[0];
            addr[1] = head[1];
            addr[2] = head[2];
            addr[3] = head[3];
            *port = head[4];
            *port = (*port << 8) + head[5];
            sock_remained_size[sn] = head[6];
            sock_remained_size[sn] = (sock_remained_size[sn] << 8) + head[7];
            sock_pack_info[sn] = PACK_FIRST;
        }

        /* Limit to available data */
        if (len < sock_remained_size[sn])
            pack_len = len;
        else
            pack_len = sock_remained_size[sn];

        /* Read data */
        eth_recv_data(sn, buf, pack_len);
        break;

    case Sn_MR_IPRAW:
        if (sock_remained_size[sn] == 0) {
            /* Read IPRAW header (6 bytes: IP[4] + length[2]) */
            eth_recv_data(sn, head, 6);
            setSn_CR(sn, Sn_CR_RECV);
            while (getSn_CR(sn))
                vTaskDelay(pdMS_TO_TICKS(1));

            /* Parse header */
            addr[0] = head[0];
            addr[1] = head[1];
            addr[2] = head[2];
            addr[3] = head[3];
            sock_remained_size[sn] = head[4];
            sock_remained_size[sn] = (sock_remained_size[sn] << 8) + head[5];
            sock_pack_info[sn] = PACK_FIRST;
        }

        /* Limit to available data */
        if (len < sock_remained_size[sn])
            pack_len = len;
        else
            pack_len = sock_remained_size[sn];

        /* Read data */
        eth_recv_data(sn, buf, pack_len);
        break;

    case Sn_MR_MACRAW:
        if (sock_remained_size[sn] == 0) {
            /* Read MACRAW header (2 bytes: length[2]) */
            eth_recv_data(sn, head, 2);
            setSn_CR(sn, Sn_CR_RECV);
            while (getSn_CR(sn))
                vTaskDelay(pdMS_TO_TICKS(1));

            /* Parse header */
            sock_remained_size[sn] = head[0];
            sock_remained_size[sn] = (sock_remained_size[sn] << 8) + head[1] - 2;

            /* Validate packet length */
            if (sock_remained_size[sn] > 1514) {
                closesocket(sn);
                return SOCKFATAL_PACKLEN;
            }
            sock_pack_info[sn] = PACK_FIRST;
        }

        /* Limit to available data */
        if (len < sock_remained_size[sn])
            pack_len = len;
        else
            pack_len = sock_remained_size[sn];

        /* Read data */
        eth_recv_data(sn, buf, pack_len);
        break;

    default:
        eth_recv_ignore(sn, pack_len);
        sock_remained_size[sn] = pack_len;
        break;
    }

    /* Issue RECV command to update pointers */
    setSn_CR(sn, Sn_CR_RECV);
    while (getSn_CR(sn))
        vTaskDelay(pdMS_TO_TICKS(1));

    /* Update remaining size */
    sock_remained_size[sn] -= pack_len;

    /* Update packet info */
    if (sock_remained_size[sn] != 0) {
        sock_pack_info[sn] |= PACK_REMAINED;
    } else {
        sock_pack_info[sn] = PACK_COMPLETED;
    }

    W5500_SOCK_DBG("[Socket %u] Received %u bytes from %u.%u.%u.%u:%u\r\n", sn, pack_len, addr[0],
                   addr[1], addr[2], addr[3], *port);
    return (int32_t)pack_len;
}

/******************************************************************************
 *                          SOCKET CONTROL                                    *
 ******************************************************************************/

/**
 * @brief Control socket options
 *
 * @param sn Socket number
 * @param cstype Control type
 * @param arg Pointer to control argument (type depends on cstype)
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t ctlsocket(uint8_t sn, ctlsock_type cstype, void *arg) {
    uint8_t tmp = 0;
    CHECK_SOCKNUM();

    switch (cstype) {
    case CS_SET_IOMODE:
        tmp = *((uint8_t *)arg);
        if (tmp == SF_IO_NONBLOCK)
            sock_io_mode |= (1 << sn);
        else if (tmp == SF_IO_BLOCK)
            sock_io_mode &= ~(1 << sn);
        else
            return SOCKERR_ARG;
        break;

    case CS_GET_IOMODE:
        *((uint8_t *)arg) = (uint8_t)((sock_io_mode >> sn) & 0x0001);
        break;

    case CS_GET_MAXTXBUF:
        *((uint16_t *)arg) = getSn_TxMAX(sn);
        break;

    case CS_GET_MAXRXBUF:
        *((uint16_t *)arg) = getSn_RxMAX(sn);
        break;

    case CS_CLR_INTERRUPT:
        if ((*(uint8_t *)arg) > SIK_ALL)
            return SOCKERR_ARG;
        setSn_IR(sn, *(uint8_t *)arg);
        break;

    case CS_GET_INTERRUPT:
        *((uint8_t *)arg) = getSn_IR(sn);
        break;

    case CS_SET_INTMASK:
        if ((*(uint8_t *)arg) > SIK_ALL)
            return SOCKERR_ARG;
        setSn_IMR(sn, *(uint8_t *)arg);
        break;

    case CS_GET_INTMASK:
        *((uint8_t *)arg) = getSn_IMR(sn);
        break;

    default:
        return SOCKERR_ARG;
    }

    return SOCK_OK;
}

/**
 * @brief Set socket option
 *
 * @param sn Socket number
 * @param sotype Option type
 * @param arg Pointer to option value (type depends on sotype)
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t setsockopt(uint8_t sn, sockopt_type sotype, void *arg) {
    CHECK_SOCKNUM();

    switch (sotype) {
    case SO_TTL:
        setSn_TTL(sn, *(uint8_t *)arg);
        break;

    case SO_TOS:
        setSn_TOS(sn, *(uint8_t *)arg);
        break;

    case SO_MSS:
        setSn_MSSR(sn, *(uint16_t *)arg);
        break;

    case SO_DESTIP:
        setSn_DIPR(sn, (uint8_t *)arg);
        break;

    case SO_DESTPORT:
        setSn_DPORT(sn, *(uint16_t *)arg);
        break;

    case SO_KEEPALIVESEND:
        CHECK_SOCKMODE(Sn_MR_TCP);
        if (getSn_KPALVTR(sn) != 0)
            return SOCKERR_SOCKOPT;
        setSn_CR(sn, Sn_CR_SEND_KEEP);
        while (getSn_CR(sn) != 0) {
            if (getSn_IR(sn) & Sn_IR_TIMEOUT) {
                setSn_IR(sn, Sn_IR_TIMEOUT);
                return SOCKERR_TIMEOUT;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        break;

    case SO_KEEPALIVEAUTO:
        CHECK_SOCKMODE(Sn_MR_TCP);
        setSn_KPALVTR(sn, *(uint8_t *)arg);
        break;

    default:
        return SOCKERR_ARG;
    }

    return SOCK_OK;
}

/**
 * @brief Get socket option
 *
 * @param sn Socket number
 * @param sotype Option type
 * @param arg Pointer to buffer for option value (type depends on sotype)
 * @return SOCK_OK on success, negative error code on failure
 */
int8_t getsockopt(uint8_t sn, sockopt_type sotype, void *arg) {
    CHECK_SOCKNUM();

    switch (sotype) {
    case SO_FLAG:
        *(uint8_t *)arg = getSn_MR(sn) & 0xF0;
        break;

    case SO_TTL:
        *(uint8_t *)arg = getSn_TTL(sn);
        break;

    case SO_TOS:
        *(uint8_t *)arg = getSn_TOS(sn);
        break;

    case SO_MSS:
        *(uint16_t *)arg = getSn_MSSR(sn);
        break;

    case SO_DESTIP:
        getSn_DIPR(sn, (uint8_t *)arg);
        break;

    case SO_DESTPORT:
        *(uint16_t *)arg = getSn_DPORT(sn);
        break;

    case SO_KEEPALIVEAUTO:
        CHECK_SOCKMODE(Sn_MR_TCP);
        *(uint16_t *)arg = getSn_KPALVTR(sn);
        break;

    case SO_SENDBUF:
        *(uint16_t *)arg = getSn_TX_FSR(sn);
        break;

    case SO_RECVBUF:
        *(uint16_t *)arg = getSn_RX_RSR(sn);
        break;

    case SO_STATUS:
        *(uint8_t *)arg = getSn_SR(sn);
        break;

    case SO_REMAINSIZE:
        if (getSn_MR(sn) & Sn_MR_TCP)
            *(uint16_t *)arg = getSn_RX_RSR(sn);
        else
            *(uint16_t *)arg = sock_remained_size[sn];
        break;

    case SO_PACKINFO:
        if ((getSn_MR(sn) == Sn_MR_TCP))
            return SOCKERR_SOCKMODE;
        *(uint8_t *)arg = sock_pack_info[sn];
        break;

    default:
        return SOCKERR_SOCKOPT;
    }

    return SOCK_OK;
}

/**
 * @}
 */