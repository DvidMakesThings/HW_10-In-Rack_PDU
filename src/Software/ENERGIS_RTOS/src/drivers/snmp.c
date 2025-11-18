/**
 * @file src/drivers/snmp.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details  SNMP agent for ENERGIS RTOS. Owns a UDP socket (port 161),
 * parses incoming PDUs and emits responses without blocking the scheduler.
 * Time base is 10 ms via SNMP_Tick10ms(), used for TimeTicks.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ------------------------------------------------------------------------------------------------
 *  Internal state
 * --------------------------------------------------------------------------------------------- */

static snmp_msg_t s_req;
static snmp_msg_t s_resp;

static volatile uint32_t s_tick10ms = 0;

static uint8_t s_sock_agent = 1;
static uint8_t s_sock_trap = 2;
static uint16_t s_local_port = SNMP_PORT_AGENT;

static uint8_t s_trap_buf[SNMP_MAX_TRAP];

static uint8_t s_errorStatus = 0;
static uint8_t s_errorIndex = 0;

static uint32_t s_startTick10ms = 0;

/* ------------------------------------------------------------------------------------------------
 *  Forward declarations
 * --------------------------------------------------------------------------------------------- */

static int32_t findEntry(const uint8_t *oid, int32_t len);
static int32_t getOID(int32_t id, uint8_t *oid, uint8_t *len);
static int32_t getValue(const uint8_t *vptr, int32_t vlen);
static int32_t getEntry(int32_t id, uint8_t *dataType, void *ptr, int32_t *len);
static int32_t setEntry(int32_t id, const void *val, int32_t vlen, uint8_t dataType, int32_t index);

static int32_t parseSNMPMessage(void);
static int32_t parseVersion(void);
static int32_t parseCommunity(void);
static int32_t parseRequest(void);
static int32_t parseSequenceOf(int32_t reqType);
static int32_t parseSequence(int32_t reqType, int32_t index);
static int32_t parseVarBind(int32_t reqType, int32_t index);

static int32_t parseLength(const uint8_t *msg, int32_t *len);
static int32_t parseTLV(const uint8_t *msg, int32_t index, snmp_tlv_t *tlv);
static void insertRespLen(int32_t reqStart, int32_t respStart, int32_t size);

static void ipToByteArray(const int8_t *ip, uint8_t *pDes);

#if _SNMP_DEBUG_
static void dumpCode(const char *hdr, const char *tail, const uint8_t *buf, int32_t len);
#endif

/* ------------------------------------------------------------------------------------------------
 *  Public API
 * --------------------------------------------------------------------------------------------- */

bool SNMP_Init(uint8_t sock_agent, uint16_t local_port, uint8_t sock_trap) {
    s_sock_agent = sock_agent;
    s_sock_trap = sock_trap;
    s_local_port = (local_port == 0) ? SNMP_PORT_AGENT : local_port;

    /* Prepare OID table and base timing */
    s_tick10ms = 0;
    s_startTick10ms = SNMP_GetTick10ms();
    initTable();

    /* Bind UDP/161 */
    closesocket(s_sock_agent);
    if (socket(s_sock_agent, Sn_MR_UDP, s_local_port, 0) != s_sock_agent) {
        ERROR_PRINT("[SNMP] failed to open UDP socket %u:%u\r\n", (unsigned)s_sock_agent,
                    (unsigned)s_local_port);
        return false;
    }

    /* Optional: initial trap setup with our current IP */
    uint8_t agent_ip[4] = {0};
    getSIPR(agent_ip);
    uint8_t mgr_ip[4] = {0, 0, 0, 0};
    initial_Trap(mgr_ip, agent_ip);

    INFO_PRINT("[SNMP] ready on UDP socket %u, port %u\r\n", (unsigned)s_sock_agent,
               (unsigned)s_local_port);
    return true;
}

