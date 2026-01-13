/**
 * @file src/snmp/snmp_outletCtrl.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp03 3. SNMP Agent - Outlet Control
 * @ingroup snmp
 * @brief SNMP outlet control with synchronous operations via SwitchTask.
 * @{
 *
 * @version 2.0.0
 * @date 2025-12-16
 *
 * @details
 * This module provides SNMP GET/SET callbacks for individual outlet control and
 * bulk operations. All operations use the synchronous SwitchTask API v2.0 to
 * ensure deterministic behavior and accurate SNMP responses.
 *
 * Key features:
 * - Individual outlet control (8 channels, numbered 1-8 in SNMP OIDs)
 * - Bulk operations (All ON, All OFF)
 * - Synchronous operations with hardware read-back verification
 * - Direct hardware reads (no cache) for GET operations
 * - Blocking writes with verification for SET operations
 *
 * Channel Indexing:
 * - SNMP OIDs use 1-based indexing (1..8)
 * - Internal SwitchTask API uses 0-based indexing (0..7)
 * - All callbacks handle conversion transparently
 *
 * GET Operations:
 * - Query current outlet state from hardware via Switch_GetStateCompat()
 * - Return 4-byte INTEGER (0=OFF, 1=ON)
 * - Non-blocking with mutex protection
 *
 * SET Operations:
 * - Write desired state via Switch_SetChannelCompat()
 * - Block until hardware write completes
 * - Automatic verification with up to 500ms polling
 * - Error logging for failures
 *
 * @note All functions are RTOS-safe and can be called from SNMP agent context.
 * @note Operations may block for up to 500ms during hardware verification.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_OUTLET_CTRL_H
#define SNMP_OUTLET_CTRL_H

#include "../CONFIG.h"

/** @name Public API
 * @{
 */

/**
 * @brief SNMP getter for outlet 1 state.
 *
 * Queries the current outlet 1 state directly from hardware via SwitchTask API.
 * Returns a 4-byte INTEGER representing the relay state.
 *
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 *
 * @return None
 *
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 * @note Non-blocking operation with mutex protection.
 */
void get_outlet1_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 2 state.
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 * @return None
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 */
void get_outlet2_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 3 state.
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 * @return None
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 */
void get_outlet3_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 4 state.
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 * @return None
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 */
void get_outlet4_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 5 state.
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 * @return None
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 */
void get_outlet5_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 6 state.
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 * @return None
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 */
void get_outlet6_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 7 state.
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 * @return None
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 */
void get_outlet7_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for outlet 8 state.
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 * @return None
 * @note Returns 0 (OFF) or 1 (ON) as 32-bit little-endian integer.
 */
void get_outlet8_State(void *buf, uint8_t *len);

/**
 * @brief SNMP setter for outlet 1 state.
 *
 * Sets the desired outlet 1 state via synchronous SwitchTask API. This operation
 * blocks until hardware write completes and verification succeeds. The function
 * interprets any non-zero value as ON, zero as OFF.
 *
 * @param size Size of incoming SNMP data (bytes to copy from val)
 * @param dataType SNMP data type identifier (unused, validation done by agent)
 * @param val Pointer to incoming SNMP INTEGER value (0=OFF, non-zero=ON)
 *
 * @return None
 *
 * @note Blocking operation with up to 500ms verification polling.
 * @note Logs error if channel index invalid or hardware write fails.
 */
void set_outlet1_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for outlet 2 state.
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (0=OFF, non-zero=ON)
 * @return None
 * @note Blocking operation with hardware verification.
 */
void set_outlet2_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for outlet 3 state.
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (0=OFF, non-zero=ON)
 * @return None
 * @note Blocking operation with hardware verification.
 */
void set_outlet3_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for outlet 4 state.
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (0=OFF, non-zero=ON)
 * @return None
 * @note Blocking operation with hardware verification.
 */
void set_outlet4_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for outlet 5 state.
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (0=OFF, non-zero=ON)
 * @return None
 * @note Blocking operation with hardware verification.
 */
void set_outlet5_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for outlet 6 state.
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (0=OFF, non-zero=ON)
 * @return None
 * @note Blocking operation with hardware verification.
 */
void set_outlet6_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for outlet 7 state.
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (0=OFF, non-zero=ON)
 * @return None
 * @note Blocking operation with hardware verification.
 */
void set_outlet7_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for outlet 8 state.
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (0=OFF, non-zero=ON)
 * @return None
 * @note Blocking operation with hardware verification.
 */
void set_outlet8_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP getter for All ON trigger state.
 *
 * Returns the current value of the All ON control OID. This is a write-oriented
 * control OID and the getter always returns 0.
 *
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 *
 * @return None
 *
 * @note Always returns 0 (trigger OIDs have no persistent state).
 */
void get_allOn_State(void *buf, uint8_t *len);

/**
 * @brief SNMP getter for All OFF trigger state.
 *
 * Returns the current value of the All OFF control OID. This is a write-oriented
 * control OID and the getter always returns 0.
 *
 * @param buf Output buffer (minimum 4 bytes required)
 * @param len Pointer to receive length; always set to 4
 *
 * @return None
 *
 * @note Always returns 0 (trigger OIDs have no persistent state).
 */
void get_allOff_State(void *buf, uint8_t *len);

/**
 * @brief SNMP setter for All ON bulk operation.
 *
 * When set to non-zero, turns all outlets ON via Switch_AllOnCompat(). This is
 * a synchronous blocking operation that does not return until all channels have
 * been switched and verified.
 *
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (non-zero triggers All ON)
 *
 * @return None
 *
 * @note Blocking operation; may take several seconds for all channels.
 * @note Logs warning if any channel fails to switch.
 */
void set_allOn_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief SNMP setter for All OFF bulk operation.
 *
 * When set to non-zero, turns all outlets OFF via Switch_AllOffCompat(). This is
 * a synchronous blocking operation that does not return until all channels have
 * been switched and verified.
 *
 * @param size Size of incoming SNMP data
 * @param dataType SNMP data type identifier
 * @param val Pointer to incoming value (non-zero triggers All OFF)
 *
 * @return None
 *
 * @note Blocking operation; may take several seconds for all channels.
 * @note Logs warning if any channel fails to switch.
 */
void set_allOff_State(int32_t size, uint8_t dataType, void *val);

/** @} */

#endif

/** @} */
