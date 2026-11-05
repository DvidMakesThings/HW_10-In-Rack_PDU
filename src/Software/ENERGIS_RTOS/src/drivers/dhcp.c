/**
 * @file src/drivers/dhcp.c
 * @author DvidMakesThings - David Sipos
 *
 * @brief Minimal DHCPv4 client implementation for the W5500.
 *
 * @details
 * See dhcp.h. Builds BOOTP/DHCP frames by hand and exchanges them over a W5500
 * UDP socket. Replies are requested as broadcast (flags = 0x8000) because the
 * device has no IP during acquisition. All wait loops feed the NET heartbeat.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#include "dhcp.h"
#include "socket.h"

#define DHCP_TAG "[DHCP]"

/* ---- Protocol constants ------------------------------------------------- */
#define DHCP_SERVER_PORT 67u
#define DHCP_CLIENT_PORT 68u

#define DHCP_BOOTREQUEST 1u
#define DHCP_BOOTREPLY 2u
#define DHCP_HTYPE_ETH 1u
#define DHCP_HLEN_ETH 6u
#define DHCP_FLAGS_BROADCAST 0x8000u
#define DHCP_MAGIC_COOKIE 0x63825363u

/* DHCP message types (option 53) */
#define DHCPDISCOVER 1u
#define DHCPOFFER 2u
#define DHCPREQUEST 3u
#define DHCPACK 5u
#define DHCPNAK 6u

/* DHCP options */
#define OPT_PAD 0u
#define OPT_SUBNET 1u
#define OPT_ROUTER 3u
#define OPT_DNS 6u
#define OPT_REQUESTED_IP 50u
#define OPT_LEASE_TIME 51u
#define OPT_MSG_TYPE 53u
#define OPT_SERVER_ID 54u
#define OPT_PARAM_REQ 55u
#define OPT_CLIENT_ID 61u
#define OPT_HOSTNAME 12u
#define OPT_END 255u

#define DHCP_FRAME_MAX 548u /* BOOTP(236) + cookie(4) + options */
#define DHCP_HOSTNAME_MAX 31u

/* ---- Module state ------------------------------------------------------- */
static uint8_t s_sock = DHCP_SOCKET_NUM;
static uint8_t s_mac[6] = {0};
static uint32_t s_xid = 0;
static bool s_bound = false;
static uint8_t s_server_id[4] = {0};
static uint8_t s_frame[DHCP_FRAME_MAX];
static char s_hostname[DHCP_HOSTNAME_MAX + 1] = {0};

/* ---- Small helpers ------------------------------------------------------ */
static void dhcp_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

static uint32_t dhcp_make_xid(void) {
    uint32_t t = to_ms_since_boot(get_absolute_time());
    uint32_t m = ((uint32_t)s_mac[2] << 24) | ((uint32_t)s_mac[3] << 16) |
                 ((uint32_t)s_mac[4] << 8) | (uint32_t)s_mac[5];
    return t ^ m ^ 0xC0FFEE00u;
}

/**
 * @brief Assemble a DHCP message into s_frame.
 *
 * @param msg_type DHCPDISCOVER or DHCPREQUEST.
 * @param req_ip   Requested IP (option 50), used for REQUEST; NULL to omit.
 * @param srv_id   Server identifier (option 54), used for REQUEST; NULL to omit.
 * @return Total frame length in bytes.
 */