/**
 * @brief Poll and service pending SNMP packets on the agent UDP socket.
 *
 * Non-blocking service routine that processes up to @p max_packets SNMPv1 requests
 * from the UDP socket bound to port 161, and emits corresponding GetResponse PDUs.
 * This variant relies on the W5500-style datagram reader @ref recvfrom_SNMP(),
 * which already consumes the 8-byte UDP datagram header (src IP[4], src port[2],
 * payload length[2]) and returns the pure ASN.1 payload starting with 0x30.
 *
 * Robustness:
 *  - If no data is pending or the bounded wait in @ref recvfrom_SNMP() elapses,
 *    the function returns early without blocking the caller.
 *  - Source address and port are taken directly from @ref recvfrom_SNMP() and
 *    used for the reply via @ref sendto().
 *
 * @param[in] max_packets Maximum number of datagrams to process in this call (>=1).
 *
 * @return int Number of serviced packets in this call (0..max_packets).
 */
int SNMP_Poll(int max_packets) {
    if (max_packets <= 0) {
        max_packets = 1;
    }

    int serviced = 0;

    while (serviced < max_packets) {
        /* Fast exit if socket RX has nothing queued */
        uint16_t rsr = getSn_RX_RSR(s_sock_agent);
        if (rsr == 0) {
            break;
        }

        uint8_t src_addr[6] = {0}; /* recvfrom_SNMP fills IPv4 in first 4 bytes */
        uint16_t src_port = 0;

        /* Read one full UDP datagram's payload (ASN.1 starts at buf[0]) */
        int rlen = recvfrom_SNMP(s_sock_agent, s_req.buffer,
                                 (rsr > SNMP_MAX_MSG) ? SNMP_MAX_MSG : rsr, src_addr, &src_port);

        if (rlen <= 0) {
            /* 0 or SOCK_BUSY/negative: nothing usable right now */
            break;
        }

        s_req.len = (uint16_t)rlen;
        s_req.index = 0;
        s_resp.index = 0;
        s_errorStatus = 0;
        s_errorIndex = 0;
        memset(s_resp.buffer, 0, sizeof(s_resp.buffer));

#if _SNMP_DEBUG_
        dumpCode("[SNMP RX]\r\n", "\r\n", s_req.buffer, s_req.len);
#endif

        if (parseSNMPMessage() != -1) {
            (void)sendto(s_sock_agent, s_resp.buffer, (uint16_t)s_resp.index, src_addr, src_port);
#if _SNMP_DEBUG_
            dumpCode("[SNMP TX]\r\n", "\r\n", s_resp.buffer, s_resp.index);
#endif
        }

        serviced++;
    }

    return serviced;
}

void SNMP_Tick10ms(void) { s_tick10ms++; }
uint32_t SNMP_GetTick10ms(void) { return s_tick10ms; }

void SNMP_GetUptime(void *ptr, uint8_t *len) {
    uint32_t now = SNMP_GetTick10ms();
    *(uint32_t *)ptr = (now - s_startTick10ms); /* TimeTicks in 10 ms. */
    *len = 4;
}

