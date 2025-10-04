/**
 * @file snmp_outletCtrl.c
 * @author DvidMakesThings - David Sipos
 * @defgroup snmp5 5. SNMP Agent - Outlet control
 * @ingroup snmp
 * @brief Custom SNMP agent implementation for ENERGIS PDU outlet control.
 * @{
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_outletCtrl.h"
#include "CONFIG.h"

// ####################################################################################
//  Outlet control callbacks: read the state of the relay and set it
//     (also update the display board LED state)
// ####################################################################################

/**
 * @brief Read relay state for channel @p ch (0..7) into SNMP buffer.
 * @param ch 0..7 channel index.
 * @param buf Output buffer (4 bytes, SNMP INTEGER).
 * @param len Output length (set to 4).
 * @return None
 */
static inline void get_outlet_State_common(uint8_t ch, void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(ch) ? 1 : 0;
    memcpy(buf, &v, 4);
    *len = 4;
}

/**
 * @brief Get the state of the outlet (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet1_State(void *buf, uint8_t *len) { get_outlet_State_common(0, buf, len); }

/**
 * @brief Set the state of the outlet (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet1_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 0, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Get the state of outlet 2 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet2_State(void *buf, uint8_t *len) { get_outlet_State_common(1, buf, len); }

/**
 * @brief Set the state of outlet 2 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet2_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 1, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Get the state of outlet 3 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet3_State(void *buf, uint8_t *len) { get_outlet_State_common(2, buf, len); }

/**
 * @brief Set the state of outlet 3 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet3_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 2, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Get the state of outlet 4 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet4_State(void *buf, uint8_t *len) { get_outlet_State_common(3, buf, len); }

/**
 * @brief Set the state of outlet 4 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet4_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 3, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Get the state of outlet 5 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet5_State(void *buf, uint8_t *len) { get_outlet_State_common(4, buf, len); }

/**
 * @brief Set the state of outlet 5 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet5_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 4, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Get the state of outlet 6 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet6_State(void *buf, uint8_t *len) { get_outlet_State_common(5, buf, len); }

/**
 * @brief Set the state of outlet 6 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet6_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 5, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Get the state of outlet 7 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet7_State(void *buf, uint8_t *len) { get_outlet_State_common(6, buf, len); }

/**
 * @brief Set the state of outlet 7 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet7_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 6, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Get the state of outlet 8 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 * @return None
 */
void get_outlet8_State(void *buf, uint8_t *len) { get_outlet_State_common(7, buf, len); }

/**
 * @brief Set the state of outlet 8 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 * @return None
 */
void set_outlet8_State(int32_t val) {
    uint8_t ab = 0, aa = 0;
    (void)set_relay_state_with_tag(SNMP_HANDLER, 7, (uint8_t)(val != 0), &ab, &aa);
    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, SNMP_HANDLER);
    }
}

/**
 * @brief Set the state of all outlets (relays) OFF when @p val == 1.
 * @param val The desired state (1 → turn all off).
 * @return None
 */
void set_allOff(int32_t val) {
    if (val == 1) {
        int changed_total = 0;
        uint8_t any_ab = 0, any_aa = 0;

        for (uint8_t ch = 0; ch < 8; ch++) {
            uint8_t ab = 0, aa = 0;
            changed_total += set_relay_state_with_tag(SNMP_HANDLER, ch, 0u, &ab, &aa);
            any_ab |= ab;
            any_aa |= aa;
        }

        INFO_PRINT("SNMP allOff: changed=%d\r\n", changed_total);

        if (any_ab || any_aa) {
            uint16_t mask = mcp_dual_asymmetry_mask();
            WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n",
                          any_aa ? "PERSISTING" : "DETECTED", (unsigned)mask, SNMP_HANDLER);
        }
    }
}

/**
 * @brief Turn *all* outlets on when @p val != 0.
 * @param val Non-zero → turn all on.
 * @return None
 */
void set_allOn(int32_t val) {
    if (val != 0) {
        int changed_total = 0;
        uint8_t any_ab = 0, any_aa = 0;

        for (uint8_t ch = 0; ch < 8; ch++) {
            uint8_t ab = 0, aa = 0;
            changed_total += set_relay_state_with_tag(SNMP_HANDLER, ch, 1u, &ab, &aa);
            any_ab |= ab;
            any_aa |= aa;
        }

        INFO_PRINT("SNMP allOn: changed=%d\r\n", changed_total);

        if (any_ab || any_aa) {
            uint16_t mask = mcp_dual_asymmetry_mask();
            WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n",
                          any_aa ? "PERSISTING" : "DETECTED", (unsigned)mask, SNMP_HANDLER);
        }
    }
}

/**
 * @brief Read all 8 relay states and return as a bitmask.
 *
 * Bit mapping (LSB first):
 *   bit 0 -> CH1, bit 1 -> CH2, ... bit 7 -> CH8 (1 = ON, 0 = OFF)
 *
 * @param buf Output buffer (SNMP INTEGER, 4 bytes). Lower byte holds the mask.
 * @param len Output length (set to 4).
 * @return void
 */
void get_allState(void *buf, uint8_t *len) {
    uint32_t mask = 0;
    for (uint8_t ch = 0; ch < 8; ++ch) {
        if (mcp_relay_read_pin(ch))
            mask |= (1u << ch);
    }
    memcpy(buf, &mask, sizeof(mask));
    *len = sizeof(mask);
}

/* Make the two OIDs resolve to the same function address (GCC/Clang). */
#if defined(__GNUC__)
extern void get_allState(void *buf, uint8_t *len);
extern void get_allOff(void *buf, uint8_t *len) __attribute__((alias("get_allState")));
extern void get_allOn(void *buf, uint8_t *len) __attribute__((alias("get_allState")));
#else
/* Fallback: tiny wrappers (still correct; may not dedupe code as tightly). */
void get_allOff(void *buf, uint8_t *len) { get_allState(buf, len); }
void get_allOn(void *buf, uint8_t *len) { get_allState(buf, len); }
#endif

/** @} */ // End of snmp5