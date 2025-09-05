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

#include "snmp.h"

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
 * @brief SNMP callback: Return latest die temp sensor voltage as string
 */
void get_tempSensorVoltage(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: Return latest die temp sensor temperature as string
 */
void get_tempSensorTemperature(void *buf, uint8_t *len);

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

/**
 * @brief Get the IP address of the device.
 * @param ptr Pointer to the buffer to store the IP address.
 * @param len Pointer to the length of the IP address.
 */
void get_networkIP(void *ptr, uint8_t *len);

/**
 * @brief Get the subnet mask of the device.
 * @param ptr Pointer to the buffer to store the subnet mask.
 * @param len Pointer to the length of the subnet mask.
 */
void get_networkMask(void *ptr, uint8_t *len);

/**
 * @brief Get the gateway address of the device.
 * @param ptr Pointer to the buffer to store the gateway address.
 * @param len Pointer to the length of the gateway address.
 */
void get_networkGateway(void *ptr, uint8_t *len);

/**
 * @brief Get the DNS server address of the device.
 * @param ptr Pointer to the buffer to store the DNS server address.
 * @param len Pointer to the length of the DNS server address.
 */
void get_networkDNS(void *ptr, uint8_t *len);

//------------------------------------------------------------------------------
// Outlet control callbacks: read the state of the relay and set it

/**
 * @brief Get the state of outlet 1 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet1_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 1 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet1_State(int32_t val);

/**
 * @brief Get the state of outlet 2 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet2_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 2 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet2_State(int32_t val);

/**
 * @brief Get the state of outlet 3 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet3_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 3 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet3_State(int32_t val);

/**
 * @brief Get the state of outlet 4 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet4_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 4 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet4_State(int32_t val);

/**
 * @brief Get the state of outlet 5 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet5_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 5 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet5_State(int32_t val);

/**
 * @brief Get the state of outlet 6 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet6_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 6 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet6_State(int32_t val);

/**
 * @brief Get the state of outlet 7 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet7_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 7 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet7_State(int32_t val);

/**
 * @brief Get the state of outlet 8 (relay) as an integer.
 * @param buf Pointer to the buffer to store the state.
 * @param len Pointer to the length of the state.
 */
void get_outlet8_State(void *buf, uint8_t *len);

/**
 * @brief Set the state of outlet 8 (relay) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_outlet8_State(int32_t val);

/**
 * @brief Set the state of all outlets (relays) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_allOff(int32_t val);

/**
 * @brief Set the state of all outlets (relays) based on the input value.
 * @param val The desired state (0 or 1).
 */
void set_allOn(int32_t val);

/**
 * @brief Read all 8 relay states and return as a bitmask.
 *
 * Bit mapping (LSB first):
 *   bit0→CH1 … bit7→CH8. 1=ON, 0=OFF.
 * Written as SNMP INTEGER (4 bytes); mask is in the low byte.
 *
 * @param buf Output buffer (>=4 bytes).
 * @param len Output length (set to 4).
 * @return void
 */
void get_allState(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for “all off” OID. Returns the same bitmask as get_allState().
 * @param buf Output buffer.
 * @param len Output length.
 * @return void
 */
void get_allOff(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for “all on” OID. Returns the same bitmask as get_allState().
 * @param buf Output buffer.
 * @param len Output length.
 * @return void
 */
void get_allOn(void *buf, uint8_t *len);

#endif