int32_t SNMP_SendTrap(uint8_t *managerIP, uint8_t *agentIP, int8_t *community,
                      snmp_entry_t enterprise_oid, uint32_t genericTrap, uint32_t specificTrap,
                      uint32_t va_count, ...) {
    int32_t idx = 0;
    int32_t pdu_len_pos = 0;
    int32_t trap_len_pos = 0;
    int32_t vbs_len_pos = 0;

    uint32_t vb_total = 0;

    /* ASN.1 Sequence (outer) */
    s_trap_buf[idx++] = 0x30;
    pdu_len_pos = idx++; /* filled later */

    /* version = 0 */
    s_trap_buf[idx++] = 0x02;
    s_trap_buf[idx++] = 0x01;
    s_trap_buf[idx++] = 0x00;

    /* community */
    size_t comm_len = strlen((const char *)community);
    s_trap_buf[idx++] = 0x04;
    s_trap_buf[idx++] = (uint8_t)comm_len;
    memcpy(&s_trap_buf[idx], community, comm_len);
    idx += (int32_t)comm_len;

    /* Trap-PDU (0xA4) */
    s_trap_buf[idx++] = 0xA4;
    trap_len_pos = idx++;

    /* enterprise OID */
    s_trap_buf[idx++] = 0x06;
    s_trap_buf[idx++] = enterprise_oid.oidlen;
    memcpy(&s_trap_buf[idx], enterprise_oid.oid, enterprise_oid.oidlen);
    idx += enterprise_oid.oidlen;

    /* agent IP (IpAddress) */
    s_trap_buf[idx++] = 0x40;
    s_trap_buf[idx++] = 0x04;
    s_trap_buf[idx++] = agentIP[0];
    s_trap_buf[idx++] = agentIP[1];
    s_trap_buf[idx++] = agentIP[2];
    s_trap_buf[idx++] = agentIP[3];

    /* generic trap */
    s_trap_buf[idx++] = 0x02;
    s_trap_buf[idx++] = 0x01;
    s_trap_buf[idx++] = (uint8_t)genericTrap;

    /* specific trap */
    s_trap_buf[idx++] = 0x02;
    s_trap_buf[idx++] = 0x01;
    s_trap_buf[idx++] = (uint8_t)specificTrap;

    /* timestamp (TimeTicks, 1 byte for simplicity here) */
    s_trap_buf[idx++] = 0x43;
    s_trap_buf[idx++] = 0x01;
    s_trap_buf[idx++] = 0x00;

    /* VarBind list */
    s_trap_buf[idx++] = 0x30;
    vbs_len_pos = idx++;

    /* Var-binds via varargs */
    va_list ap;
    va_start(ap, va_count);
    for (uint32_t i = 0; i < va_count; i++) {
        snmp_entry_t *e = va_arg(ap, snmp_entry_t *);
        uint8_t *p = &s_trap_buf[idx];

        *p++ = 0x30; /* VarBind SEQUENCE */
        *p++ = 0xFF; /* placeholder for VB length */
        int vb_len_pos_local = 1;

        /* name (OID) */
        *p++ = 0x06;
        *p++ = e->oidlen;
        memcpy(p, e->oid, e->oidlen);
        p += e->oidlen;

        /* value */
        if (e->dataType == SNMPDTYPE_OCTET_STRING || e->dataType == SNMPDTYPE_OBJ_ID) {
            uint8_t dlen = e->dataLen;
            if (e->dataType == SNMPDTYPE_OCTET_STRING)
                dlen = (uint8_t)strlen((const char *)e->u.octetstring);
            *p++ = e->dataType;
            *p++ = dlen;
            memcpy(p, e->u.octetstring, dlen);
            p += dlen;
        } else {
            *p++ = e->dataType;
            *p++ = 4;
            uint32_t v = HTONL(e->u.intval);
            memcpy(p, &v, 4);
            p += 4;
        }

        int vb_len = (int)(p - (&s_trap_buf[idx] + 2));
        s_trap_buf[idx + vb_len_pos_local] = (uint8_t)vb_len;
        idx += 2 + vb_len;
        vb_total += (uint32_t)(2 + vb_len);
    }
    va_end(ap);

    s_trap_buf[vbs_len_pos] = (uint8_t)vb_total;
    s_trap_buf[trap_len_pos] = (uint8_t)((idx - trap_len_pos - 1));
    s_trap_buf[pdu_len_pos] = (uint8_t)((idx - pdu_len_pos - 1));

    /* Send trap via a transient UDP/162 socket */
    closesocket(s_sock_trap);
    (void)socket(s_sock_trap, Sn_MR_UDP, SNMP_PORT_TRAP, 0);
    (void)sendto(s_sock_trap, s_trap_buf, (uint16_t)idx, managerIP, SNMP_PORT_TRAP);
    closesocket(s_sock_trap);

    return 0;
}

/* ------------------------------------------------------------------------------------------------
 *  Core implementation
 * --------------------------------------------------------------------------------------------- */

