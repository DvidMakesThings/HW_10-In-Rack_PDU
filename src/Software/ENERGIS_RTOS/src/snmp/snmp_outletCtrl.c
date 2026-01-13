/**
 * @file snmp_outletCtrl.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-12-16
 *
 * @details
 * Implementation of SNMP outlet control callbacks using the synchronous SwitchTask
 * API v2.0. Provides deterministic GET/SET operations with hardware verification.
 *
 * Design Principles:
 * - All operations are deterministic and synchronous
 * - GET operations read directly from hardware (no cache)
 * - SET operations block until hardware write is verified
 * - Detailed error codes enable accurate SNMP response
 *
 * SNMP SET Flow:
 * 1. SET request arrives from SNMP agent
 * 2. setter_n() decodes and validates requested state
 * 3. Switch_SetChannelCompat() called (blocking)
 * 4. SwitchTask acquires mutex, executes I2C write
 * 5. Hardware read-back verifies the write (up to 500ms polling)
 * 6. Result returned and SNMP response sent with correct status
 *
 * SNMP GET Flow:
 * 1. GET request arrives from SNMP agent
 * 2. getter_n() calls Switch_GetStateCompat() (blocking)
 * 3. Hardware read executed with mutex protection
 * 4. Actual hardware state returned as 4-byte INTEGER
 * 5. SNMP response sent with current value
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ==================== Internal Helper Functions ==================== */

/**
 * @brief Generic SNMP getter for outlet state.
 *
 * Reads the current state of a specified outlet from hardware and encodes the
 * result as a 4-byte little-endian INTEGER (0=OFF, 1=ON). Uses synchronous
 * SwitchTask API with mutex protection.
 *
 * @param ch 1-based channel index from SNMP OID (1..8)
 * @param buf Output buffer; minimum 4 bytes required
 * @param len Pointer to receive length; always set to 4
 *
 * @return None
 *
 * @note Returns 0 if channel out of range or hardware read fails.
 * @note Blocks briefly during hardware I2C read operation.
 */
static inline void getter_n(uint8_t ch, void *buf, uint8_t *len) {
    int32_t v = 0;

    /* Validate channel range (SNMP uses 1-based indexing) */
    if (ch >= 1 && ch <= 8) {
        bool state = false;

        /* Synchronous read via SwitchTask (mutex-protected hardware I2C read) */
        if (Switch_GetStateCompat((uint8_t)(ch - 1), &state)) {
            v = state ? 1 : 0;
        }
    }

    /* Write fixed 4-byte INTEGER to match SNMP table definition */
    memcpy(buf, &v, 4);
    if (len) {
        *len = 4;
    }
}

/**
 * @brief Generic SNMP setter for outlet state.
 *
 * Sets the desired state of a specified outlet via synchronous SwitchTask API.
 * Implements strict "write-then-verify" semantics with active hardware polling.
 *
 * Operation sequence:
 * 1. Decode and validate requested state (0=OFF, non-zero=ON)
 * 2. Validate channel range (1..8)
 * 3. Execute synchronous switch via Switch_SetChannelCompat() (blocking)
 * 4. SwitchTask polls MCP23017 relay GPIO up to 500ms until state matches
 * 5. Log error if channel invalid, switch fails, or timeout/mismatch occurs
 *
 * @param ch 1-based channel index from SNMP OID (1..8)
 * @param u32 Desired state (0=OFF, non-zero=ON)
 *
 * @return None
 *
 * @note Blocking operation with up to 500ms hardware verification.
 * @note Logs structured error codes for diagnostics.
 */
static inline void setter_n(uint8_t ch, uint32_t u32) {
    bool desired = (u32 != 0u);

    /* Validate channel range */
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

    /* Convert to 0-based index for SwitchTask API */
    uint8_t idx = (uint8_t)(ch - 1u);

    /* Execute synchronous switch with hardware verification */
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

void get_outlet1_State(void *buf, uint8_t *len) { getter_n(1, buf, len); }
void get_outlet2_State(void *buf, uint8_t *len) { getter_n(2, buf, len); }
void get_outlet3_State(void *buf, uint8_t *len) { getter_n(3, buf, len); }
void get_outlet4_State(void *buf, uint8_t *len) { getter_n(4, buf, len); }
void get_outlet5_State(void *buf, uint8_t *len) { getter_n(5, buf, len); }
void get_outlet6_State(void *buf, uint8_t *len) { getter_n(6, buf, len); }
void get_outlet7_State(void *buf, uint8_t *len) { getter_n(7, buf, len); }
void get_outlet8_State(void *buf, uint8_t *len) { getter_n(8, buf, len); }

/* ==================== Per-Channel SET Callbacks ==================== */

void set_outlet1_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    /* Copy SNMP value (handle variable size inputs) */
    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    /* Normalize to 0/1 */
    if (u32 != 0u) {
        u32 = 1u;
    }

    setter_n(1, u32);
}

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

void get_allOn_State(void *buf, uint8_t *len) {
    int32_t v = 0;
    memcpy(buf, &v, 4);
    *len = 4;
}

void set_allOn_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    /* Copy SNMP value */
    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    /* Execute bulk ON if non-zero */
    if (u32 != 0u) {
        bool ok = Switch_AllOnCompat(500);
        /* Log warning if operation fails */
        if (!ok) {
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NET_SNMP_OUTLETCTRL, 0x0);
            WARNING_PRINT("0x%04X [SNMP] AllOn failed\r\n", (unsigned)err_code);
        }
    }
}

void get_allOff_State(void *buf, uint8_t *len) {
    int32_t v = 0;
    memcpy(buf, &v, 4);
    *len = 4;
}

void set_allOff_State(int32_t size, uint8_t dataType, void *val) {
    (void)dataType;
    uint32_t u32 = 0;

    /* Copy SNMP value */
    if (val && (size > 0)) {
        size_t n = (size_t)size;
        if (n > sizeof(u32)) {
            n = sizeof(u32);
        }
        memcpy(&u32, val, n);
    }

    /* Execute bulk OFF if non-zero */
    if (u32 != 0u) {
        bool ok = Switch_AllOffCompat(500);
        /* Log warning if operation fails */
        if (!ok) {
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NET_SNMP_OUTLETCTRL, 0x1);
            WARNING_PRINT("0x%04X [SNMP] AllOff failed\r\n", (unsigned)err_code);
        }
    }
}