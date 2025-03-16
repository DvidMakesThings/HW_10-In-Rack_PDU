#include "helper_functions.h"

// i2c_scan_bus: Scans an I2C bus and prints detected devices.
void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name) {
    DEBUG_PRINT("Scanning %s...\n", bus_name);
    bool device_found = false;
    uint8_t dummy = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        int ret = i2c_write_blocking(i2c, addr, &dummy, 1, false);
        if (ret >= 0) {
            DEBUG_PRINT("Found device at address 0x%02X\n\n", addr);
            device_found = true;
        }
    }
    if (!device_found) {
        DEBUG_PRINT("No devices found on %s.\n", bus_name);
    }
}

//------------------------------------------------------------------------------
// activate_relay: Activates a relay (0-7) and turns on the corresponding LED.
void activate_relay(uint8_t relay_number) {
    if (relay_number > 7) {
        ERROR_PRINT("Invalid relay number %d. Must be between 0 and 7.\n", relay_number);
        return;
    }
    mcp_relay_write_pin(relay_number, 1);
    mcp_display_write_pin(relay_number, 1);
}

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
