/**
 * @file snmp_voltageMon.c
 * @author DvidMakesThings - David Sipos
 * @defgroup snmp6 6. SNMP Agent - Voltage monitoring
 * @ingroup snmp
 * @brief Custom SNMP agent implementation for ENERGIS PDU voltage monitoring.
 * @{
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_voltageMon.h"

static float latest_temp_voltage = 0.0f;
static float latest_temp_celsius = 0.0f;

// ####################################################################################
//  ADC + Voltage monitoring under .1.3.6.1.4.1.19865.3.X.0
// ####################################################################################

/**
 * @brief SNMP callback: return measured 12V supply voltage.
 */
void get_VSUPPLY(void *buf, uint8_t *len) {
    float v = get_Voltage(V_SUPPLY) * (110000.0f / 10000.0f); 
    *len = snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief SNMP callback: return measured USB 5V voltage.
 */
void get_VUSB(void *buf, uint8_t *len) {
    float v = get_Voltage(V_USB) * (20000.0f / 10000.0f); 
    *len = snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief SNMP callback: return measured 12V supply voltage.
 */
void get_VSUPPLY_divider(void *buf, uint8_t *len) {
    float v = get_Voltage(V_SUPPLY); 
    *len = snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief SNMP callback: return measured USB 5V voltage.
 */
void get_VUSB_divider(void *buf, uint8_t *len) {
    float v = get_Voltage(V_USB); 
    *len = snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief SNMP callback: Return latest die temp sensor voltage as string
 */
void get_tempSensorVoltage(void *buf, uint8_t *len) {
    adc_select_input(4);
    uint16_t raw = adc_read();
    latest_temp_voltage = raw * 3.0f / 4096.0f;
    *len = snprintf((char *)buf, 16, "%.5f", latest_temp_voltage);
}

/**
 * @brief SNMP callback: Return latest die temp sensor temperature as string
 */
void get_tempSensorTemperature(void *buf, uint8_t *len) {
    adc_select_input(4);
    uint16_t raw = adc_read();
    latest_temp_celsius = 27.0f - (latest_temp_voltage - 0.706f) / 0.001721f;
    *len = snprintf((char *)buf, 16, "%.3f", latest_temp_celsius);
}

/**
 * @brief SNMP callback: Return core regulator target voltage (VREG->VSEL).
 *
 * Reads the VREG register at 0x40064000, extracts the VSEL field [3:0],
 * and decodes it into the configured core supply voltage in volts.
 *
 * Voltage range: 0.85 V + (VSEL * 0.05 V).
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_coreVREG(void *buf, uint8_t *len) {
    uint32_t reg = *((volatile uint32_t *)0x40064000);
    uint32_t vsel = reg & 0xF;
    float v = 0.85f + (float)vsel * 0.05f;
    *len = snprintf((char *)buf, 16, "%.2f", v);
}

/**
 * @brief SNMP callback: Return core regulator status flags.
 *
 * Reads the VREG register at 0x40064000, extracts status bits [31:30],
 * and reports them as a human-readable string.
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_coreVREG_status(void *buf, uint8_t *len) {
    uint32_t reg = *((volatile uint32_t *)0x40064000);
    uint32_t status = (reg >> 30) & 0x3;
    const char *s;
    switch (status) {
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
        s = "Unknown";
        break;
    }
    *len = snprintf((char *)buf, 16, "%s", s);
}

/**
 * @brief SNMP callback: Return bandgap reference voltage (constant).
 *
 * The RP2040 uses an internal bandgap reference of ~1.1 V.
 * This value is fixed and not directly measurable via ADC.
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_bandgapRef(void *buf, uint8_t *len) { *len = snprintf((char *)buf, 16, "1.10"); }

/**
 * @brief SNMP callback: Return USB PHY supply rail voltage (constant).
 *
 * The USB transceiver on RP2040 operates from a dedicated 1.8 V domain.
 * This value is fixed by design and not measurable internally.
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_usbPHYrail(void *buf, uint8_t *len) { *len = snprintf((char *)buf, 16, "1.80"); }

/**
 * @brief SNMP callback: Return nominal I/O rail voltage (constant).
 *
 * The RP2040 I/O supply (IOVDD) is typically 3.3 V in standard designs.
 * This agent reports it as a fixed 3.30 V nominal value.
 *
 * @param buf Pointer to output buffer (OCTET STRING).
 * @param len Pointer to output length.
 */
void get_ioRail(void *buf, uint8_t *len) { *len = snprintf((char *)buf, 16, "3.30"); }

/** @} */ // End of snmp6