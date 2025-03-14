#include "helper_functions.h"

// Example handler for the root path
void handle_root_request(http_request_t* request, http_response_t* response) {
    DEBUG_PRINT("[DEBUG] Handling root request...\n");

    const char* html_response = "<html><body><h1>ENERGIS PDU</h1><p>System is running.</p></body></html>";
    
    // Set the response body and status
    http_response_set_body(response, html_response, strlen(html_response));
    http_response_set_status(response, 200);  // OK status
    
    // Send the response
    http_response_send(response, request->socket);
}



// i2c_scan_bus: Scans an I2C bus and prints detected devices.
void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name) {
    printf("Scanning %s...\n", bus_name);
    bool device_found = false;
    uint8_t dummy = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        int ret = i2c_write_blocking(i2c, addr, &dummy, 1, false);
        if (ret >= 0) {
            printf("Found device at address 0x%02X\n", addr);
            device_found = true;
        }
    }
    if (!device_found) {
        printf("No devices found on %s.\n", bus_name);
    }
}

//------------------------------------------------------------------------------
// activate_relay: Activates a relay (0-7) and turns on the corresponding LED.
void activate_relay(uint8_t relay_number) {
    if (relay_number > 7) {
        printf("Invalid relay number %d. Must be between 0 and 7.\n", relay_number);
        return;
    }
    mcp_relay_write_pin(relay_number, 1);
    mcp_display_write_pin(relay_number, 1);
}