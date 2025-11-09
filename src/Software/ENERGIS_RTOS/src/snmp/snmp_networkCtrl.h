/**
 * @file snmp_networkCtrl.h
 * @author DvidMakesThings - David Sipos
 * @brief SNMP network configuration getters (RTOS)
 *
 * @version 1.0.0
 * @date 2025-11-07
 * @details Formats wiz_NetInfo fields into dotted strings for SNMP GETs.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_NETWORK_CTRL_H
#define SNMP_NETWORK_CTRL_H

#include "../CONFIG.h"

/**
 * @brief Format an IP address as a string.
 * @param ptr Pointer to buffer to store the IP string.
 * @param ip Array containing the 4 bytes of the network address.
 * @return Requested formatted string.
 */
void get_networkIP(void *ptr, uint8_t *len);

/**
 * @brief Get the network subnet mask as a string.
 * @param ptr Pointer to buffer to store the subnet mask string.
 * @param len Pointer to variable to store the length of the string.
 * @return Subnet mask string
 */
void get_networkMask(void *ptr, uint8_t *len);

/**
 * @brief Get the network gateway address as a string.
 * @param ptr Pointer to buffer to store the gateway address string.
 * @param len Pointer to variable to store the length of the string.
 * @return Gateway address string
 */
void get_networkGateway(void *ptr, uint8_t *len);

/**
 * @brief Get the network DNS server address as a string.
 * @param ptr Pointer to buffer to store the DNS server address string.
 * @param len Pointer to variable to store the length of the string.
 * @return DNS server address string
 */
void get_networkDNS(void *ptr, uint8_t *len);

#endif