static uint16_t dhcp_build(uint8_t msg_type, const uint8_t *req_ip, const uint8_t *srv_id) {
    memset(s_frame, 0, sizeof(s_frame));

    s_frame[0] = DHCP_BOOTREQUEST;
    s_frame[1] = DHCP_HTYPE_ETH;
    s_frame[2] = DHCP_HLEN_ETH;
    s_frame[3] = 0; /* hops */

    s_frame[4] = (uint8_t)(s_xid >> 24);
    s_frame[5] = (uint8_t)(s_xid >> 16);
    s_frame[6] = (uint8_t)(s_xid >> 8);
    s_frame[7] = (uint8_t)(s_xid);

    /* secs = 0 (8,9); flags = broadcast (10,11) */
    s_frame[10] = (uint8_t)(DHCP_FLAGS_BROADCAST >> 8);
    s_frame[11] = (uint8_t)(DHCP_FLAGS_BROADCAST & 0xFF);

    /* ciaddr/yiaddr/siaddr/giaddr already zero (12..27) */

    /* chaddr (28..43): client MAC */
    memcpy(&s_frame[28], s_mac, 6);

    /* sname(64)+file(128) left zero (44..235) */

    /* Magic cookie at offset 236 */
    uint16_t i = 236;
    s_frame[i++] = (uint8_t)(DHCP_MAGIC_COOKIE >> 24);
    s_frame[i++] = (uint8_t)(DHCP_MAGIC_COOKIE >> 16);
    s_frame[i++] = (uint8_t)(DHCP_MAGIC_COOKIE >> 8);
    s_frame[i++] = (uint8_t)(DHCP_MAGIC_COOKIE);

    /* Option 53: message type */
    s_frame[i++] = OPT_MSG_TYPE;
    s_frame[i++] = 1;
    s_frame[i++] = msg_type;

    /* Option 61: client identifier (01 + MAC) */
    s_frame[i++] = OPT_CLIENT_ID;
    s_frame[i++] = 7;
    s_frame[i++] = DHCP_HTYPE_ETH;
    memcpy(&s_frame[i], s_mac, 6);
    i += 6;

    /* Option 12: host name, so the DHCP server can register/display us by name. */
    {
        size_t hlen = strlen(s_hostname);
        if (hlen > 0 && hlen <= DHCP_HOSTNAME_MAX) {
            s_frame[i++] = OPT_HOSTNAME;
            s_frame[i++] = (uint8_t)hlen;
            memcpy(&s_frame[i], s_hostname, hlen);
            i += (uint16_t)hlen;
        }
    }

    /* Option 50: requested IP (REQUEST only) */
    if (req_ip) {
        s_frame[i++] = OPT_REQUESTED_IP;
        s_frame[i++] = 4;
        memcpy(&s_frame[i], req_ip, 4);
        i += 4;
    }

    /* Option 54: server identifier (REQUEST only) */
    if (srv_id) {
        s_frame[i++] = OPT_SERVER_ID;
        s_frame[i++] = 4;
        memcpy(&s_frame[i], srv_id, 4);
        i += 4;
    }

    /* Option 55: parameter request list (subnet, router, dns) */
    s_frame[i++] = OPT_PARAM_REQ;
    s_frame[i++] = 3;
    s_frame[i++] = OPT_SUBNET;
    s_frame[i++] = OPT_ROUTER;
    s_frame[i++] = OPT_DNS;

    /* Option 255: end */
    s_frame[i++] = OPT_END;

    return i;
}

/**
 * @brief Parse a received DHCP reply.
 *
 * @param buf     Received datagram.
 * @param len     Datagram length.
 * @param out_ip  Out: yiaddr (offered/assigned IP).
 * @param out_sn  Out: subnet (option 1), zeroed if absent.
 * @param out_gw  Out: router (option 3), zeroed if absent.
 * @param out_dns Out: DNS (option 6), zeroed if absent.
 * @param out_srv Out: server id (option 54), zeroed if absent.
 * @param lease_s Out: lease time (option 51), 0 if absent.
 * @return DHCP message type (option 53), or 0 if not a valid reply for us.
 */
