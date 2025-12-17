/**
 * @file src/snmp/snmp_outletCtrl.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp03 3. SNMP Agent - Outlet control (RTOS)
 * @ingroup snmp
 * @brief Outlet state GET/SET callbacks that talk to SwitchTask (sole MCP owner).
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details Uses RTOS-safe SwitchTask API: query current state and request changes.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_OUTLET_CTRL_H
#define SNMP_OUTLET_CTRL_H

#include "../CONFIG.h"

/**
 * @brief Get callback for CH1
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet1_State(void *buf, uint8_t *len);

/**
 * @brief Get callback for CH2
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet2_State(void *buf, uint8_t *len);

/**
 * @brief Get callback for CH3
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet3_State(void *buf, uint8_t *len);

/**
 * @brief Get callback for CH4
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet4_State(void *buf, uint8_t *len);

/**
 * @brief Get callback for CH5
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet5_State(void *buf, uint8_t *len);

/**
 * @brief Get callback for CH6
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet6_State(void *buf, uint8_t *len);

/**
 * @brief Get callback for CH7
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet7_State(void *buf, uint8_t *len);

/**
 * @brief Get callback for CH8
 *
 * @param buf  Destination buffer for SNMP OCTET/INTEGER serialization..
 * @param len  In: max length (not used here). Out: produced length (getters).
 * @return None
 * @note  All getters write ASCII "0" or "1" into buf and set *len accordingly.
 */
void get_outlet8_State(void *buf, uint8_t *len);

/**
 * @brief Set callback for CH1
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet1_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief Set callback for CH2
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet2_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief Set callback for CH3
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet3_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief Set callback for CH4
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet4_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief Set callback for CH5
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet5_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief Set callback for CH6
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet6_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief Set callback for CH7
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet7_State(int32_t size, uint8_t dataType, void *val);

/**
 * @brief Set callback for CH8
 *
 * @param size  Size of incoming data (not used here).
 * @param dataType  Data type of incoming data (not used here).
 * @param val  Pointer to incoming value (expects 4-byte INTEGER with 0=OFF, nonzero=ON).
 * @return None
 */
void set_outlet8_State(int32_t size, uint8_t dataType, void *val);

/** @brief SNMP GET callback for ALL ON
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_allOn_State(void *buf, uint8_t *len);

/** @brief SNMP GET callback for ALL OFF
 * @param buf  Output buffer for GET.
 * @param len  Out length for GET.
 * @return None
 */
void get_allOff_State(void *buf, uint8_t *len);

/** @brief SNMP SET callback for ALL ON
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_allOn_State(int32_t size, uint8_t dataType, void *val);

/** @brief SNMP SET callback for ALL OFF
 * @param u32  Nonzero -> ON, zero -> OFF.
 * @return None
 */
void set_allOff_State(int32_t size, uint8_t dataType, void *val);

#endif

/** @} */
