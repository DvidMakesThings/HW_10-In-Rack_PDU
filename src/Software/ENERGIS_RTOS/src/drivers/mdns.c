/**
 * @file src/drivers/mdns.c
 * @author DvidMakesThings - David Sipos
 *
 * @brief Minimal multicast-DNS responder implementation for the W5500.
 *
 * @details
 * See mdns.h. Opens a multicast UDP socket on 224.0.0.251:5353 and replies to
 * A-record queries for "<hostname>.local" with the current device IP. Only the
 * subset of mDNS needed for name resolution is implemented (A record, cache
 * flush). Responses are sent to the multicast group; the group MAC is programmed
 * into Sn_DHAR at init and left in place so sendto() emits proper multicast.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#include "mdns.h"
#include "socket.h"

#define MDNS_TAG "[MDNS]"

#define MDNS_PORT 5353u
#define MDNS_TTL 120u
#define DNS_TYPE_A 1u
#define DNS_TYPE_ANY 255u
#define DNS_CLASS_IN 1u
#define DNS_CLASS_FLUSH 0x8001u /* IN + cache-flush bit */

/** Local setter for the destination hardware (MAC) register. */
#define setSn_DHAR(sn, mac) w5500_write_buf(Sn_DHAR(sn), (mac), 6)

static const uint8_t MDNS_MCAST_IP[4] = {224, 0, 0, 251};
static const uint8_t MDNS_MCAST_MAC[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0xFB};

static uint8_t s_sock = MDNS_SOCKET_NUM;
static uint8_t s_ip[4] = {0};

/* Two encoded FQDNs ([len]label..[5]local[0]): s_fqdn is the unique per-unit name
 * (e.g. "energis-0c77f6"), s_fqdn_g is the shared generic name ("energis"). The
 * responder answers either, which keeps "energis.local" as an easy discovery entry
 * point while unique names avoid collisions when several units share a link. */
static uint8_t s_fqdn[80];
static uint8_t s_fqdn_len = 0;
static uint8_t s_fqdn_g[80];
static uint8_t s_fqdn_g_len = 0;

/**
 * @brief Build the wire-format DNS name "<hostname>.local" into @p out.
 * @return Encoded length.
 */
static uint8_t mdns_build_fqdn(const char *hostname, uint8_t *out) {
    uint8_t i = 0;
    size_t hlen = hostname ? strlen(hostname) : 0;
    if (hlen > 63)
        hlen = 63;

    out[i++] = (uint8_t)hlen;
    for (size_t k = 0; k < hlen; k++)
        out[i++] = (uint8_t)hostname[k];

    out[i++] = 5;
    out[i++] = 'l';
    out[i++] = 'o';
    out[i++] = 'c';
    out[i++] = 'a';
    out[i++] = 'l';
    out[i++] = 0; /* root */
    return i;
}

static inline char mdns_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/**
 * @brief Compare a question name in @p buf at @p off against @p fqdn.
 *
 * Does not follow DNS compression pointers (mDNS queries carry full names).
 *
 * @param buf      Datagram.
 * @param len      Datagram length.
 * @param off      Offset of the question name.
 * @param fqdn     Encoded FQDN to compare against.
 * @param fqdn_len Length of @p fqdn.
 * @param name_end Out: offset just past the name on match.
 * @return true if the name matches (case-insensitive).
 */
static bool mdns_name_matches(const uint8_t *buf, int32_t len, int32_t off, const uint8_t *fqdn,
                              uint8_t fqdn_len, int32_t *name_end) {
    int32_t i = off;
    uint8_t j = 0;

    while (i < len) {
        uint8_t l = buf[i];
        if (l == 0) {
            i++;
            if (j != fqdn_len - 1) /* our encoding ends with the root byte too */
                return false;
            *name_end = i;
            return true;
        }
        if ((l & 0xC0) != 0) /* compression pointer: not expected here */
            return false;
        if (j >= fqdn_len)
            return false;
        if (fqdn[j] != l)
            return false;
        i++;
        j++;
        for (uint8_t k = 0; k < l; k++) {
            if (i >= len || j >= fqdn_len)
                return false;
            if (mdns_lower((char)buf[i]) != mdns_lower((char)fqdn[j]))
                return false;
            i++;
            j++;
        }
    }
    return false;
}
/**
 * @brief Build an A-record response packet for our name/IP into @p pkt.
 * @return Packet length in bytes.
 */
