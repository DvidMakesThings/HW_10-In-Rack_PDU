/**
 * @file helper_functions.c
 * @author David Sipos
 * @brief Helper functions for the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "helper_functions.h"

/**
 * @brief Scans an I2C bus for devices and prints their addresses.
 * @param i2c The I2C instance to scan.
 * @param bus_name The name of the bus for logging purposes.
 */
void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name) {
    DEBUG_PRINT("Scanning %s...\n", bus_name);
    bool device_found = false;
    uint8_t dummy = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        int ret = i2c_write_blocking(i2c, addr, &dummy, 1, false);
        if (ret >= 0) {
            DEBUG_PRINT("Found device at address 0x%02X\n", addr);
            device_found = true;
        }
    }
    if (!device_found) {
        DEBUG_PRINT("No devices found on %s.\n", bus_name);
    }
}

/**
 * @brief Returns the HTML content for a given request.
 * @param request The HTTP request string.
 * @return The HTML content as a string.
 */
const char *get_page_content(const char *request) {
    if (strstr(request, "GET /settings.html"))
        return settings_html;
    if (strstr(request, "GET /user_manual.html"))
        return user_manual_html;
    if (strstr(request, "GET /programming_manual.html"))
        return programming_manual_html;
    if (strstr(request, "GET /help.html"))
        return help_html;
    return control_html; // Default page
}