static int32_t findEntry(const uint8_t *oid, int32_t len) {
    for (int32_t i = 0; i < maxData; i++) {
        if (len == snmpData[i].oidlen && memcmp(snmpData[i].oid, oid, len) == 0)
            return i;
    }
    return OID_NOT_FOUND;
}

static int32_t getOID(int32_t id, uint8_t *oid, uint8_t *len) {
    if (!(id >= 0 && id < maxData))
        return INVALID_ENTRY_ID;
    *len = snmpData[id].oidlen;
    for (uint8_t j = 0; j < *len; j++)
        oid[j] = snmpData[id].oid[j];
    return SNMP_SUCCESS;
}

static int32_t getValue(const uint8_t *vptr, int32_t vlen) {
    int32_t value = 0;
    for (int32_t i = 0; i < vlen; i++)
        value = (value << 8) | vptr[i];
    return value;
}

static int32_t getEntry(int32_t id, uint8_t *dataType, void *ptr, int32_t *len) {
    if (!(id >= 0 && id < maxData))
        return INVALID_ENTRY_ID;
    *dataType = snmpData[id].dataType;

    switch (*dataType) {
    case SNMPDTYPE_OCTET_STRING:
    case SNMPDTYPE_OBJ_ID: {
        if (snmpData[id].getfunction)
            snmpData[id].getfunction(&snmpData[id].u.octetstring, &snmpData[id].dataLen);

        if (*dataType == SNMPDTYPE_OCTET_STRING)
            snmpData[id].dataLen = (uint8_t)strlen((const char *)snmpData[id].u.octetstring);

        *len = snmpData[id].dataLen;
        memcpy(ptr, snmpData[id].u.octetstring, (size_t)*len);
    } break;

    case SNMPDTYPE_INTEGER:
    case SNMPDTYPE_TIME_TICKS:
    case SNMPDTYPE_COUNTER:
    case SNMPDTYPE_GAUGE: {
        if (snmpData[id].getfunction)
            snmpData[id].getfunction(&snmpData[id].u.intval, &snmpData[id].dataLen);

        *len = snmpData[id].dataLen ? snmpData[id].dataLen : 4;
        uint8_t *p = (uint8_t *)ptr;
        uint32_t v = snmpData[id].u.intval;
        for (int32_t j = 0; j < *len; j++)
            p[j] = (uint8_t)((v >> ((*len - j - 1) * 8)) & 0xFF);
    } break;

    default:
        return INVALID_DATA_TYPE;
    }
    return SNMP_SUCCESS;
}

static int32_t setEntry(int32_t id, const void *val, int32_t vlen, uint8_t dataType,
                        int32_t index) {
    if (snmpData[id].dataType != dataType) {
        s_errorStatus = BAD_VALUE;
        s_errorIndex = index;
        return INVALID_DATA_TYPE;
    }

    switch (snmpData[id].dataType) {
    case SNMPDTYPE_OCTET_STRING:
    case SNMPDTYPE_OBJ_ID: {
        memcpy(snmpData[id].u.octetstring, val, (size_t)vlen);
        snmpData[id].dataLen = (uint8_t)vlen;
    } break;

    case SNMPDTYPE_INTEGER:
    case SNMPDTYPE_TIME_TICKS:
    case SNMPDTYPE_COUNTER:
    case SNMPDTYPE_GAUGE: {
        snmpData[id].u.intval = (uint32_t)getValue((const uint8_t *)val, vlen);
        snmpData[id].dataLen = (uint8_t)vlen;
        if (snmpData[id].setfunction)
            snmpData[id].setfunction((int32_t)snmpData[id].u.intval);
    } break;

    default:
        return INVALID_DATA_TYPE;
    }
    return SNMP_SUCCESS;
}

/* --- TLV helpers --------------------------------------------------------------- */