static uint16_t mdns_build_answer(uint8_t *pkt, const uint8_t *fqdn, uint8_t fqdn_len) {
    uint16_t p = 0;

    /* Header: id=0, flags=0x8400 (response + authoritative), AN=1 */
    pkt[p++] = 0x00;
    pkt[p++] = 0x00;
    pkt[p++] = 0x84;
    pkt[p++] = 0x00;
    pkt[p++] = 0x00;
    pkt[p++] = 0x00; /* QDCOUNT = 0 */
    pkt[p++] = 0x00;
    pkt[p++] = 0x01; /* ANCOUNT = 1 */
    pkt[p++] = 0x00;
    pkt[p++] = 0x00; /* NSCOUNT = 0 */
    pkt[p++] = 0x00;
    pkt[p++] = 0x00; /* ARCOUNT = 0 */

    /* Answer name */
    memcpy(&pkt[p], fqdn, fqdn_len);
    p += fqdn_len;

    /* TYPE A, CLASS IN + flush */
    pkt[p++] = (uint8_t)(DNS_TYPE_A >> 8);
    pkt[p++] = (uint8_t)(DNS_TYPE_A & 0xFF);
    pkt[p++] = (uint8_t)(DNS_CLASS_FLUSH >> 8);
    pkt[p++] = (uint8_t)(DNS_CLASS_FLUSH & 0xFF);

    /* TTL */
    pkt[p++] = (uint8_t)(MDNS_TTL >> 24);
    pkt[p++] = (uint8_t)(MDNS_TTL >> 16);
    pkt[p++] = (uint8_t)(MDNS_TTL >> 8);
    pkt[p++] = (uint8_t)(MDNS_TTL & 0xFF);

    /* RDLENGTH + RDATA (IPv4) */
    pkt[p++] = 0x00;
    pkt[p++] = 0x04;
    pkt[p++] = s_ip[0];
    pkt[p++] = s_ip[1];
    pkt[p++] = s_ip[2];
    pkt[p++] = s_ip[3];

    return p;
}

/**
 * @brief Multicast an A-record answer for one of our names.
 */
static void mdns_announce_name(const uint8_t *fqdn, uint8_t fqdn_len) {
    uint8_t pkt[128];
    uint16_t p = mdns_build_answer(pkt, fqdn, fqdn_len);

    /* Re-assert the multicast destination (DIPR/DHAR/DPORT) so the W5500 emits a
     * proper multicast frame using the group MAC instead of attempting ARP. */
    setSn_DIPR(s_sock, (uint8_t *)MDNS_MCAST_IP);
    setSn_DHAR(s_sock, (uint8_t *)MDNS_MCAST_MAC);
    setSn_DPORT(s_sock, MDNS_PORT);

    (void)sendto(s_sock, pkt, p, (uint8_t *)MDNS_MCAST_IP, MDNS_PORT);
}

/**
 * @brief Build and multicast an A-record answer for our (unique) name/IP.
 */
void mDNS_Announce(void) { mdns_announce_name(s_fqdn, s_fqdn_len); }

