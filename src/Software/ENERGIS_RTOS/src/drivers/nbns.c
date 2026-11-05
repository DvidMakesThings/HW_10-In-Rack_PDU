/**
 * @file src/drivers/nbns.c
 * @author DvidMakesThings - David Sipos
 *
 * @brief Minimal NetBIOS Name Service responder implementation for the W5500.
 *
 * @details
 * See nbns.h. Listens on UDP/137 for NetBIOS name queries and answers positive
 * name-query responses for our single-label name so Windows can resolve the unit
 * (e.g. "http://energis") even on network profiles where mDNS is blocked.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#include "nbns.h"
#include "socket.h"

#define NBNS_TAG "[NBNS]"
#define NBNS_PORT 137u
#define NBNS_TTL 1800u

#define NB_TYPE 0x0020u  /* NetBIOS general name service */
#define NB_CLASS 0x0001u /* Internet class */

static uint8_t s_sock = NBNS_SOCKET_NUM;
static uint8_t s_ip[4] = {0};
static uint8_t s_enc[30];   /* unique name, first-level encoded (15 chars) */
static uint8_t s_enc_g[30]; /* generic alias, first-level encoded */
static bool s_has_alias = false;

/**
 * @brief First-level encode a padded 15-char NetBIOS name (suffix excluded) into
 *        @p out (30 bytes). Each byte becomes 'A'+high nibble, 'A'+low nibble.
 */
static void nbns_encode15(const char *name, uint8_t *out) {
    char padded[15];
    size_t n = name ? strlen(name) : 0;
    for (size_t i = 0; i < 15; i++) {
        char c = (i < n) ? name[i] : ' ';
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32); /* uppercase */
        padded[i] = c;
    }
    for (int i = 0; i < 15; i++) {
        uint8_t b = (uint8_t)padded[i];
        out[i * 2] = (uint8_t)('A' + (b >> 4));
        out[i * 2 + 1] = (uint8_t)('A' + (b & 0x0F));
    }
}

bool NBNS_Init(uint8_t sock, const char *name, const char *alias, const uint8_t ip[4]) {
    s_sock = sock;
    if (ip)
        memcpy(s_ip, ip, 4);
    nbns_encode15(name, s_enc);
    s_has_alias = (alias && *alias);
    if (s_has_alias)
        nbns_encode15(alias, s_enc_g);

    closesocket(s_sock);
    if (socket(s_sock, Sn_MR_UDP, NBNS_PORT, 0) != (int8_t)s_sock) {
#if ERRORLOGGER
        uint16_t ec = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0xE);
        WARNING_PRINT_CODE(ec, "%s socket open failed\r\n", NBNS_TAG);
        Storage_EnqueueWarningCode(ec);
#endif
        return false;
    }

    INFO_PRINT("%s responder up for '%s' (+'%s') -> %u.%u.%u.%u (sock %u)\r\n", NBNS_TAG,
               name ? name : "?", s_has_alias ? alias : "-", s_ip[0], s_ip[1], s_ip[2], s_ip[3],
               (unsigned)s_sock);
    return true;
}

void NBNS_SetIP(const uint8_t ip[4]) {
    if (ip)
        memcpy(s_ip, ip, 4);
}

void NBNS_Run(void) {
    uint8_t buf[256];
    uint8_t from[4];
    uint16_t fport;

    for (uint8_t iter = 0; iter < 4; iter++) {
        int32_t n = recvfrom_SNMP(s_sock, buf, sizeof(buf), from, &fport);
        if (n < 50)
            return;

        /* Header: id(2) flags(2) qd(2) an(2) ns(2) ar(2). Must be a query. */
        uint16_t flags = ((uint16_t)buf[2] << 8) | buf[3];
        if (flags & 0x8000u) /* response bit set -> not a query */
            continue;
        uint16_t qd = ((uint16_t)buf[4] << 8) | buf[5];
        if (qd == 0)
            continue;

        /* Question name: length byte (0x20) then 32 encoded bytes then 0x00. */
        if (buf[12] != 0x20)
            continue;
        const uint8_t *qname = &buf[13];

        /* Compare the first 30 encoded bytes (the 15 name chars); ignore the
         * 2-byte suffix so any NetBIOS suffix (workstation/server) matches. Accept
         * either the unique name or the shared generic alias. */
        if (memcmp(qname, s_enc, 30) != 0 && !(s_has_alias && memcmp(qname, s_enc_g, 30) == 0))
            continue;

        if (!(s_ip[0] | s_ip[1] | s_ip[2] | s_ip[3]))
            continue;

        /* Build a positive NAME QUERY RESPONSE. */
        uint8_t pkt[64];
        uint16_t p = 0;
        pkt[p++] = buf[0]; /* transaction id (echo) */
        pkt[p++] = buf[1];
        pkt[p++] = 0x85; /* flags: response + authoritative + recursion desired */
        pkt[p++] = 0x00;
        pkt[p++] = 0x00;
        pkt[p++] = 0x00; /* QDCOUNT = 0 */
        pkt[p++] = 0x00;
        pkt[p++] = 0x01; /* ANCOUNT = 1 */
        pkt[p++] = 0x00;
        pkt[p++] = 0x00; /* NSCOUNT = 0 */
        pkt[p++] = 0x00;
        pkt[p++] = 0x00; /* ARCOUNT = 0 */

        /* RR name: echo the queried 32-byte encoded name. */
        pkt[p++] = 0x20;
        memcpy(&pkt[p], qname, 32);
        p += 32;
        pkt[p++] = 0x00;

        pkt[p++] = (uint8_t)(NB_TYPE >> 8);
        pkt[p++] = (uint8_t)(NB_TYPE & 0xFF);
        pkt[p++] = (uint8_t)(NB_CLASS >> 8);
        pkt[p++] = (uint8_t)(NB_CLASS & 0xFF);

        pkt[p++] = (uint8_t)(NBNS_TTL >> 24);
        pkt[p++] = (uint8_t)(NBNS_TTL >> 16);
        pkt[p++] = (uint8_t)(NBNS_TTL >> 8);
        pkt[p++] = (uint8_t)(NBNS_TTL & 0xFF);

        pkt[p++] = 0x00;
        pkt[p++] = 0x06; /* RDLENGTH = 6 */
        pkt[p++] = 0x00; /* NB_FLAGS: B-node, unique */
        pkt[p++] = 0x00;
        pkt[p++] = s_ip[0];
        pkt[p++] = s_ip[1];
        pkt[p++] = s_ip[2];
        pkt[p++] = s_ip[3];

        INFO_PRINT("%s query from %u.%u.%u.%u -> answering %u.%u.%u.%u\r\n", NBNS_TAG, from[0],
                   from[1], from[2], from[3], s_ip[0], s_ip[1], s_ip[2], s_ip[3]);
        (void)sendto(s_sock, pkt, p, from, fport);
        return;
    }
}