static int32_t parseLength(const uint8_t *msg, int32_t *len) {
    int32_t i = 1;
    if (msg[0] & 0x80) {
        int32_t tlen = (msg[0] & 0x7F) - 1;
        *len = msg[i++];
        while (tlen--) {
            *len = (*len << 8) | msg[i++];
        }
    } else {
        *len = msg[0];
    }
    return i;
}

static int32_t parseTLV(const uint8_t *msg, int32_t index, snmp_tlv_t *tlv) {
    tlv->start = index;
    int32_t Llen = parseLength(&msg[index + 1], &tlv->len);
    tlv->vstart = index + Llen + 1;

    switch (msg[index]) {
    case SNMPDTYPE_SEQUENCE:
    case GET_REQUEST:
    case GET_NEXT_REQUEST:
    case SET_REQUEST:
        tlv->nstart = tlv->vstart;
        break;
    default:
        tlv->nstart = tlv->vstart + tlv->len;
        break;
    }
    return 0;
}

static void insertRespLen(int32_t reqStart, int32_t respStart, int32_t size) {
    /* Fill the length bytes of the response at respStart to match 'size' of content */
    if (s_req.buffer[reqStart + 1] & 0x80) {
        int32_t lenLength = s_req.buffer[reqStart + 1] & 0x7F;
        int32_t indexStart = respStart + 2;
        for (int32_t i = 0; i < lenLength; i++) {
            int32_t shift = 8 * (lenLength - 1 - i);
            s_resp.buffer[indexStart + i] = (uint8_t)((size >> shift) & 0xFF);
        }
    } else {
        s_resp.buffer[respStart + 1] = (uint8_t)(size & 0xFF);
    }
}

/* --- VarBind / Sequences -------------------------------------------------------------------- */

static int32_t parseVarBind(int32_t reqType, int32_t index) {
    int32_t seglen = 0, id, size = 0;
    snmp_tlv_t name, value;

    parseTLV(s_req.buffer, s_req.index, &name);
    if (s_req.buffer[name.start] != SNMPDTYPE_OBJ_ID)
        return -1;

    id = findEntry(&s_req.buffer[name.vstart], name.len);

    if (reqType == GET_REQUEST || reqType == SET_REQUEST) {
        seglen = name.nstart - name.start;
        memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[name.start], seglen);
        s_req.index += seglen;
        s_resp.index += seglen;
        size += seglen;
    } else if (reqType == GET_NEXT_REQUEST) {
        s_resp.buffer[s_resp.index] = s_req.buffer[name.start];
        if (++id >= maxData) {
            id = OID_NOT_FOUND;
            seglen = name.nstart - name.start;
            memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[name.start], seglen);
            s_req.index += seglen;
            s_resp.index += seglen;
            size += seglen;
        } else {
            s_req.index += name.nstart - name.start;
            s_resp.buffer[s_resp.index + 1] = 0; /* len will be set by getOID */
            getOID(id, &s_resp.buffer[s_resp.index + 2], &s_resp.buffer[s_resp.index + 1]);
            seglen = s_resp.buffer[s_resp.index + 1] + 2;
            s_resp.index += seglen;
            size += seglen;
        }
    }

    parseTLV(s_req.buffer, s_req.index, &value);

    if (id != OID_NOT_FOUND) {
        uint8_t dtype;
        int32_t vlen;

        if (reqType == GET_REQUEST || reqType == GET_NEXT_REQUEST) {
            getEntry(id, &dtype, &s_resp.buffer[s_resp.index + 2], &vlen);

            s_resp.buffer[s_resp.index] = dtype;
            s_resp.buffer[s_resp.index + 1] = (uint8_t)vlen;
            seglen = 2 + vlen;

            s_resp.index += seglen;
            s_req.index += (value.nstart - value.start);

        } else { /* SET_REQUEST */
            setEntry(id, &s_req.buffer[value.vstart], value.len, s_req.buffer[value.start], index);

            seglen = value.nstart - value.start;
            memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[value.start], seglen);
            s_resp.index += seglen;
            s_req.index += seglen;
        }
    } else {
        seglen = value.nstart - value.start;
        memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[value.start], seglen);
        s_resp.index += seglen;
        s_req.index += seglen;

        s_errorIndex = (uint8_t)index;
        s_errorStatus = NO_SUCH_NAME;
    }

    size += seglen;
    return size;
}

