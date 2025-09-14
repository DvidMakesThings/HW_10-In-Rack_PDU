/**
 * @file snmp_custom.c
 * @author DvidMakesThings - David Sipos
 * @defgroup snmp2 2. SNMP Agent
 * @ingroup snmp
 * @brief Custom SNMP agent implementation for ENERGIS PDU.
 * @{
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_custom.h"
#include "CONFIG.h" // UART_CMD_BUF_LEN, etc.
#include "drivers/CAT24C512_driver.h"
#include "network/wizchip_conf.h" // wiz_NetInfo
#include "snmp.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static float latest_temp_voltage = 0.0f;
static float latest_temp_celsius = 0.0f;

/**
 * @brief Bring in your running network configuration
 * @return Pointer to the network configuration structure.
 */
extern wiz_NetInfo g_net_info;

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

// Forward‐declare our new callbacks

//------------------------------------------------------------------------------
// the full table of OID support:

dataEntryType snmpData[] = {
    // Standard system‐group stuff
    {8,
     {0x2b, 6, 1, 2, 1, 1, 1, 0},
     SNMPDTYPE_OCTET_STRING,
     29,
     {"ENERGIS 8 CHANNEL MANAGED PDU"},
     NULL,
     NULL},
    {8,
     {0x2b, 6, 1, 2, 1, 1, 2, 0},
     SNMPDTYPE_OBJ_ID,
     8,
     {"\x2b\x06\x01\x02\x01\x01\x02\x00"},
     NULL,
     NULL},
    {8, {0x2b, 6, 1, 2, 1, 1, 3, 0}, SNMPDTYPE_TIME_TICKS, 0, {""}, currentUptime, NULL},
    {8, {0x2b, 6, 1, 2, 1, 1, 4, 0}, SNMPDTYPE_OCTET_STRING, 64, {""}, get_sysContact, NULL},
    {8, {0x2b, 6, 1, 2, 1, 1, 5, 0}, SNMPDTYPE_OCTET_STRING, 15, {""}, get_serialNumber, NULL},
    {8, {0x2b, 6, 1, 2, 1, 1, 6, 0}, SNMPDTYPE_OCTET_STRING, 4, {"Wien"}, NULL, NULL},
    {8, {0x2b, 6, 1, 2, 1, 1, 7, 0}, SNMPDTYPE_INTEGER, 4, {""}, NULL, NULL},

    // “long-length” tests
    {10,
     {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x01, 0x00},
     SNMPDTYPE_OCTET_STRING,
     30,
     {"long-length OID Test #1"},
     NULL,
     NULL},
    {10,
     {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0xad, 0x42, 0x01, 0x00},
     SNMPDTYPE_OCTET_STRING,
     35,
     {"long-length OID Test #2"},
     NULL,
     NULL},
    {10,
     {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0xad, 0x42, 0x02, 0x00},
     SNMPDTYPE_OBJ_ID,
     10,
     {"\x2b\x06\x01\x04\x01\x81\xad\x42\x02\x00"},
     NULL,
     NULL},

    // Network configuration under .1.3.6.1.4.1.19865.4.X.0
    //    a) ip address:  .4.1.0
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x01, 0x00},
     SNMPDTYPE_OCTET_STRING,
     16,
     {""},
     get_networkIP,
     NULL},
    //    b) subnet mask: .4.2.0
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x02, 0x00},
     SNMPDTYPE_OCTET_STRING,
     16,
     {""},
     get_networkMask,
     NULL},
    //    c) gateway:     .4.3.0
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x03, 0x00},
     SNMPDTYPE_OCTET_STRING,
     16,
     {""},
     get_networkGateway,
     NULL},
    //    d) DNS server:  .4.4.0
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x04, 0x00},
     SNMPDTYPE_OCTET_STRING,
     16,
     {""},
     get_networkDNS,
     NULL},

    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x01, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet1_State,
     set_outlet1_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x02, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet2_State,
     set_outlet2_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x03, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet3_State,
     set_outlet3_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x04, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet4_State,
     set_outlet4_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x05, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet5_State,
     set_outlet5_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x06, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet6_State,
     set_outlet6_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x07, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet7_State,
     set_outlet7_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x08, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_outlet8_State,
     set_outlet8_State},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x09, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_allOff,
     set_allOff},
    {11,
     {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x0A, 0x00},
     SNMPDTYPE_INTEGER,
     4,
     {""},
     get_allOn,
     set_allOn},
    // Temperature monitoring under .1.3.6.1.4.1.19865.3.X.0
    //    a) die sensor voltage:     .3.1.0
    {11,
     {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x01, 0x00},
     SNMPDTYPE_OCTET_STRING,
     16,
     {""},
     get_tempSensorVoltage,
     NULL},
    //    b) die sensor temperature: .3.2.0
    {11,
     {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x02, 0x00},
     SNMPDTYPE_OCTET_STRING,
     16,
     {""},
     get_tempSensorTemperature,
     NULL},
};

