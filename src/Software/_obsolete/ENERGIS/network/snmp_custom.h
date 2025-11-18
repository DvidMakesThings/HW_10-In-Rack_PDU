/**
 * @file snmp_custom.h
 * @author DvidMakesThings - David Sipos
 * @brief Custom SNMP functions for the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-05-19
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef _SNMP_CUSTOM_H_
#define _SNMP_CUSTOM_H_

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../CONFIG.h"
#include "snmp.h"
#include "snmp_networkCtrl.h"
#include "snmp_outletCtrl.h"
#include "snmp_powerMon.h"
#include "snmp_voltageMon.h"

extern dataEntryType snmpData[];
extern const int32_t maxData;

// Define for using W5500-EVB: H/W Dependency (e.g., LEDs...)
// #define _USE_WIZNET_W5500_EVB_

#define COMMUNITY "public\0"
#define COMMUNITY_SIZE (strlen(COMMUNITY))
#define SNMP_HANDLER "<SNMP Handler>"

/**
 * @brief Initialize the SNMP table with default values.
 *
 * This function initializes the SNMP table with default values for
 * various OIDs. It is called during the initialization of the SNMP agent.
 */
void initTable();

/**
 * @brief Initialize the SNMP agent with the given manager and agent IP addresses.
 *
 * @param managerIP Pointer to the manager IP address.
 * @param agentIP Pointer to the agent IP address.
 */
void initial_Trap(uint8_t *managerIP, uint8_t *agentIP);

//------------------------------------------------------------------------------
// Callbacks for the SNMP agent

/**
 * @brief Get the IP address of the device.
 * @param ptr Pointer to the buffer to store the IP address.
 * @param len Pointer to the length of the IP address.
 */
void get_serialNumber(void *buf, uint8_t *len);

/**
 * @brief Return contact information as an OCTET STRING.
 * @param buf Pointer to the buffer to store the contact information.
 * @param len Pointer to the length of the contact information.
 */
void get_sysContact(void *buf, uint8_t *len);

#endif