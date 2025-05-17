#include "snmp_custom.h"           
#include "snmp.h"
#include "drivers/CAT24C512_driver.h"
#include "network/wizchip_conf.h"   // wiz_NetInfo
#include "CONFIG.h"                 // UART_CMD_BUF_LEN, etc.
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Bring in your running network configuration
extern wiz_NetInfo g_net_info;

// Forward‐declare our new callbacks
static void get_networkIP(void *ptr, uint8_t *len);
static void get_networkMask(void *ptr, uint8_t *len);
static void get_networkGateway(void *ptr, uint8_t *len);
static void get_networkDNS(void *ptr, uint8_t *len);
static void get_serialNumber(void *buf, uint8_t *len);
static void get_sysContact(void *buf, uint8_t *len);
static void get_outlet1_State(void *buf, uint8_t *len);
static void set_outlet1_State(int32_t val);
static void get_outlet2_State(void *buf, uint8_t *len);
static void set_outlet2_State(int32_t val);
static void get_outlet3_State(void *buf, uint8_t *len);
static void set_outlet3_State(int32_t val);
static void get_outlet4_State(void *buf, uint8_t *len);
static void set_outlet4_State(int32_t val);
static void get_outlet5_State(void *buf, uint8_t *len);
static void set_outlet5_State(int32_t val);
static void get_outlet6_State(void *buf, uint8_t *len);
static void set_outlet6_State(int32_t val);
static void get_outlet7_State(void *buf, uint8_t *len);
static void set_outlet7_State(int32_t val);
static void get_outlet8_State(void *buf, uint8_t *len);
static void set_outlet8_State(int32_t val);
static void get_allOff(void *buf, uint8_t *len);
static void set_allOff(int32_t val);

//------------------------------------------------------------------------------
// the full table of OID support:
dataEntryType snmpData[] = {
    // Standard system‐group stuff
    {8, {0x2b,6,1,2,1,1,1,0}, SNMPDTYPE_OCTET_STRING, 29, {"ENERGIS 8 CHANNEL MANAGED PDU"}, NULL, NULL},
    {8, {0x2b,6,1,2,1,1,2,0}, SNMPDTYPE_OBJ_ID,        8,  {"\x2b\x06\x01\x02\x01\x01\x02\x00"}, NULL, NULL},
    {8, {0x2b,6,1,2,1,1,3,0}, SNMPDTYPE_TIME_TICKS,    0,  {""},                              currentUptime, NULL},
    {8, {0x2b,6,1,2,1,1,4,0}, SNMPDTYPE_OCTET_STRING,  64, {""},                              get_sysContact, NULL},
    {8, {0x2b,6,1,2,1,1,5,0}, SNMPDTYPE_OCTET_STRING,  15, {""},                              get_serialNumber, NULL},
    {8, {0x2b,6,1,2,1,1,6,0}, SNMPDTYPE_OCTET_STRING,   4, {"Wien"},                          NULL, NULL},
    {8, {0x2b,6,1,2,1,1,7,0}, SNMPDTYPE_INTEGER,        4, {""},                              NULL, NULL},

    // “long-length” tests
    {10,{0x2b,0x06,0x01,0x04,0x01,0x81,0x9b,0x19,0x01,0x00},
        SNMPDTYPE_OCTET_STRING, 30, {"long-length OID Test #1"}, NULL, NULL},
    {10,{0x2b,0x06,0x01,0x04,0x01,0x81,0xad,0x42,0x01,0x00},
        SNMPDTYPE_OCTET_STRING, 35, {"long-length OID Test #2"}, NULL, NULL},
    {10,{0x2b,0x06,0x01,0x04,0x01,0x81,0xad,0x42,0x02,0x00},
        SNMPDTYPE_OBJ_ID,        10, {"\x2b\x06\x01\x04\x01\x81\xad\x42\x02\x00"}, NULL, NULL},

    // Network configuration under .1.3.6.1.4.1.19865.4.X.0
    //    a) ip address:  .4.1.0
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x01,0x00},
        SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkIP,    NULL},
    //    b) subnet mask: .4.2.0
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x02,0x00},
        SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkMask,  NULL},
    //    c) gateway:     .4.3.0
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x03,0x00},
        SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkGateway,NULL},
    //    d) DNS server:  .4.4.0
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x04,0x00},
        SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkDNS,    NULL},

    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x01,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet1_State, set_outlet1_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x02,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet2_State, set_outlet2_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x03,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet3_State, set_outlet3_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x04,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet4_State, set_outlet4_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x05,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet5_State, set_outlet5_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x06,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet6_State, set_outlet6_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x07,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet7_State, set_outlet7_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x08,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_outlet8_State, set_outlet8_State},
    {11,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x09,0x00}, SNMPDTYPE_INTEGER, 4,{""}, get_allOff, set_allOff},
};

const int32_t maxData = (sizeof(snmpData) / sizeof(dataEntryType));

void initTable() {
    // Example integer value for [OID 1.3.6.1.2.1.1.7.0]
    snmpData[6].u.intval = -5;
}