const int32_t maxData = (sizeof(snmpData) / sizeof(dataEntryType));

/**
 * @brief Initialize the SNMP table with default values.
 *
 * This function initializes the SNMP table with default values for
 * various OIDs. It is called during the initialization of the SNMP agent.
 */
void initTable() {
    // Example integer value for [OID 1.3.6.1.2.1.1.7.0]
    snmpData[6].u.intval = -5;
}

/**
 * @brief Initialize the SNMP agent with the given manager and agent IP addresses.
 *
 * @param managerIP Pointer to the manager IP address.
 * @param agentIP Pointer to the agent IP address.
 */
void initial_Trap(uint8_t *managerIP, uint8_t *agentIP) {
    // SNMP Trap: WarmStart(1) Trap
    {
        dataEntryType enterprise_oid = {
            0x0a,
            {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x01, 0x00},
            SNMPDTYPE_OBJ_ID,
            0x0a,
            {"\x2b\x06\x01\x04\x01\x81\x9b\x19\x10\x00"},
            NULL,
            NULL};
        snmp_sendTrap(managerIP, agentIP, (void *)COMMUNITY, enterprise_oid, SNMPTRAP_WARMSTART, 0,
                      0);
    }
}

//------------------------------------------------------------------------------
// Callbacks for the SNMP agent

/**
 * @brief SNMP callback: Return latest die temp sensor voltage as string
 */
void get_tempSensorVoltage(void *buf, uint8_t *len) {
    adc_select_input(4);
    uint16_t raw = adc_read();
    latest_temp_voltage = raw * 3.0f / 4096.0f;
    *len = snprintf((char *)buf, 16, "%.5f", latest_temp_voltage);
}

/**
 * @brief SNMP callback: Return latest die temp sensor temperature as string
 */
void get_tempSensorTemperature(void *buf, uint8_t *len) {
    adc_select_input(4);
    uint16_t raw = adc_read();
    latest_temp_celsius = 27.0f - (latest_temp_voltage - 0.706f) / 0.001721f;
    *len = snprintf((char *)buf, 16, "%.3f", latest_temp_celsius);
}
/**
 * @brief Return contact information as an OCTET STRING.
 * @param buf Pointer to the buffer to store the contact information.
 * @param len Pointer to the length of the contact information.
 */
void get_sysContact(void *buf, uint8_t *len) {
    const char *s = "dvidmakesthings@gmail.com";
    *len = strlen(s);
    memcpy(buf, s, *len);
}

/**
 * @brief Read the device Serial Number from EEPROM and return as an OCTET STRING.
 * @param buf Pointer to the buffer to store the serial number.
 * @param len Pointer to the length of the serial number.
 */
void get_serialNumber(void *buf, uint8_t *len) {
    // Buffer must hold DEFAULT_SN plus terminating '\0'
    char sn[sizeof(DEFAULT_SN)];
    // Read exactly strlen(DEFAULT_SN)+1 bytes (including the '\0')
    EEPROM_ReadSystemInfo((uint8_t *)sn, sizeof(sn));
    // Now copy into SNMP buffer without the terminating NUL
    *len = strlen(sn);
    memcpy(buf, sn, *len);
}

//------------------------------------------------------------------------------
// Network callbacks: format each 4‐byte field into a dotted‐decimal string

/**
 * @brief Get the IP address of the device.
 * @param ptr Pointer to the buffer to store the IP address.
 * @param len Pointer to the length of the IP address.
 */
void get_networkIP(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2],
                    g_net_info.ip[3]);
}

/**
 * @brief Get the subnet mask of the device.
 * @param ptr Pointer to the buffer to store the subnet mask.
 * @param len Pointer to the length of the subnet mask.
 */
void get_networkMask(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.sn[0], g_net_info.sn[1], g_net_info.sn[2],
                    g_net_info.sn[3]);
}

/**
 * @brief Get the gateway address of the device.
 * @param ptr Pointer to the buffer to store the gateway address.
 * @param len Pointer to the length of the gateway address.
 */
void get_networkGateway(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.gw[0], g_net_info.gw[1], g_net_info.gw[2],
                    g_net_info.gw[3]);
}

/**
 * @brief Get the DNS server address of the device.
 * @param ptr Pointer to the buffer to store the DNS server address.
 * @param len Pointer to the length of the DNS server address.
 */
void get_networkDNS(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.dns[0], g_net_info.dns[1], g_net_info.dns[2],
                    g_net_info.dns[3]);
}

//------------------------------------------------------------------------------
// Outlet control callbacks: read the state of the relay and set it
//    (also update the display board LED state)

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
