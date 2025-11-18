/**
 * @file snmp_voltageMon.h
 * @author DvidMakesThings - David Sipos
 * @brief Custom SNMP functions for the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-05-19
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef _SNMP_VOLTAGE_MON_H_
#define _SNMP_VOLTAGE_MON_H_

#include "CONFIG.h"
#include "snmp.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ####################################################################################
//  ADC + Voltage monitoring under .1.3.6.1.4.1.19865.3.X.0
// ####################################################################################

/**
 * @brief SNMP callback: Return latest die temp sensor voltage as string
 */
void get_tempSensorVoltage(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: Return latest die temp sensor temperature as string
 */
void get_tempSensorTemperature(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: return measured 12V supply voltage.
 */
void get_VSUPPLY(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: return measured USB 5V voltage.
 */
void get_VUSB(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: return measured 12V supply voltage.
 */
void get_VSUPPLY_divider(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: return measured USB 5V voltage.
 */
void get_VUSB_divider(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: Return core regulator target voltage (VREG->VSEL).
 *
 * Reads the VREG register at 0x40064000, extracts the VSEL field [3:0],
 * and decodes it into the configured core supply voltage in volts.
 *
 * Voltage range: 0.85 V + (VSEL * 0.05 V).
 *
 * Example:
 *   VSEL = 0x4 → 1.05 V
 *   VSEL = 0x7 → 1.20 V
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_coreVREG(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: Return core regulator status flags.
 *
 * Reads the VREG register at 0x40064000, extracts status bits [31:30],
 * and reports them as a human-readable string.
 *
 * Status encoding:
 *   0 → "OK"
 *   1 → "Overload"
 *   2 → "Hi-Z"
 *   3 → "Unknown"
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_coreVREG_status(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: Return bandgap reference voltage (constant).
 *
 * The RP2040 uses an internal bandgap reference of ~1.1 V.
 * This value is fixed and not directly measurable via ADC.
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_bandgapRef(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: Return USB PHY supply rail voltage (constant).
 *
 * The USB transceiver on RP2040 operates from a dedicated 1.8 V domain.
 * This value is fixed by design and not measurable internally.
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_usbPHYrail(void *buf, uint8_t *len);

/**
 * @brief SNMP callback: Return nominal I/O rail voltage (constant).
 *
 * The RP2040 I/O supply (IOVDD) is typically 3.3 V in standard designs.
 * This agent reports it as a fixed 3.30 V nominal value.
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_ioRail(void *buf, uint8_t *len);

#endif