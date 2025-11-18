/**
 * @file src/drivers/snmp.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers07 7. SNMP Agent Implementation
 * @ingroup drivers
 * @brief Header file for non-blocking SNMPv1 agent with UDP transport
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 * đ
 * @details  SNMP agent for ENERGIS RTOS. Owns a UDP socket (port 161),
 * parses incoming PDUs and emits responses without blocking the scheduler.
 * Time base is 10 ms via SNMP_Tick10ms(), used for TimeTicks.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */
#ifndef ENERGIS_SNMP_H
#define ENERGIS_SNMP_H

/* ------------------------------------------------------------------------------------------------
 *  Build-time options
 * --------------------------------------------------------------------------------------------- */

/** @brief Enable hexdumps of requests/responses on the UART console. */
#ifndef _SNMP_DEBUG_
#define _SNMP_DEBUG_ 1
#endif

/** @brief Maximum OID components per entry. */
#ifndef SNMP_MAX_OID
#define SNMP_MAX_OID 12
#endif

/** @brief Maximum string size for OCTET STRING values. */
#ifndef SNMP_MAX_STRING
#define SNMP_MAX_STRING 64
#endif

/** @brief Maximum SNMP PDU size supported. */
#ifndef SNMP_MAX_MSG
#define SNMP_MAX_MSG 768
#endif

/** @brief Maximum SNMP TRAP PDU size. */
#ifndef SNMP_MAX_TRAP
#define SNMP_MAX_TRAP 512
#endif

/** @brief Default SNMP agent and trap ports. */
#define SNMP_PORT_AGENT 161
#define SNMP_PORT_TRAP 162

/** @brief SNMP version supported (v1). */
#define SNMP_V1 0

/* ------------------------------------------------------------------------------------------------
 *  ASN.1 / SNMP tags and error codes
 * --------------------------------------------------------------------------------------------- */

#define SNMPDTYPE_INTEGER 0x02
#define SNMPDTYPE_OCTET_STRING 0x04
#define SNMPDTYPE_NULL_ITEM 0x05
#define SNMPDTYPE_OBJ_ID 0x06
#define SNMPDTYPE_SEQUENCE 0x30
#define SNMPDTYPE_SEQUENCE_OF SNMPDTYPE_SEQUENCE

#define SNMPDTYPE_COUNTER 0x41
#define SNMPDTYPE_GAUGE 0x42
#define SNMPDTYPE_TIME_TICKS 0x43
#define SNMPDTYPE_OPAQUE 0x44

#define GET_REQUEST 0xA0
#define GET_NEXT_REQUEST 0xA1
#define GET_RESPONSE 0xA2
#define SET_REQUEST 0xA3

#define VALID_REQUEST(x) ((x) == GET_REQUEST || (x) == GET_NEXT_REQUEST || (x) == SET_REQUEST)

/* Generic trap types */
#define SNMPTRAP_COLDSTART 0x00
#define SNMPTRAP_WARMSTART 0x01
#define SNMPTRAP_LINKDOWN 0x02
#define SNMPTRAP_LINKUP 0x03
#define SNMPTRAP_AUTHENTICATION 0x04
#define SNMPTRAP_EGPNEIGHBORLOSS 0x05

/* Agent return/error codes */
#define SNMP_SUCCESS 0
#define OID_NOT_FOUND -1
#define TABLE_FULL -2
#define ILLEGAL_LENGTH -3
#define INVALID_ENTRY_ID -4
#define INVALID_DATA_TYPE -5

#define NO_SUCH_NAME 2
#define BAD_VALUE 3

#ifndef HTONL
#define HTONL(x)                                                                                   \
    ((((x) >> 24) & 0x000000FF) | (((x) >> 8) & 0x0000FF00) | (((x) << 8) & 0x00FF0000) |          \
     (((x) << 24) & 0xFF000000))
#endif

#define COPY_SEGMENT(x)                                                                            \
    {                                                                                              \
        request_msg.index += seglen;                                                               \
        memcpy(&response_msg.buffer[response_msg.index], &request_msg.buffer[x.start], seglen);    \
        response_msg.index += seglen;                                                              \
    }

/* ------------------------------------------------------------------------------------------------
 *  Public types
 * --------------------------------------------------------------------------------------------- */

/**
 * @brief TLV cursor describing a parsed element in an SNMP message.
 * @details LONG DESCRIPTION
 */
typedef struct {
    int32_t start;  /**< Absolute index of the TLV tag. */
    int32_t len;    /**< L-length value (decoded). */
    int32_t vstart; /**< Absolute index of the TLV value. */
    int32_t nstart; /**< Absolute index of the next TLV following this one. */
} snmp_tlv_t;

