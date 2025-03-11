#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include <stdio.h>          // For printf()
#include "hardware/i2c.h"
#include "../drivers/MCP23017_display_driver.h"
#include "../drivers/MCP23017_relay_driver.h"
#include "../network/w5500.h"   
#include "../network/socket.h"    
#include "../CONFIG.h"

// Example handler for the root path
void handle_root_request(http_request_t* request, http_response_t* response);

// i2c_scan_bus: Scans an I2C bus and prints detected devices.
void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name);

//------------------------------------------------------------------------------
// activate_relay: Activates a relay (0-7) and turns on the corresponding LED.
void activate_relay(uint8_t relay_number);

#endif // HELPER_FUNCTIONS_H