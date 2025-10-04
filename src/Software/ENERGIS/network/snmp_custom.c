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

//------------------------------------------------------------------------------
// the full table of OID support:
#define SNMP_MAX_OID_LEN 16


// clang-format off
dataEntryType snmpData[] = {
    // Standard system‐group stuff
    {8,  {0x2b, 6, 1, 2, 1, 1, 1, 0}, SNMPDTYPE_OCTET_STRING, 29,   {"ENERGIS 8 CHANNEL MANAGED PDU"}, NULL, NULL},
    {8,  {0x2b, 6, 1, 2, 1, 1, 2, 0}, SNMPDTYPE_OBJ_ID, 8,          {"\x2b\x06\x01\x02\x01\x01\x02\x00"}, NULL, NULL},
    {8,  {0x2b, 6, 1, 2, 1, 1, 3, 0}, SNMPDTYPE_TIME_TICKS, 0,      {""}, currentUptime, NULL},
    {8,  {0x2b, 6, 1, 2, 1, 1, 4, 0}, SNMPDTYPE_OCTET_STRING, 64,   {""}, get_sysContact, NULL},
    {8,  {0x2b, 6, 1, 2, 1, 1, 5, 0}, SNMPDTYPE_OCTET_STRING, 15,   {""}, get_serialNumber, NULL},
    {8,  {0x2b, 6, 1, 2, 1, 1, 6, 0}, SNMPDTYPE_OCTET_STRING, 4,    {"Wien"}, NULL, NULL},
    {8,  {0x2b, 6, 1, 2, 1, 1, 7, 0}, SNMPDTYPE_INTEGER, 4,         {""}, NULL, NULL},

    // “long-length” tests
    {10, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 30,  {"long-length OID Test #1"}, NULL, NULL},
    {10, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0xad, 0x42, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 35,  {"long-length OID Test #2"}, NULL, NULL},
    {10, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0xad, 0x42, 0x02, 0x00}, SNMPDTYPE_OBJ_ID, 10,        {"\x2b\x06\x01\x04\x01\x81\xad\x42\x02\x00"}, NULL, NULL},

    // Network configuration under .1.3.6.1.4.1.19865.4.X.0
    //    a) ip address:  .4.1.0
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkIP, NULL},
    //    b) subnet mask: .4.2.0
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkMask, NULL},
    //    c) gateway:     .4.3.0
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkGateway, NULL},
    //    d) DNS server:  .4.4.0
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x04, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_networkDNS, NULL},

    // Outlet state control
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x01, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet1_State, set_outlet1_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x02, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet2_State, set_outlet2_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x03, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet3_State, set_outlet3_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x04, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet4_State, set_outlet4_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x05, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet5_State, set_outlet5_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x06, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet6_State, set_outlet6_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x07, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet7_State, set_outlet7_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x08, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_outlet8_State, set_outlet8_State},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x09, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_allOff, set_allOff},
    {11, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x02, 0x0A, 0x00}, SNMPDTYPE_INTEGER, 4, {""}, get_allOn, set_allOn},

    // ADC + Voltage monitoring under .1.3.6.1.4.1.19865.3.X.0
    //    a) die sensor voltage:     .3.1.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_tempSensorVoltage, NULL},
    //    b) die sensor temperature: .3.2.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_tempSensorTemperature, NULL},
    //    c) 12V PSU voltage: .3.3.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VSUPPLY, NULL},
    //    d) 5V USB voltage: .3.4.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VUSB, NULL},
    //    e) 12V PSU divider voltage: .3.5.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VSUPPLY_divider, NULL},
    //    f) 5V USB divider voltage: .3.6.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_VUSB_divider, NULL},
    //    g) Core VREG target voltage: .3.7.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x07, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_coreVREG, NULL},
    //    h) Core VREG status flags:  .3.8.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x08, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_coreVREG_status, NULL},
    //    i) Bandgap reference (const): .3.9.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x09, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_bandgapRef, NULL},
    //    j) USB PHY rail (const): .3.10.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x0A, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_usbPHYrail, NULL},
    //    k) IO rail nominal (const): .3.11.0
    {11, {0x2b, 0x06, 0x01, 0x04, 0x01, 0x81, 0x9b, 0x19, 0x03, 0x0B, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_ioRail, NULL},

    // Power monitoring under .1.3.6.1.4.1.19865.5.<ch>.<metric>.0
    // Metrics: 1=Voltage, 2=Current, 3=Power, 4=PowerFactor, 5=kWh, 6=Uptime

    // ================= Channel 1 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x01, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_0_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x01, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_0_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x01, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_0_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x01, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_0_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x01, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_0_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x01, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_0_MEAS_UPTIME, NULL},

    // ================= Channel 2 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x02, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_1_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x02, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_1_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x02, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_1_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x02, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_1_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x02, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_1_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x02, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_1_MEAS_UPTIME, NULL},

    // ================= Channel 3 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x03, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_2_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x03, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_2_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x03, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_2_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x03, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_2_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x03, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_2_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x03, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_2_MEAS_UPTIME, NULL},

    // ================= Channel 4 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x04, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_3_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x04, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_3_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x04, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_3_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x04, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_3_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x04, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_3_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x04, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_3_MEAS_UPTIME, NULL},

    // ================= Channel 5 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x05, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_4_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x05, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_4_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x05, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_4_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x05, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_4_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x05, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_4_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x05, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_4_MEAS_UPTIME, NULL},

    // ================= Channel 6 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x06, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_5_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x06, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_5_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x06, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_5_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x06, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_5_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x06, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_5_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x06, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_5_MEAS_UPTIME, NULL},

    // ================= Channel 7 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x07, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_6_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x07, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_6_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x07, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_6_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x07, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_6_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x07, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_6_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x07, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_6_MEAS_UPTIME, NULL},

    // ================= Channel 8 =================
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x08, 0x01, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_7_MEAS_VOLTAGE, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x08, 0x02, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_7_MEAS_CURRENT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x08, 0x03, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_7_MEAS_WATT, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x08, 0x04, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_7_MEAS_PF, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x08, 0x05, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_7_MEAS_KWH, NULL},
    {12, {0x2b, 6, 1, 4, 1, 0x81, 0x9b, 0x19, 0x05, 0x08, 0x06, 0x00}, SNMPDTYPE_OCTET_STRING, 16, {""}, get_power_7_MEAS_UPTIME, NULL},

};

// clang-format on

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

// ####################################################################################
//  Callbacks for the SNMP agent
// ####################################################################################

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
