/**
 * @file src/snmp/snmp_voltageMon.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-07
 * 
 * @details Uses Pico SDK ADC for on-die sensor; helper HAL for external rails.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static float g_temp_v = 0.0f;

/**
 * @brief Get the on-die temperature sensor voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_tempSensorVoltage(void *buf, uint8_t *len) {
    adc_select_input(4);
    uint16_t raw = adc_read();
    g_temp_v = (float)raw * 3.0f / 4096.0f;
    *len = (uint8_t)snprintf((char *)buf, 16, "%.5f", g_temp_v);
}

/**
 * @brief Get the on-die temperature sensor temperature in Celsius.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_tempSensorTemperature(void *buf, uint8_t *len) {
    /* use last g_temp_v if available; refresh if zero */
    if (g_temp_v <= 0.0f) {
        adc_select_input(4);
        uint16_t raw = adc_read();
        g_temp_v = (float)raw * 3.0f / 4096.0f;
    }
    float t = 27.0f - (g_temp_v - 0.706f) / 0.001721f;
    *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", t);
}

/**
 * @brief Get the V_SUPPLY rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VSUPPLY(void *buf, uint8_t *len) {
    adc_select_input(V_SUPPLY);
    vTaskDelay(pdMS_TO_TICKS(1));
    float v = get_Voltage(V_SUPPLY) * SUPPLY_DIVIDER;
    *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief Get the V_USB rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VUSB(void *buf, uint8_t *len) {
    adc_select_input(V_USB);
    vTaskDelay(pdMS_TO_TICKS(1));
    float v = get_Voltage(V_USB) * VBUS_DIVIDER;
    *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief Get the V_SUPPLY divider tap voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VSUPPLY_divider(void *buf, uint8_t *len) {
    adc_select_input(V_SUPPLY);
    vTaskDelay(pdMS_TO_TICKS(1));
    float v = get_Voltage(V_SUPPLY); // raw divider tap voltage
    *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief Get the V_USB divider tap voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_VUSB_divider(void *buf, uint8_t *len) {
    adc_select_input(V_USB);
    vTaskDelay(pdMS_TO_TICKS(1));
    float v = get_Voltage(V_USB); // raw divider tap voltage
    *len = (uint8_t)snprintf((char *)buf, 16, "%.3f", v);
}

/**
 * @brief Get the core VREG voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_coreVREG(void *buf, uint8_t *len) {
    uint32_t reg = *((volatile uint32_t *)0x40064000);
    uint32_t vsel = reg & 0xF;
    float v = 0.85f + 0.05f * (float)vsel;
    *len = (uint8_t)snprintf((char *)buf, 16, "%.2f", v);
}

/**
 * @brief Get the core VREG status.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_coreVREG_status(void *buf, uint8_t *len) {
    uint32_t reg = *((volatile uint32_t *)0x40064000);
    const char *s = "Unknown";
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

/**
 * @brief Get the bandgap reference voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_bandgapRef(void *buf, uint8_t *len) { *len = (uint8_t)snprintf((char *)buf, 16, "1.10"); }

/**
 * @brief Get the USB PHY rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_usbPHYrail(void *buf, uint8_t *len) { *len = (uint8_t)snprintf((char *)buf, 16, "1.80"); }

/**
 * @brief Get the IO rail voltage.
 * @param buf Output buffer for ASCII result.
 * @param len Out length (bytes written).
 * @return None
 */
void get_ioRail(void *buf, uint8_t *len) { *len = (uint8_t)snprintf((char *)buf, 16, "3.30"); }