/**
 * @brief SNMP message work buffer.
 * @details LONG DESCRIPTION
 */
typedef struct {
    uint8_t buffer[SNMP_MAX_MSG]; /**< Raw bytes. */
    int32_t len;                  /**< Total valid bytes in buffer. */
    int32_t index;                /**< Cursor during parsing/encoding. */
} snmp_msg_t;

/**
 * @brief OID entry descriptor used by the agent’s MIB callbacks.
 * @details LONG DESCRIPTION
 */
typedef struct {
    uint8_t oidlen;            /**< Number of OID components. */
    uint8_t oid[SNMP_MAX_OID]; /**< OID components. */
    uint8_t dataType;          /**< ASN.1 type of the value. */
    uint8_t dataLen;           /**< Length of current value. */
    union {
        uint8_t octetstring[SNMP_MAX_STRING];
        uint32_t intval;
    } u;                                          /**< Backing storage for value. */
    void (*getfunction)(void *ptr, uint8_t *len); /**< Optional getter callback. */
    void (*setfunction)(int32_t v);               /**< Optional setter callback. */
} snmp_entry_t;

/* ------------------------------------------------------------------------------------------------
 *  Public API
 * --------------------------------------------------------------------------------------------- */

/**
 * @brief Initialize SNMP agent UDP transport and core.
 *
 * @param sock_agent  Socket number to own for UDP/161.
 * @param local_port  Local UDP port to bind (typically 161).
 * @param sock_trap   Socket number used for trap transmission (UDP/162).
 * @return true on success, false otherwise.
 */
bool SNMP_Init(uint8_t sock_agent, uint16_t local_port, uint8_t sock_trap);

/**
 * @brief Service pending SNMP datagrams without blocking.
 *
 * @param max_packets Maximum number of packets to process this call (1..N).
 * @return Number of processed packets (>=0).
 */
int SNMP_Poll(int max_packets);

/**
 * @brief 10 ms periodic tick for SNMP timing (called from NetTask).
 *
 * @return None
 * @note If you keep a 1 kHz system tick, call this every 10 ticks.
 */
void SNMP_Tick10ms(void);

/**
 * @brief Return the agent tick counter (in 10 ms units).
 *
 * @return Current tick count.
 */
uint32_t SNMP_GetTick10ms(void);

/**
 * @brief Populate current uptime (TimeTicks) for the systemUpTime OID.
 *
 * @param ptr  Output pointer to a 32-bit TimeTicks value.
 * @param len  Output length (always 4).
 */
void SNMP_GetUptime(void *ptr, uint8_t *len);

/**
 * @brief Send an SNMP trap to a manager.
 *
 * @param managerIP       4-byte IPv4 of the manager.
 * @param agentIP         4-byte IPv4 of this agent.
 * @param community       Community string (e.g., "public").
 * @param enterprise_oid  OID entry describing the enterprise id.
 * @param genericTrap     Generic trap code.
 * @param specificTrap    Specific trap code.
 * @param va_count        Number of additional var-binds (dataEntry pointers).
 * @return 0 on success, negative on error.
 */
int32_t SNMP_SendTrap(uint8_t *managerIP, uint8_t *agentIP, int8_t *community,
                      snmp_entry_t enterprise_oid, uint32_t genericTrap, uint32_t specificTrap,
                      uint32_t va_count, ...);

/* ------------------------------------------------------------------------------------------------
 *  External MIB table and helpers (provided by snmp_custom.*)
 * --------------------------------------------------------------------------------------------- */

/**
 * @brief Initialize OID table and agent community (implemented in snmp_custom.c).
 * @details LONG DESCRIPTION
 */
extern void initTable(void);

/**
 * @brief Perform initial trap setup with manager/agent IP (in snmp_custom.c).
 * @details LONG DESCRIPTION
 */
extern void initial_Trap(uint8_t *managerIP, uint8_t *agentIP);

/**
 * @brief Global OID table (implemented in snmp_custom.c).
 * @details LONG DESCRIPTION
 */
extern snmp_entry_t snmpData[];
/**
 * @brief Number of valid entries in @ref snmpData.
 * @details LONG DESCRIPTION
 */
extern const int32_t maxData;

/* ------------------------------------------------------------------------------------------------
 *  Community string (provided by snmp_custom.*)
 * --------------------------------------------------------------------------------------------- */

/** @brief Community string bytes (length @ref COMMUNITY_SIZE). */
extern const uint8_t COMMUNITY[];
/** @brief Community string length. */
extern const uint8_t COMMUNITY_SIZE;

#endif /* ENERGIS_SNMP_H */

/** @} */