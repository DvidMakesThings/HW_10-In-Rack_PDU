/**
 * @file snmp_networkCtrl.h
 * @author DvidMakesThings - David Sipos
 * @brief Custom SNMP functions for the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-05-19
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef _SNMP_NETWORK_CTRL_H_
#define _SNMP_NETWORK_CTRL_H_

#include "CONFIG.h"
#include "snmp.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#endif