bool mDNS_Init(uint8_t sock, const char *hostname, const char *alias, const uint8_t ip[4]) {
    s_sock = sock;
    if (ip)
        memcpy(s_ip, ip, 4);
    s_fqdn_len = mdns_build_fqdn(hostname, s_fqdn);
    s_fqdn_g_len = (alias && *alias) ? mdns_build_fqdn(alias, s_fqdn_g) : 0;

    /* Program multicast group destination before opening in MULTI mode. */
    closesocket(s_sock);
    setSn_DIPR(s_sock, (uint8_t *)MDNS_MCAST_IP);
    setSn_DHAR(s_sock, (uint8_t *)MDNS_MCAST_MAC);
    setSn_DPORT(s_sock, MDNS_PORT);

    if (socket(s_sock, Sn_MR_UDP, MDNS_PORT, SF_MULTI_ENABLE) != (int8_t)s_sock) {
#if ERRORLOGGER
        uint16_t ec = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0xC);
        WARNING_PRINT_CODE(ec, "%s multicast socket open failed\r\n", MDNS_TAG);
        Storage_EnqueueWarningCode(ec);
#endif
        return false;
    }

    INFO_PRINT("%s responder up for %s.local (+%s.local) -> %u.%u.%u.%u (sock %u)\r\n", MDNS_TAG,
               hostname ? hostname : "?", (alias && *alias) ? alias : "-", s_ip[0], s_ip[1], s_ip[2],
               s_ip[3], (unsigned)s_sock);
    if (s_ip[0] | s_ip[1] | s_ip[2] | s_ip[3])
        mDNS_Announce();
    return true;
}

void mDNS_SetIP(const uint8_t ip[4]) {
    if (!ip)
        return;
    if (memcmp(s_ip, ip, 4) == 0)
        return;
    memcpy(s_ip, ip, 4);
    if (s_ip[0] | s_ip[1] | s_ip[2] | s_ip[3])
        mDNS_Announce();
}

void mDNS_Run(void) {
    uint8_t buf[512];
    uint8_t from[4];
    uint16_t fport;

    /* Drain several datagrams per call; the link can carry heavy mDNS traffic. */
    for (uint8_t iter = 0; iter < 6; iter++) {
        int32_t n = recvfrom_SNMP(s_sock, buf, sizeof(buf), from, &fport);
        if (n < 12)
            return;

        /* Must be a query (QR=0) with a sane question count. */
        uint16_t flags = ((uint16_t)buf[2] << 8) | buf[3];
        if (flags & 0x8000u)
            continue;
        uint16_t qd = ((uint16_t)buf[4] << 8) | buf[5];
        if (qd == 0 || qd > 50)
            continue;

        int32_t off = 12;
        for (uint16_t q = 0; q < qd && off < n; q++) {
            int32_t name_end = 0;
            const uint8_t *match_fqdn = NULL;
            uint8_t match_len = 0;
            if (mdns_name_matches(buf, n, off, s_fqdn, s_fqdn_len, &name_end)) {
                match_fqdn = s_fqdn;
                match_len = s_fqdn_len;
            } else if (s_fqdn_g_len &&
                       mdns_name_matches(buf, n, off, s_fqdn_g, s_fqdn_g_len, &name_end)) {
                match_fqdn = s_fqdn_g;
                match_len = s_fqdn_g_len;
            }

            if (match_fqdn && name_end + 4 <= n) {
                uint16_t qtype = ((uint16_t)buf[name_end] << 8) | buf[name_end + 1];
                if (qtype == DNS_TYPE_A || qtype == DNS_TYPE_ANY) {
                    INFO_PRINT("%s query from %u.%u.%u.%u -> answering %u.%u.%u.%u\r\n", MDNS_TAG,
                               from[0], from[1], from[2], from[3], s_ip[0], s_ip[1], s_ip[2],
                               s_ip[3]);
                    if (s_ip[0] | s_ip[1] | s_ip[2] | s_ip[3]) {
                        /* Answer with the exact name that was queried. */
                        mdns_announce_name(match_fqdn, match_len);
                    }
                    return;
                }
            }

            /* Skip this question (name + qtype(2) + qclass(2)) to reach the next. */
            if (match_fqdn) {
                off = name_end + 4;
            } else {
                while (off < n && buf[off] != 0) {
                    if ((buf[off] & 0xC0) != 0) { /* compression pointer */
                        off += 2;
                        goto skip_fixed;
                    }
                    off += buf[off] + 1;
                }
                off += 1; /* root label */
            skip_fixed:
                off += 4; /* qtype + qclass */
            }
        }
    }
}
