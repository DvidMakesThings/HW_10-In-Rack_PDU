/**
 * @file src/snmp/snmp_custom.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp SNMP Agent
 * @brief SNMP agent implementation for ENERGIS PDU.
 * @{
 *
 * @defgroup snmp01 1. SNMP Agent - ENERGIS OID Table
 * @ingroup snmp
 * @brief Enterprise OID table definition and initialization
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details
 * This module defines the complete SNMP OID (Object Identifier) table for the
 * ENERGIS managed PDU, mapping enterprise-specific OIDs to getter and setter
 * callback functions implemented in domain-specific modules.
 *
 * The OID table includes:
 * - RFC1213 system group (sysDescr, sysObjectID, sysUpTime, etc.)
 * - Network configuration exposure (IP, gateway, subnet, DNS, MAC)
 * - Outlet control with GET/SET operations (8 channels plus bulk operations)
 * - Voltage and temperature monitoring (RP2040 internal rails and project supplies)
 * - Per-outlet power telemetry (voltage, current, power, PF, energy, uptime)
 * - Overcurrent protection status and control
 *
 * All callbacks are RTOS-safe and use appropriate synchronization primitives
 * to access shared resources without causing race conditions or blocking.
 *
 * @note This is a table-only definition file; actual logic resides in domain modules.
 * @note Enterprise OID prefix: 1.3.6.1.4.1.19865
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_CUSTOM_H
#define SNMP_CUSTOM_H

#include "../CONFIG.h"

/** @name Globals
 * @{
 */
/** @var COMMUNITY */
extern const uint8_t COMMUNITY[];
/** @var COMMUNITY_SIZE */
extern const uint8_t COMMUNITY_SIZE;

/* Global OID table + size (consumed by snmp.c) */
/** @var snmpData */
extern snmp_entry_t snmpData[];
/** @var maxData */
extern const int32_t maxData;
/** @} */

/** @name Public API
 * @{
 */

/**
 * @brief Initialize the SNMP OID table.
 *
 * This function is called during system initialization to set up any dynamic
 * entries in the SNMP table. Currently, the table is fully static and this
 * function serves as a placeholder for future initialization requirements.
 *
 * @return None
 *
 * @note Call this before starting the SNMP agent.
 * @note This function is currently a no-op as all table entries are compile-time constants.
 */
void initTable(void);

/**
 * @brief Send SNMP WarmStart trap notification to manager.
 *
 * Sends an SNMP WarmStart trap (generic-trap 1) to notify the management station
 * that the agent has reinitialized. This is typically called once during system
 * boot after network services become available.
 *
 * The trap includes the enterprise OID (1.3.6.1.4.1.19865.1.0) and standard
 * trap metadata but no additional variable bindings.
 *
 * @param managerIP Pointer to 4-byte array containing manager IPv4 address
 * @param agentIP Pointer to 4-byte array containing agent's own IPv4 address
 *
 * @return None
 *
 * @note This function uses the global COMMUNITY string for authentication.
 * @note Trap delivery is best-effort UDP and not guaranteed.
 */
void initial_Trap(uint8_t *managerIP, uint8_t *agentIP);
/** @} */

#endif

/** @} */