static uint8_t dhcp_parse(const uint8_t *buf, int32_t len, uint8_t out_ip[4], uint8_t out_sn[4],
                          uint8_t out_gw[4], uint8_t out_dns[4], uint8_t out_srv[4],
                          uint32_t *lease_s) {
    if (len < 240)
        return 0;
    if (buf[0] != DHCP_BOOTREPLY)
        return 0;

    uint32_t xid = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) |
                   (uint32_t)buf[7];
    if (xid != s_xid)
        return 0;

    uint32_t cookie = ((uint32_t)buf[236] << 24) | ((uint32_t)buf[237] << 16) |
                      ((uint32_t)buf[238] << 8) | (uint32_t)buf[239];
    if (cookie != DHCP_MAGIC_COOKIE)
        return 0;

    memset(out_sn, 0, 4);
    memset(out_gw, 0, 4);
    memset(out_dns, 0, 4);
    memset(out_srv, 0, 4);
    *lease_s = 0;

    /* yiaddr at offset 16 */
    memcpy(out_ip, &buf[16], 4);

    uint8_t msg_type = 0;
    int32_t i = 240;
    while (i < len) {
        uint8_t code = buf[i++];
        if (code == OPT_END)
            break;
        if (code == OPT_PAD)
            continue;
        if (i >= len)
            break;
        uint8_t olen = buf[i++];
        if (i + olen > len)
            break;

        switch (code) {
        case OPT_MSG_TYPE:
            if (olen >= 1)
                msg_type = buf[i];
            break;
        case OPT_SUBNET:
            if (olen >= 4)
                memcpy(out_sn, &buf[i], 4);
            break;
        case OPT_ROUTER:
            if (olen >= 4)
                memcpy(out_gw, &buf[i], 4);
            break;
        case OPT_DNS:
            if (olen >= 4)
                memcpy(out_dns, &buf[i], 4);
            break;
        case OPT_SERVER_ID:
            if (olen >= 4)
                memcpy(out_srv, &buf[i], 4);
            break;
        case OPT_LEASE_TIME:
            if (olen >= 4)
                *lease_s = ((uint32_t)buf[i] << 24) | ((uint32_t)buf[i + 1] << 16) |
                           ((uint32_t)buf[i + 2] << 8) | (uint32_t)buf[i + 3];
            break;
        default:
            break;
        }
        i += olen;
    }

    return msg_type;
}

/**
 * @brief Wait for a DHCP reply of the expected type within a timeout.
 *
 * @return true if a matching reply was captured into the out buffers.
 */
