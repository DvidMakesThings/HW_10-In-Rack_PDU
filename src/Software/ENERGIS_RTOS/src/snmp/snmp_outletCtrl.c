/**
 * @file snmp_outletCtrl.c
 * @author DvidMakesThings - David Sipos
 * @defgroup snmp SNMP
 * @brief SNMP protocol handlers for ENERGIS.
 * @{
 *
 * @defgroup snmp02 2. Outlet Control
 * @ingroup snmp
 * @brief SNMP outlet control with deterministic synchronous operations.
 * @{
 * @version 2.0.0
 * @date 2025-12-16
 *
 * @details
 * SNMP outlet control using the v2.0 synchronous SwitchTask API.
 *
 * Design Principles:
 * - All operations are DETERMINISTIC and SYNCHRONOUS
 * - GETTER reads directly from hardware (not cache)
 * - SETTER blocks until hardware is written and verified
 * - Detailed error codes enable accurate SNMP response
 *
 * SNMP SET Flow:
 * 1. SET request arrives
 * 2. setter_n() calls Switch_SetChannel() (BLOCKING)
 * 3. SwitchTask mutex acquired, I2C write executed
 * 4. Hardware read-back verifies the write
 * 5. Result returned (success/fail)
 * 6. SNMP response sent with correct status
 *
 * SNMP GET Flow:
 * 1. GET request arrives
 * 2. getter_n() calls Switch_GetState() (BLOCKING)
 * 3. Hardware read executed with mutex held
 * 4. Actual hardware state returned
 * 5. SNMP response sent with current value
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ==================== Internal Helper Functions ==================== */

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
        bool state = false;

        /* Deterministic read via SwitchTask (mutex, verify, cache) */
        if (Switch_GetStateCompat((uint8_t)(ch - 1), &state)) {
            v = state ? 1 : 0;
        }
    }

    /* SNMP INTEGER entries in snmpData are declared with dataLen=4.
       Return a fixed 4-byte value to match the MIB entry. */
    memcpy(buf, &v, 4);
    if (len) {
        *len = 4;
    }
}

/**
 * @brief SNMP setter for outlet state (channel 1..8) as INTEGER (4 bytes).
 *
 * Implements strict "write-then-verify" semantics with active MCP polling:
 *  1) Decode requested state.
 *  2) Execute synchronous switch via Switch_SetChannelCompat().
 *  3) Poll MCP23017 relay GPIO up to 500 ms until the state matches.
 *  4) Log failures (invalid channel, switch failure, timeout/mismatch).
 *
 * @param ch   1-based channel index from OID (1..8).
 * @param u32  Desired state; 0 = OFF, non-zero = ON.
 */
static inline void setter_n(uint8_t ch, uint32_t u32) {
    bool desired = (u32 != 0u);

    /* Validate channel range (SNMP uses 1..8) */
    if (ch < 1u || ch > 8u) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_SNMP_OUTLETCTRL, 0x01);
        ERROR_PRINT_CODE("0x%x [SNMP OUTLET] Invalid channel index: %u\r\n", errorcode,
                          (unsigned)ch);
        // Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    uint8_t idx = (uint8_t)(ch - 1u);

    /* Step 1: Synchronous switch via SwitchTask (includes write + verify) */
    if (!Switch_SetChannelCompat(idx, desired, 0u)) {
#if ERRORLOGGER
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_SNMP_OUTLETCTRL, 0x02);
        ERROR_PRINT_CODE("0x%x [SNMP OUTLET] Switch_SetChannelCompat failed: ch=%u state=%u\r\n",
                          errorcode, (unsigned)ch, (unsigned)(desired ? 1u : 0u));
        // Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }
}

/* ==================== Per-Channel GET Callbacks ==================== */

/**
 * @brief SNMP GET callback for outlet 1 state.
 */
void get_outlet1_State(void *buf, uint8_t *len) { getter_n(1, buf, len); }

/**
 * @brief SNMP GET callback for outlet 2 state.
 */
void get_outlet2_State(void *buf, uint8_t *len) { getter_n(2, buf, len); }

/**
 * @brief SNMP GET callback for outlet 3 state.
 */
void get_outlet3_State(void *buf, uint8_t *len) { getter_n(3, buf, len); }

/**
 * @brief SNMP GET callback for outlet 4 state.
 */
void get_outlet4_State(void *buf, uint8_t *len) { getter_n(4, buf, len); }

/**
 * @brief SNMP GET callback for outlet 5 state.
 */
void get_outlet5_State(void *buf, uint8_t *len) { getter_n(5, buf, len); }

/**
 * @brief SNMP GET callback for outlet 6 state.
 */
void get_outlet6_State(void *buf, uint8_t *len) { getter_n(6, buf, len); }

/**
 * @brief SNMP GET callback for outlet 7 state.
 */
void get_outlet7_State(void *buf, uint8_t *len) { getter_n(7, buf, len); }

/**
 * @brief SNMP GET callback for outlet 8 state.
 */
void get_outlet8_State(void *buf, uint8_t *len) { getter_n(8, buf, len); }

/* ==================== Per-Channel SET Callbacks ==================== */

/**
 * @brief SNMP SET callback for outlet 1 state.
 */
void set_outlet1_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    /* Normalize to logical 0/1 */
    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(1, u32);
}

/**
 * @brief SNMP SET callback for outlet 2 state.
 */
void set_outlet2_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(2, u32);
}

/**
 * @brief SNMP SET callback for outlet 3 state.
 */
void set_outlet3_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(3, u32);
}

/**
 * @brief SNMP SET callback for outlet 4 state.
 */
void set_outlet4_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(4, u32);
}

/**
 * @brief SNMP SET callback for outlet 5 state.
 */
void set_outlet5_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(5, u32);
}

/**
 * @brief SNMP SET callback for outlet 6 state.
 */
void set_outlet6_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(6, u32);
}

/**
 * @brief SNMP SET callback for outlet 7 state.
 */
void set_outlet7_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(7, u32);
}

/**
 * @brief SNMP SET callback for outlet 8 state.
 */
void set_outlet8_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(8, u32);
}

/* ==================== Bulk Operations ==================== */

/**
 * @brief SNMP GET callback for allOn trigger (always returns 0).
 */
void get_allOn_State(void *buf, uint8_t *len) {
    int32_t v = 0;
    memcpy(buf, &v, 4);
    *len = 4;
}

/**
 * @brief SNMP SET callback for allOn trigger.
 *
 * @details
 * When set to non-zero, turns all outlets ON.
 * BLOCKING operation - returns only after all channels are set.
 */
void set_allOn_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        bool ok = Switch_AllOnCompat(500);
        if (!ok) {
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NET_SNMP_OUTLETCTRL, 0x0);
            WARNING_PRINT("0x%04X [SNMP] AllOn failed\r\n", (unsigned)err_code);
        }
    }
}

/**
 * @brief SNMP GET callback for allOff trigger (always returns 0).
 */
void get_allOff_State(void *buf, uint8_t *len) {
    int32_t v = 0;
    memcpy(buf, &v, 4);
    *len = 4;
}

/**
 * @brief SNMP SET callback for allOff trigger.
 *
 * @details
 * When set to non-zero, turns all outlets OFF.
 * BLOCKING operation - returns only after all channels are set.
 */
void set_allOff_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    if (u32 != 0u) {
        bool ok = Switch_AllOffCompat(500);
        if (!ok) {
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NET_SNMP_OUTLETCTRL, 0x1);
            WARNING_PRINT("0x%04X [SNMP] AllOff failed\r\n", (unsigned)err_code);
        }
    }
}