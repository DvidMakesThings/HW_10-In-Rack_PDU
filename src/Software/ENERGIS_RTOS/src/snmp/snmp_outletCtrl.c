/**
 * @file src/snmp/snmp_outletCtrl.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2.0
 * @date 2025-12-10
 *
 * @details Uses RTOS-safe SwitchTask API: query current state and request changes.
 * All MCP I2C operations are delegated to SwitchTask to prevent watchdog
 * starvation during SNMP stress testing.
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

/**
 * @brief SNMP getter for outlet state (channel 1..8) as INTEGER (4 bytes).
 *
 * @param ch   1-based channel index from OID (1..8).
 * @param buf  Output buffer; writes a 32-bit little-endian integer (0 or 1).
 * @param len  Out length; always set to 4.
 * @note Includes 50ms delay for cooperative scheduling during stress testing.
 */
static inline void getter_n(uint8_t ch, void *buf, uint8_t *len) {
    int32_t v = 0;

    if (ch >= 1 && ch <= 8) {
        /* Read current channel state from SwitchTask cache (non-blocking) */
        bool on = false;
        if (Switch_GetState((uint8_t)(ch - 1), &on)) {
            v = on ? 1 : 0;
        }
    }

    /* SNMP INTEGER entries in snmpData are declared with dataLen=4.
       Return a fixed 4-byte value to match the MIB entry. */
    memcpy(buf, &v, 4);
    *len = 4;

    /* Yield to higher-priority tasks (ButtonTask) - safety critical */
    vTaskDelay(pdMS_TO_TICKS(25));
}

/**
 * @brief SNMP setter for outlet state (channel 1..8).
 *
 * @param ch   1-based channel index from OID (1..8).
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 * @note Uses non-blocking SwitchTask API with timeout=0 (no wait).
 *       Includes 50ms delay for cooperative scheduling during stress testing.
 */
static inline void setter_n(uint8_t ch, uint32_t u32) {
    if (ch >= 1 && ch <= 8) {
        /* Enqueue command to SwitchTask (non-blocking, timeout=0) */
        (void)Switch_SetChannel((uint8_t)(ch - 1), (u32 != 0), 0);
    }

    /* Yield to higher-priority tasks (ButtonTask) - safety critical */
    vTaskDelay(pdMS_TO_TICKS(25));
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
    uint8_t states = 0;
    if (Switch_GetAllStates(&states)) {
        /* All ON if all 8 bits are set */
        bool all_on = (states == 0xFF);
        write_bool_ascii(buf, len, all_on);
    } else {
        write_bool_ascii(buf, len, false);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

/** @brief SNMP GET callback for ALL OFF
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_allOff(void *buf, uint8_t *len) {
    uint8_t states = 0;
    if (Switch_GetAllStates(&states)) {
        /* All OFF if all 8 bits are clear */
        bool all_off = (states == 0x00);
        write_bool_ascii(buf, len, all_off);
    } else {
        write_bool_ascii(buf, len, false);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
}

/** @brief SNMP SET callback for ALL ON
 * @param u32  Nonzero -> turn all ON.
 * @return None
 */
void set_allOn(uint32_t u32) {
    if (!u32)
        return;

    /* Enqueue all-on command to SwitchTask (non-blocking, timeout=0) */
    (void)Switch_AllOn(0);

    /* Longer delay for bulk operation - allows SwitchTask to process */
    vTaskDelay(pdMS_TO_TICKS(100));
}

/** @brief SNMP SET callback for ALL OFF
 * @param u32  Nonzero -> turn all OFF.
 * @return None
 */
void set_allOff(uint32_t u32) {
    if (!u32)
        return;

    /* Enqueue all-off command to SwitchTask (non-blocking, timeout=0) */
    (void)Switch_AllOff(0);

    /* Longer delay for bulk operation - allows SwitchTask to process */
    vTaskDelay(pdMS_TO_TICKS(100));
}