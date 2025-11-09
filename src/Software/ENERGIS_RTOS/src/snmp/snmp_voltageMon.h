/**
 * @file snmp_voltageMon.h
 * @author DvidMakesThings - David Sipos
 * @brief SNMP voltage/temperature callbacks (RTOS-safe)
 *
 * @version 1.0.0
 * @date 2025-11-07
 * @details Returns ADC-derived rails and internal temperature; plus constants.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_VOLTAGE_MON_H
#define SNMP_VOLTAGE_MON_H

#include "../CONFIG.h"

/**
 * @brief Get the on-die temperature sensor voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_tempSensorVoltage(void *buf, uint8_t *len);

/**
 * @brief Get the on-die temperature sensor temperature in Celsius.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_tempSensorTemperature(void *buf, uint8_t *len);

/**
 * @brief Get the V_SUPPLY rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VSUPPLY(void *buf, uint8_t *len);

/**
 * @brief Get the V_USB rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VUSB(void *buf, uint8_t *len);

/**
 * @brief Get the V_SUPPLY divider tap voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VSUPPLY_divider(void *buf, uint8_t *len);

/**
 * @brief Get the V_USB divider tap voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VUSB_divider(void *buf, uint8_t *len);

/**
 * @brief Get the core VREG voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_coreVREG(void *buf, uint8_t *len);

/**
 * @brief Get the core VREG status.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_coreVREG_status(void *buf, uint8_t *len);

/**
 * @brief Get the bandgap reference voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_bandgapRef(void *buf, uint8_t *len);

/**
 * @brief Get the USB PHY rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_usbPHYrail(void *buf, uint8_t *len);

/**
 * @brief Get the IO rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_ioRail(void *buf, uint8_t *len);

#endif
