/**
 * @file snmp_powerMon.h
 * @author DvidMakesThings - David Sipos
 * @brief SNMP power monitoring callbacks (RTOS-safe reads)
 *
 * @version 1.0.0
 * @date 2025-11-07
 * @details Exposes Vrms/Irms/W/PF/kWh/Uptime per channel via SNMP GET callbacks.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SNMP_POWER_MON_H
#define SNMP_POWER_MON_H

#include "../CONFIG.h"

extern void Meter_GetLatest(uint8_t ch, float *v, float *a, float *w, float *pf, float *kwh,
                            uint32_t *uptime);

/**
 * @brief Detailed description
 *
 * @param buf  Destination buffer for ASCII float/u32.
 * @param len  Out length.
 * @return None
 */
void get_power_0_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_0_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_0_MEAS_WATT(void *buf, uint8_t *len);
void get_power_0_MEAS_PF(void *buf, uint8_t *len);
void get_power_0_MEAS_KWH(void *buf, uint8_t *len);
void get_power_0_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_1_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_1_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_1_MEAS_WATT(void *buf, uint8_t *len);
void get_power_1_MEAS_PF(void *buf, uint8_t *len);
void get_power_1_MEAS_KWH(void *buf, uint8_t *len);
void get_power_1_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_2_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_2_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_2_MEAS_WATT(void *buf, uint8_t *len);
void get_power_2_MEAS_PF(void *buf, uint8_t *len);
void get_power_2_MEAS_KWH(void *buf, uint8_t *len);
void get_power_2_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_3_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_3_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_3_MEAS_WATT(void *buf, uint8_t *len);
void get_power_3_MEAS_PF(void *buf, uint8_t *len);
void get_power_3_MEAS_KWH(void *buf, uint8_t *len);
void get_power_3_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_4_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_4_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_4_MEAS_WATT(void *buf, uint8_t *len);
void get_power_4_MEAS_PF(void *buf, uint8_t *len);
void get_power_4_MEAS_KWH(void *buf, uint8_t *len);
void get_power_4_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_5_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_5_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_5_MEAS_WATT(void *buf, uint8_t *len);
void get_power_5_MEAS_PF(void *buf, uint8_t *len);
void get_power_5_MEAS_KWH(void *buf, uint8_t *len);
void get_power_5_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_6_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_6_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_6_MEAS_WATT(void *buf, uint8_t *len);
void get_power_6_MEAS_PF(void *buf, uint8_t *len);
void get_power_6_MEAS_KWH(void *buf, uint8_t *len);
void get_power_6_MEAS_UPTIME(void *buf, uint8_t *len);

void get_power_7_MEAS_VOLTAGE(void *buf, uint8_t *len);
void get_power_7_MEAS_CURRENT(void *buf, uint8_t *len);
void get_power_7_MEAS_WATT(void *buf, uint8_t *len);
void get_power_7_MEAS_PF(void *buf, uint8_t *len);
void get_power_7_MEAS_KWH(void *buf, uint8_t *len);
void get_power_7_MEAS_UPTIME(void *buf, uint8_t *len);

#endif
