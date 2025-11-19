/**
 * @file src/snmp/snmp_outletCtrl.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Uses RTOS-safe SwitchTask API: query current state and request changes.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/**
 * @brief Write boolean as ASCII '0' or '1' into output buffer.
 *
 * @param buf  Output buffer (must have at least 1 byte).
 * @param len  Out parameter set to number of bytes written (always 1).
 * @param v    Boolean value to encode.
 * @return None
 */
static inline void write_bool_ascii(void *buf, uint8_t *len, bool v) {
    char *b = (char *)buf;
    b[0] = v ? '1' : '0';
    *len = 1;
}

// For later use in RTOS queueing or SwitchTask indirection
/* static inline void getter_n(uint8_t ch, void *buf, uint8_t *len) {
    bool on = false;
    (void)Switch_GetState(ch - 1, &on); // channels are 1..8 on OID, 0..7 internal
    write_bool_ascii(buf, len, on);
}
*/

// For later use in RTOS queueing or SwitchTask indirection
/* static inline void setter_n(uint8_t ch, uint32_t u32) {
    bool on = (u32 != 0);
    (void)Switch_RequestSet(ch - 1, on, pdMS_TO_TICKS(200));
}
*/

/**
 * @brief SNMP getter for outlet state (channel 1..8) as INTEGER (4 bytes).
 *
 * @param ch   1-based channel index from OID (1..8).
 * @param buf  Output buffer; writes a 32-bit little-endian integer (0 or 1).
 * @param len  Out length; always set to 4.
 */
static inline void getter_n(uint8_t ch, void *buf, uint8_t *len) {
    int32_t v = 0;

    if (ch >= 1 && ch <= 8) {
        /* Read current channel state and normalize to 0/1 */
        v = mcp_get_channel_state((uint8_t)(ch - 1)) ? 1 : 0;
    }

    /* SNMP INTEGER entries in snmpData are declared with dataLen=4.
       Return a fixed 4-byte value to match the MIB entry. */
    memcpy(buf, &v, 4);
    *len = 4;
}

/**
 * @brief SNMP setter for outlet state (channel 1..8).
 *
 * @param ch   1-based channel index from OID (1..8).
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 * @note Uses direct MCP access (temporary, no RTOS indirection).
 */
static inline void setter_n(uint8_t ch, uint32_t u32) {
    if (ch >= 1 && ch <= 8) {
        (void)mcp_set_channel_state((uint8_t)(ch - 1), (u32 != 0));
    }
}

/**
 * @brief SNMP GET/SET callbacks for CH1
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet1_State(void *buf, uint8_t *len) { getter_n(1, buf, len); }

/**
 * @brief SNMP GET/SET callbacks for CH2
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet2_State(void *buf, uint8_t *len) { getter_n(2, buf, len); }

/**
 * @brief SNMP GET/SET callbacks for CH3
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet3_State(void *buf, uint8_t *len) { getter_n(3, buf, len); }

/**
 * @brief SNMP GET/SET callbacks for CH4
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet4_State(void *buf, uint8_t *len) { getter_n(4, buf, len); }

/**
 * @brief SNMP GET/SET callbacks for CH5
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet5_State(void *buf, uint8_t *len) { getter_n(5, buf, len); }

/**
 * @brief SNMP GET/SET callbacks for CH6
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet6_State(void *buf, uint8_t *len) { getter_n(6, buf, len); }

/**
 * @brief SNMP GET/SET callbacks for CH7
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet7_State(void *buf, uint8_t *len) { getter_n(7, buf, len); }

/**
 * @brief SNMP GET/SET callbacks for CH8
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_outlet8_State(void *buf, uint8_t *len) { getter_n(8, buf, len); }

/**
 * @brief SNMP SET callbacks for CH1
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet1_State(uint32_t u32) { setter_n(1, u32); }

/** @brief SNMP SET callbacks for CH2
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet2_State(uint32_t u32) { setter_n(2, u32); }

/** @brief SNMP SET callbacks for CH3
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet3_State(uint32_t u32) { setter_n(3, u32); }

/** @brief SNMP SET callbacks for CH4
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet4_State(uint32_t u32) { setter_n(4, u32); }

/** @brief SNMP SET callbacks for CH5
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet5_State(uint32_t u32) { setter_n(5, u32); }

/** @brief SNMP SET callbacks for CH6
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet6_State(uint32_t u32) { setter_n(6, u32); }

/** @brief SNMP SET callbacks for CH7
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet7_State(uint32_t u32) { setter_n(7, u32); }

/** @brief SNMP SET callbacks for CH8
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_outlet8_State(uint32_t u32) { setter_n(8, u32); }

/** @brief SNMP GET callback for ALL ON
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_allOn(void *buf, uint8_t *len) {
    bool all_on = true;
    for (uint8_t i = 0; i < 8; i++) {
        if (!mcp_get_channel_state(i)) {
            all_on = false;
            break;
        }
    }
    write_bool_ascii(buf, len, all_on);
}

/** @brief SNMP GET callback for ALL OFF
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_allOff(void *buf, uint8_t *len) {
    bool all_off = true;
    for (uint8_t i = 0; i < 8; i++) {
        if (mcp_get_channel_state(i)) {
            all_off = false;
            break;
        }
    }
    write_bool_ascii(buf, len, all_off);
}

/** @brief SNMP SET callback for ALL ON
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_allOn(uint32_t u32) {
    if (!u32)
        return;

    for (uint8_t i = 0; i < 8; i++) {
        (void)mcp_set_channel_state(i, 1);
    }
}

/** @brief SNMP SET callback for ALL OFF
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_allOff(uint32_t u32) {
    if (!u32)
        return;

    for (uint8_t i = 0; i < 8; i++) {
        (void)mcp_set_channel_state(i, 0);
    }
}