void initial_Trap(uint8_t *managerIP, uint8_t *agentIP) {
    // SNMP Trap: WarmStart(1) Trap
    {
        dataEntryType enterprise_oid = {
            0x0a, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x01, 0x00},
            SNMPDTYPE_OBJ_ID, 0x0a, {"\x2b\x06\x01\x04\x01\x81\x9b\x19\x10\x00"}, NULL, NULL};
        snmp_sendTrap(managerIP, agentIP, (void *)COMMUNITY, enterprise_oid, SNMPTRAP_WARMSTART, 0,
                      0);
    }
}

//------------------------------------------------------------------------------
// Callbacks for the SNMP agent

/**
 * @brief Return contact information as an OCTET STRING.
 * @param buf Pointer to the buffer to store the contact information.
 * @param len Pointer to the length of the contact information.
 */
static void get_sysContact(void *buf, uint8_t *len) {
    const char *s = "dvidmakesthings@gmail.com";
    *len = strlen(s);
    memcpy(buf, s, *len);
}

/**
 * @brief Read the device Serial Number from EEPROM and return as an OCTET STRING.
 * @param buf Pointer to the buffer to store the serial number.
 * @param len Pointer to the length of the serial number.
 */
static void get_serialNumber(void *buf, uint8_t *len) {
    // Buffer must hold DEFAULT_SN plus terminating '\0'
    char sn[sizeof(DEFAULT_SN)];
    // Read exactly strlen(DEFAULT_SN)+1 bytes (including the '\0')
    EEPROM_ReadSystemInfo((uint8_t*)sn, sizeof(sn));
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
static void get_networkIP(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u",
        g_net_info.ip[0], g_net_info.ip[1],
        g_net_info.ip[2], g_net_info.ip[3]);
}

/**
 * @brief Get the subnet mask of the device.
 * @param ptr Pointer to the buffer to store the subnet mask.
 * @param len Pointer to the length of the subnet mask.
 */
static void get_networkMask(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u",
        g_net_info.sn[0], g_net_info.sn[1],
        g_net_info.sn[2], g_net_info.sn[3]);
}

/**
 * @brief Get the gateway address of the device.
 * @param ptr Pointer to the buffer to store the gateway address.
 * @param len Pointer to the length of the gateway address.
 */
static void get_networkGateway(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u",
        g_net_info.gw[0], g_net_info.gw[1],
        g_net_info.gw[2], g_net_info.gw[3]);
}

/**
 * @brief Get the DNS server address of the device.
 * @param ptr Pointer to the buffer to store the DNS server address.
 * @param len Pointer to the length of the DNS server address.
 */
static void get_networkDNS(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u",
        g_net_info.dns[0], g_net_info.dns[1],
        g_net_info.dns[2], g_net_info.dns[3]);
}

//------------------------------------------------------------------------------
// Outlet control callbacks: read the state of the relay and set it
//    (also update the display board LED state)


/**
 * @brief Get the state of the outlet (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet1_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(0) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of the outlet (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet1_State(int32_t val) {
    bool curr=mcp_relay_read_pin(0), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(1);
}

/**
 * @brief Get the state of outlet 2 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet2_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(1) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of outlet 2 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet2_State(int32_t val) {
    bool curr=mcp_relay_read_pin(1), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(2);
}

/**
 * @brief Get the state of outlet 3 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet3_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(2) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of outlet 3 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet3_State(int32_t val) {
    bool curr=mcp_relay_read_pin(2), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(3);
}

/**
 * @brief Get the state of outlet 4 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet4_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(3) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of outlet 4 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet4_State(int32_t val) {
    bool curr=mcp_relay_read_pin(3), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(4);
}

/**
 * @brief Get the state of outlet 5 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet5_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(4) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of outlet 5 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet5_State(int32_t val) {
    bool curr=mcp_relay_read_pin(4), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(5);
}

/**
 * @brief Get the state of outlet 6 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet6_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(5) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of outlet 6 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet6_State(int32_t val) {
    bool curr=mcp_relay_read_pin(5), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(6);
}

/**
 * @brief Get the state of outlet 7 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet7_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(6) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of outlet 7 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet7_State(int32_t val) {
    bool curr=mcp_relay_read_pin(6), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(7);
}

/**
 * @brief Get the state of outlet 8 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_outlet8_State(void *buf, uint8_t *len) {
    int32_t v = mcp_relay_read_pin(7) ? 1 : 0; memcpy(buf,&v,4); *len=4;
}

/**
 * @brief Set the state of outlet 8 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_outlet8_State(int32_t val) {
    bool curr=mcp_relay_read_pin(7), want=(val!=0);
    if(curr!=want) PDU_Display_ToggleRelay(8);
}

/**
 * @brief Get the state of all outlets (relays) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
static void get_allOff(void *buf, uint8_t *len) {
    int32_t v = 0;
    memcpy(buf, &v, 4);
    *len = 4;
}

/**
 * @brief Set the state of all outlets (relays) based on the input value.
 * @param val The desired state (0 or 1).
 */
static void set_allOff(int32_t val) {
    if (val == 1) {
        for (uint8_t ch = 1; ch <= 8; ch++) {
            if (mcp_relay_read_pin(ch-1)) {
                PDU_Display_ToggleRelay(ch);
            }
        }
    }
}