/* --- VarBind SEQUENCE --------------------------------------------------- */
static int32_t parseSequence(int32_t reqType, int32_t index) {
    int32_t seglen, respLoc;
    snmp_tlv_t seq;

    parseTLV(s_req.buffer, s_req.index, &seq);
    if (s_req.buffer[seq.start] != SNMPDTYPE_SEQUENCE)
        return -1;

    /* Copy SEQUENCE header (tag+len); will fill its length after composing content */
    seglen = seq.vstart - seq.start;
    respLoc = s_resp.index;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[seq.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;

    /* Build inner content and set SEQUENCE length to content only */
    int32_t content = parseVarBind(reqType, index);
    insertRespLen(seq.start, respLoc, content);

    /* Return total bytes emitted for this SEQUENCE (header + content) */
    return content + seglen;
}

/* --- VarBindList ------------------------------------------ */
static int32_t parseSequenceOf(int32_t reqType) {
    int32_t seglen, respLoc;
    snmp_tlv_t seqof;

    parseTLV(s_req.buffer, s_req.index, &seqof);
    if (s_req.buffer[seqof.start] != SNMPDTYPE_SEQUENCE_OF)
        return -1;

    /* Copy SEQUENCE OF header; fill length after composing all inner SEQUENCEs */
    seglen = seqof.vstart - seqof.start;
    respLoc = s_resp.index;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[seqof.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;

    /* Inner content = sum of child SEQUENCEs (their returns already include their own headers) */
    int32_t content = 0;
    int32_t idx = 0;
    while (s_req.index < s_req.len) {
        int32_t one = parseSequence(reqType, idx++);
        if (one < 0)
            return -1;
        content += one;
    }

    /* Fix SEQUENCE OF length to content only */
    insertRespLen(seqof.start, respLoc, content);

    /* Return total bytes (header + content) */
    return content + seglen;
}

/* --- PDU (GetRequest/SetRequest/GetNext) ------------------------------- */
static int32_t parseRequest(void) {
    int32_t seglen, respLoc, reqType;
    snmp_tlv_t snmpreq, requestid, errStatus, errIndex;

    parseTLV(s_req.buffer, s_req.index, &snmpreq);
    reqType = s_req.buffer[snmpreq.start];
    if (!VALID_REQUEST(reqType))
        return -1;

    /* Copy PDU header; we’ll set its length to content later */
    seglen = snmpreq.vstart - snmpreq.start;
    respLoc = s_resp.index;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[snmpreq.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;

    /* Flip tag to GetResponse */
    s_resp.buffer[snmpreq.start] = GET_RESPONSE;

    /* Build PDU content and track only content length (exclude header) */
    int32_t content = 0;

    parseTLV(s_req.buffer, s_req.index, &requestid);
    seglen = requestid.nstart - requestid.start;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[requestid.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;
    content += seglen;

    parseTLV(s_req.buffer, s_req.index, &errStatus);
    seglen = errStatus.nstart - errStatus.start;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[errStatus.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;
    content += seglen;

    parseTLV(s_req.buffer, s_req.index, &errIndex);
    seglen = errIndex.nstart - errIndex.start;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[errIndex.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;
    content += seglen;

    /* VarBindList */
    int32_t r = parseSequenceOf(reqType);
    if (r < 0)
        return -1;
    content += r;

    /* Fix PDU length to content only */
    insertRespLen(snmpreq.start, respLoc, content);

    /* Apply error if set */
    if (s_errorStatus) {
        s_resp.buffer[errStatus.vstart] = s_errorStatus;
        s_resp.buffer[errIndex.vstart] = s_errorIndex + 1;
    }

    /* Return total bytes (header + content) */
    return content + (snmpreq.vstart - snmpreq.start);
}

static int32_t parseCommunity(void) {
    int32_t seglen, size = 0;
    snmp_tlv_t community;

    parseTLV(s_req.buffer, s_req.index, &community);
    if (!(s_req.buffer[community.start] == SNMPDTYPE_OCTET_STRING &&
          community.len == COMMUNITY_SIZE))
        return -1;

    if (memcmp(&s_req.buffer[community.vstart], COMMUNITY, COMMUNITY_SIZE) == 0) {
        seglen = community.nstart - community.start;
        memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[community.start], seglen);
        s_req.index += seglen;
        s_resp.index += seglen;
        size += seglen;

        int32_t r = parseRequest();
        if (r < 0)
            return -1;
        size += r;
    } else {
        return -1;
    }
    return size;
}

static int32_t parseVersion(void) {
    int32_t seglen, size = 0;
    snmp_tlv_t tlv;

    parseTLV(s_req.buffer, s_req.index, &tlv);
    if (!(s_req.buffer[tlv.start] == SNMPDTYPE_INTEGER && s_req.buffer[tlv.vstart] == SNMP_V1))
        return -1;

    seglen = tlv.nstart - tlv.start;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[tlv.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;
    size += seglen;

    int32_t r = parseCommunity();
    if (r < 0)
        return -1;
    return size + r; /* bytes appended after version TLV */
}

static int32_t parseSNMPMessage(void) {
    snmp_tlv_t tlv;

    s_req.index = 0;
    s_resp.index = 0;

    parseTLV(s_req.buffer, s_req.index, &tlv);
    if (s_req.buffer[tlv.start] != SNMPDTYPE_SEQUENCE_OF)
        return -1;

    /* Copy message header; fix length after composing content */
    int32_t seglen = tlv.vstart - tlv.start;
    int32_t respLoc = s_resp.index;
    memcpy(&s_resp.buffer[s_resp.index], &s_req.buffer[tlv.start], seglen);
    s_req.index += seglen;
    s_resp.index += seglen;

    /* Build content (version + community + PDU) */
    int32_t r = parseVersion();
    if (r < 0)
        return -1;

    /* Set outer length to content only */
    int32_t content_len = (int32_t)(s_resp.index - tlv.vstart);
    insertRespLen(tlv.start, respLoc, content_len);

    return 0;
}

/* ------------------------------------------------------------------------------------------------
 *  Utilities
 * --------------------------------------------------------------------------------------------- */

static void ipToByteArray(const int8_t *ip, uint8_t *pDes) {
    unsigned u1 = 0, u2 = 0, u3 = 0, u4 = 0;
    sscanf((const char *)ip, "%u.%u.%u.%u", &u1, &u2, &u3, &u4);
    pDes[0] = (uint8_t)u1;
    pDes[1] = (uint8_t)u2;
    pDes[2] = (uint8_t)u3;
    pDes[3] = (uint8_t)u4;
}

#if _SNMP_DEBUG_
static void dumpCode(const char *hdr, const char *tail, const uint8_t *buf, int32_t len) {
    log_printf("%s", hdr);
    for (int i = 0; i < len; i++) {
        if ((i % 16) == 0)
            log_printf("0x%04x : ", i);
        log_printf("%02X ", buf[i]);
        if ((i % 16) == 15) {
            log_printf("  ");
            for (int j = i - 15; j <= i; j++)
                log_printf("%c", isprint(buf[j]) ? buf[j] : '.');
            log_printf("\r\n");
        }
    }
    if (len % 16) {
        int rem = 16 - (len % 16);
        for (int s = 0; s < rem * 3 + 2; s++)
            log_printf(" ");
        for (int j = len - (len % 16); j < len; j++)
            log_printf("%c", isprint(buf[j]) ? buf[j] : '.');
        log_printf("\r\n");
    }
    log_printf("%s", tail);
}
#endif