static bool dhcp_wait_reply(uint8_t want_type, uint32_t timeout_ms, uint8_t out_ip[4],
                            uint8_t out_sn[4], uint8_t out_gw[4], uint8_t out_dns[4],
                            uint8_t out_srv[4], uint32_t *lease_s) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    uint8_t rxbuf[DHCP_FRAME_MAX];
    uint8_t from[4];
    uint16_t fport;

    for (;;) {
        dhcp_beat();
        int32_t n = recvfrom_SNMP(s_sock, rxbuf, sizeof(rxbuf), from, &fport);
        if (n > 0) {
            uint8_t t = dhcp_parse(rxbuf, n, out_ip, out_sn, out_gw, out_dns, out_srv, lease_s);
            if (t == want_type)
                return true;
            if (t == DHCPNAK)
                return false;
        }
        if ((to_ms_since_boot(get_absolute_time()) - start) >= timeout_ms)
            return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ---- Public API --------------------------------------------------------- */

void DHCP_Init(uint8_t sock, const uint8_t *mac, const char *hostname) {
    s_sock = sock;
    if (mac)
        memcpy(s_mac, mac, 6);
    /* Advertise the given host name (the unit's unique name) so the DHCP server
     * can register and display it. */
    if (hostname) {
        strncpy(s_hostname, hostname, DHCP_HOSTNAME_MAX);
        s_hostname[DHCP_HOSTNAME_MAX] = '\0';
    } else {
        s_hostname[0] = '\0';
    }
    s_bound = false;
}

void DHCP_Reset(void) { s_bound = false; }

bool DHCP_IsBound(void) { return s_bound; }

bool DHCP_Acquire(uint8_t ip[4], uint8_t sn[4], uint8_t gw[4], uint8_t dns[4], uint32_t *lease_s) {
    const uint8_t bcast[4] = {255, 255, 255, 255};
    const uint8_t zero[4] = {0, 0, 0, 0};

    if (lease_s)
        *lease_s = 0;
    s_bound = false;

    /* Clear chip addressing so broadcast works while we have no lease. */
    setSIPR((uint8_t *)zero);
    setSUBR((uint8_t *)zero);
    setGAR((uint8_t *)zero);

    /* Open the DHCP UDP socket on the client port. */
    closesocket(s_sock);
    if (socket(s_sock, Sn_MR_UDP, DHCP_CLIENT_PORT, 0) != (int8_t)s_sock) {
#if ERRORLOGGER
        uint16_t ec = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0xA);
        WARNING_PRINT_CODE(ec, "%s socket open failed\r\n", DHCP_TAG);
        Storage_EnqueueWarningCode(ec);
#endif
        return false;
    }

    for (uint8_t attempt = 0; attempt < DHCP_RETRY_MAX; attempt++) {
        dhcp_beat();
        s_xid = dhcp_make_xid();

        /* --- DISCOVER --- */
        uint16_t len = dhcp_build(DHCPDISCOVER, NULL, NULL);
        (void)sendto(s_sock, s_frame, len, (uint8_t *)bcast, DHCP_SERVER_PORT);
        INFO_PRINT("%s DISCOVER sent (try %u/%u)\r\n", DHCP_TAG, (unsigned)(attempt + 1),
                   (unsigned)DHCP_RETRY_MAX);

        uint8_t off_ip[4], off_sn[4], off_gw[4], off_dns[4], off_srv[4];
        uint32_t off_lease = 0;
        if (!dhcp_wait_reply(DHCPOFFER, DHCP_STEP_TIMEOUT_MS, off_ip, off_sn, off_gw, off_dns,
                             off_srv, &off_lease)) {
            continue; /* no offer; retry */
        }
        INFO_PRINT("%s OFFER %u.%u.%u.%u\r\n", DHCP_TAG, off_ip[0], off_ip[1], off_ip[2], off_ip[3]);

        /* --- REQUEST --- */
        len = dhcp_build(DHCPREQUEST, off_ip, off_srv);
        (void)sendto(s_sock, s_frame, len, (uint8_t *)bcast, DHCP_SERVER_PORT);

        uint8_t ack_ip[4], ack_sn[4], ack_gw[4], ack_dns[4], ack_srv[4];
        uint32_t ack_lease = 0;
        if (!dhcp_wait_reply(DHCPACK, DHCP_STEP_TIMEOUT_MS, ack_ip, ack_sn, ack_gw, ack_dns, ack_srv,
                             &ack_lease)) {
            continue; /* no ack; retry from DISCOVER */
        }

        /* --- Bound --- */
        memcpy(ip, ack_ip, 4);
        memcpy(sn, ack_sn, 4);
        memcpy(gw, ack_gw, 4);
        memcpy(dns, ack_dns, 4);
        memcpy(s_server_id, ack_srv, 4);
        if (lease_s)
            *lease_s = ack_lease;

        closesocket(s_sock);

        /* Apply leased addressing to the chip immediately. */
        setSIPR(ip);
        setSUBR(sn);
        setGAR(gw);

        s_bound = true;
        INFO_PRINT("%s BOUND ip=%u.%u.%u.%u gw=%u.%u.%u.%u lease=%lus\r\n", DHCP_TAG, ip[0], ip[1],
                   ip[2], ip[3], gw[0], gw[1], gw[2], gw[3], (unsigned long)ack_lease);
        return true;
    }

    closesocket(s_sock);
#if ERRORLOGGER
    {
        uint16_t ec = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0xB);
        WARNING_PRINT_CODE(ec, "%s no lease after %u tries\r\n", DHCP_TAG, (unsigned)DHCP_RETRY_MAX);
        Storage_EnqueueWarningCode(ec);
    }
#endif
    return false;
}
