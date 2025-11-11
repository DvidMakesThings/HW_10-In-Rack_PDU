/**
 * @file src/snmp/snmp_custom.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp SNMP Agent
 * @brief SNMP agent implementation for ENERGIS PDU.
 * @{
 *
 * @defgroup snmp01 1. SNMP Agent - ENERGIS OID table (RTOS)
 * @ingroup snmp
 * @brief Defines enterprise OIDs and binds to RTOS-safe callbacks.
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 * 
 * @details Table-only file; logic lives in per-domain modules.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_CUSTOM_H
#define SNMP_CUSTOM_H

#include "../CONFIG.h"

extern const uint8_t COMMUNITY[];
extern const uint8_t COMMUNITY_SIZE;

/* Global OID table + size (consumed by snmp.c) */
extern snmp_entry_t snmpData[];
extern const int32_t maxData;

/**
 * @brief Detailed description
 *
 * @param None
 * @param None
 * @return None
 * @note Initializes constant entries as needed.
 */
void initTable(void);

/**
 * @brief Detailed description
 *
 * @param managerIP Pointer to 4-byte manager IPv4
 * @param agentIP   Pointer to 4-byte agent IPv4
 * @return None
 * @note Sends a WarmStart trap on boot.
 */
void initial_Trap(uint8_t *managerIP, uint8_t *agentIP);

#endif

/** @} */