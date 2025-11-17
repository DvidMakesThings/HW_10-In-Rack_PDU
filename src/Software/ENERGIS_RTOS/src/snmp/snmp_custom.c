/**
 * @file src/snmp/snmp_custom.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-07
 * 
 * @details Table-only file; logic lives in per-domain modules.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

const uint8_t COMMUNITY[] = "public";
const uint8_t COMMUNITY_SIZE = (uint8_t)(sizeof(COMMUNITY) - 1);

static void get_sysContact(void *buf, uint8_t *len) {
    const char *s = "dvidmakesthings@gmail.com";
    uint8_t L = (uint8_t)strlen(s);
    memcpy(buf, s, L);
    *len = L;
}

static void get_sysName(void *buf, uint8_t *len) {
    const char *s = "ENERGIS 8 CHANNEL MANAGED PDU";
    uint8_t L = (uint8_t)strlen(s);
    memcpy(buf, s, L);
    *len = L;
}

static void get_sysLocation(void *buf, uint8_t *len) {
    const char *s = "Wien";
    uint8_t L = (uint8_t)strlen(s);
    memcpy(buf, s, L);
    *len = L;
}

static void get_sysSN(void *buf, uint8_t *len) {
    const char *s = DEFAULT_SN;
    uint8_t L = (uint8_t)strlen(s);
    memcpy(buf, s, L);
    *len = L;
}

/* clang-format off */
snmp_entry_t snmpData[] = {
    /* RFC1213 system group (.1.3.6.1.2.1.1.X.0) */
    {8,  {0x2b,6,1,2,1,1,1,0},  SNMPDTYPE_OCTET_STRING, 0, {""}, get_sysName,      NULL}, /* sysDescr */
    {8,  {0x2b,6,1,2,1,1,2,0},  SNMPDTYPE_OBJ_ID,       8,  {"\x2b\x06\x01\x04\x01\x81\x9b\x19"}, NULL, NULL}, /* sysObjectID = 1.3.6.1.4.1.19865 */
    {8,  {0x2b,6,1,2,1,1,3,0},  SNMPDTYPE_TIME_TICKS,   4,  {""}, SNMP_GetUptime,  NULL}, /* sysUpTime */
    {8,  {0x2b,6,1,2,1,1,4,0},  SNMPDTYPE_OCTET_STRING, 0,  {""}, get_sysContact,  NULL}, /* sysContact */
    {8,  {0x2b,6,1,2,1,1,5,0},  SNMPDTYPE_OCTET_STRING, 0,  {""}, get_sysSN,     NULL}, /* sysSN */
    {8,  {0x2b,6,1,2,1,1,6,0},  SNMPDTYPE_OCTET_STRING, 0,  {""}, get_sysLocation, NULL}, /* sysLocation */
    {8,  {0x2b,6,1,2,1,1,7,0},  SNMPDTYPE_INTEGER,      4,  { .intval = 72 },      NULL, NULL}, /* sysServices */

    // “long-length” tests
    {10, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 30,  {"long-length OID Test #1"}, NULL, NULL},
    {10, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0xad, 0x42, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 35,  {"long-length OID Test #2"}, NULL, NULL},
    {10, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0xad, 0x42, 0x02, 0x00}, SNMPDTYPE_OBJ_ID, 10,        {"\x2b\x06\x01\x04\x01\x81\xad\x42\x02\x00"}, NULL, NULL},

    /* network (.1.3.6.1.4.1.19865.4.X.0) */
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x01,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkIP, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x02,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkMask, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x03,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkGateway, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x04,0x04,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkDNS, NULL},

    /* outlet control (.1.3.6.1.4.1.19865.2.N.0) */
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x01,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet1_State, (void (*)(int32_t))set_outlet1_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x02,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet2_State, (void (*)(int32_t))set_outlet2_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x03,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet3_State, (void (*)(int32_t))set_outlet3_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x04,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet4_State, (void (*)(int32_t))set_outlet4_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x05,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet5_State, (void (*)(int32_t))set_outlet5_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x06,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet6_State, (void (*)(int32_t))set_outlet6_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x07,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet7_State, (void (*)(int32_t))set_outlet7_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x08,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet8_State, (void (*)(int32_t))set_outlet8_State},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x09,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_allOff, (void (*)(int32_t))set_allOff},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x02,0x0A,0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_allOn,  (void (*)(int32_t))set_allOn},

    /* voltage/temperature (.1.3.6.1.4.1.19865.3.X.0) */
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x01,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_tempSensorVoltage, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x02,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_tempSensorTemperature, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x03,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VSUPPLY, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x04,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VUSB, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x05,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VSUPPLY_divider, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x06,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VUSB_divider, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x07,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_coreVREG, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x08,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_coreVREG_status, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x09,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_bandgapRef, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x0A,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_usbPHYrail, NULL},
    {11, {0x2b,6,1,4,1,0x81,0x9b,0x19,0x03,0x0B,0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_ioRail, NULL},

    /* power (.1.3.6.1.4.1.19865.5.<ch>.<metric>.0) */
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x01,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_0_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x01,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_0_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x01,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_0_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x01,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_0_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x01,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_0_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x01,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_0_MEAS_UPTIME,NULL},

    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x02,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_1_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x02,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_1_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x02,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_1_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x02,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_1_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x02,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_1_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x02,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_1_MEAS_UPTIME,NULL},

    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x03,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_2_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x03,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_2_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x03,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_2_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x03,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_2_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x03,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_2_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x03,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_2_MEAS_UPTIME,NULL},

    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x04,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_3_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x04,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_3_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x04,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_3_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x04,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_3_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x04,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_3_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x04,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_3_MEAS_UPTIME,NULL},

    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x05,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_4_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x05,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_4_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x05,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_4_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x05,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_4_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x05,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_4_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x05,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_4_MEAS_UPTIME,NULL},

    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x06,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_5_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x06,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_5_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x06,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_5_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x06,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_5_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x06,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_5_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x06,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_5_MEAS_UPTIME,NULL},

    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x07,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_6_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x07,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_6_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x07,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_6_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x07,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_6_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x07,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_6_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x07,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_6_MEAS_UPTIME,NULL},

    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x08,0x01,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_7_MEAS_VOLTAGE,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x08,0x02,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_7_MEAS_CURRENT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x08,0x03,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_7_MEAS_WATT,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x08,0x04,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_7_MEAS_PF,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x08,0x05,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_7_MEAS_KWH,NULL},
    {12,{0x2b,6,1,4,1,0x81,0x9b,0x19,0x05,0x08,0x06,0x00},SNMPDTYPE_OCTET_STRING,16,{""},get_power_7_MEAS_UPTIME,NULL},
    
};
/* clang-format on */

const int32_t maxData = (int32_t)(sizeof(snmpData) / sizeof(snmpData[0]));

void initTable(void) {
    /* Nothing dynamic to initialize right now.
     */
}

void initial_Trap(uint8_t *managerIP, uint8_t *agentIP) {
    /* enterprise OID = 1.3.6.1.4.1.19865.1.0 (example) */
    snmp_entry_t enterprise_oid = {
        .oidlen = 10,
        .oid = {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x01, 0x00},
        .dataType = SNMPDTYPE_OBJ_ID,
        .dataLen = 10,
        .u = {.octetstring = "\x2b\x06\x01\x04\x01\x81\x9b\x19\x01\x00"},
        .getfunction = NULL,
        .setfunction = NULL};

    /* Send a warmStart trap with no extra var-binds */
    (void)SNMP_SendTrap(managerIP, agentIP, (int8_t *)COMMUNITY, enterprise_oid, SNMPTRAP_WARMSTART,
                        0, 0);
}
