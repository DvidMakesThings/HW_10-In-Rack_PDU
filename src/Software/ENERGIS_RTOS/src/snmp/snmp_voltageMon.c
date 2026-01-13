/**
 * @file src/snmp/snmp_voltageMon.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details
 * Implementation of SNMP voltage monitoring callbacks. All measurements are read
 * from MeterTask's system telemetry cache (non-blocking) or directly from RP2040
 * hardware registers (VREG status). Returns zeros or default values if telemetry
 * is unavailable.
 *
 * Temperature and voltage measurements use the RP2040 ADC with appropriate
 * conversion formulas. Fixed voltage rails return constant strings for efficiency.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static float g_temp_v = 0.0f;

void get_tempSensorVoltage(void *buf, uint8_t *len) {
    system_telemetry_t sys = {0};
    /* Read cached system telemetry */
    if (MeterTask_GetSystemTelemetry(&sys)) {
        /* Convert raw ADC count to voltage */
        float v = (float)sys.raw_temp * (ADC_VREF / ADC_MAX);
        *len = (uint8_t)snprintf((char *)buf, 16, "%.5f", v);
#ifdef g_temp_v
        g_temp_v = v;
#endif
    } else {
        /* Return zero if telemetry unavailable */
        *len = (uint8_t)snprintf((char *)buf, 16, "%.5f", 0.0f);
    }
}

void get_tempSensorTemperature(void *buf, uint8_t *len) {
    system_telemetry_t sys = {0};
    /* Read cached system telemetry */
    if (MeterTask_GetSystemTelemetry(&sys)) {
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", sys.die_temp_c);
#ifdef g_temp_v
        g_temp_v = (float)sys.raw_temp * (ADC_VREF / ADC_MAX);
#endif
    } else {
        /* Return zero if telemetry unavailable */
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", 0.0f);
    }
}

void get_VSUPPLY(void *buf, uint8_t *len) {
    system_telemetry_t sys = {0};
    /* Read cached system telemetry */
    if (MeterTask_GetSystemTelemetry(&sys)) {
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", sys.vsupply_volts);
    } else {
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", 0.0f);
    }
}

void get_VUSB(void *buf, uint8_t *len) {
    system_telemetry_t sys = {0};
    /* Read cached system telemetry */
    if (MeterTask_GetSystemTelemetry(&sys)) {
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", sys.vusb_volts);
    } else {
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", 0.0f);
    }
}

void get_VSUPPLY_divider(void *buf, uint8_t *len) {
    system_telemetry_t sys = {0};
    /* Read cached system telemetry */
    if (MeterTask_GetSystemTelemetry(&sys)) {
        /* Convert raw ADC count to voltage at divider tap */
        float vtap = (float)sys.raw_vsupply * (ADC_VREF / ADC_MAX);
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", vtap);
    } else {
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", 0.0f);
    }
}

void get_VUSB_divider(void *buf, uint8_t *len) {
    system_telemetry_t sys = {0};
    /* Read cached system telemetry */
    if (MeterTask_GetSystemTelemetry(&sys)) {
        /* Convert raw ADC count to voltage at divider tap */
        float vtap = (float)sys.raw_vusb * (ADC_VREF / ADC_MAX);
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", vtap);
    } else {
        *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", 0.0f);
    }
}

void get_coreVREG(void *buf, uint8_t *len) {
    /* Read VREG register directly (RP2040 address 0x40064000) */
    uint32_t reg = *((volatile uint32_t *)0x40064000);
    uint32_t vsel = reg & 0xF;
    /* Calculate voltage: V = 0.85 + 0.05 * VSEL */
    float v = 0.85f + 0.05f * (float)vsel;
    *len = (uint8_t)snprintf((char *)buf, 16, "%.2f", v);
}

void get_coreVREG_status(void *buf, uint8_t *len) {
    /* Read VREG register directly */
    uint32_t reg = *((volatile uint32_t *)0x40064000);
    const char *s = "Unknown";
    /* Decode status from bits[31:30] */
    switch ((reg >> 30) & 0x3) {
    case 0:
        s = "OK";
        break;
    case 1:
        s = "Overload";
        break;
    case 2:
        s = "Hi-Z";
        break;
    default:
        break;
    }
    *len = (uint8_t)snprintf((char *)buf, 16, "%s", s);
}

void get_bandgapRef(void *buf, uint8_t *len) { *len = (uint8_t)snprintf((char *)buf, 16, "1.10"); }

void get_usbPHYrail(void *buf, uint8_t *len) { *len = (uint8_t)snprintf((char *)buf, 16, "1.80"); }

void get_ioRail(void *buf, uint8_t *len) { *len = (uint8_t)snprintf((char *)buf, 16, "3.30"); }
