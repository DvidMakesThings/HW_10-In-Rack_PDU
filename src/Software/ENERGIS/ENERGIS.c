/**
 * @file main.c
 * @author David Sipos
 * @brief 
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */

#include "pico/stdlib.h"
#include "hardware/clocks.h"  // Required for set_sys_clock_khz()
#include "hardware/i2c.h"
#include <stdio.h>          // For printf()
#include "CONFIG.h"            
#include "drivers/CAT24C512_driver.h"      
#include "drivers/ILI9488_driver.h"        
#include "drivers/MCP23017_display_driver.h"       
#include "drivers/MCP23017_relay_driver.h"
#include "drivers/w5500_driver.h"   
#include "utils/EEPROM_MemoryMap.h"  
#include "dummy_eeprom.h"
#include "PDU_display.h"      // For PDU_Display_Test() 
#include "startup.h"          // For init_gpio()    
#include "hardware/resets.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/spi.h"
#include "hardware/structs/iobank0.h"
#include "hardware/pwm.h"



// Forward declaration for the function that will run on core 1.
void core1_task(void);
void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name);
void activate_relay(uint8_t relay_number);


int main(void) {
    // Initialize standard I/O (for debugging via USB serial, for example)
    stdio_init_all();
    //sleep_ms(10000);
    // --- Overclocking ---
    if (set_sys_clock_khz(200000, true) == 0) {
        printf("Overclocked to 200MHz successfully.\n");
    } else {
        printf("Failed to overclock to 200MHz.\n");
    }

    startup_init();
    i2c_scan_bus(i2c0, "I2C0");
    i2c_scan_bus(i2c1, "I2C1");
        
    // --- Hardware Initialization ---
    // Initialize the EEPROM (CAT24C512)
    CAT24C512_Init();   printf("EEPROM initialized\n");
    write_dummy_eeprom_content();

    // Initialize the MCP23017 I/O expanders.
    mcp_display_init(); printf("Display-board initialized\n");
    mcp_relay_init();   printf("Relay-board initialized\n");
    
    // Little showoff :D
    mcp_display_write_reg(MCP23017_OLATA, 0xFF);
    mcp_display_write_reg(MCP23017_OLATB, 0xFF);
    sleep_ms(500);
    for(uint8_t i = 16; i > 0; i--) {
        mcp_display_write_pin(i-1, 0);
        sleep_ms(50);
    }
    mcp_display_write_pin(PWR_LED, 1);
    mcp_display_write_pin(FAULT_LED, 1);

    // Initialize the ILI9488 display.
    ILI9488_Init();
    PDU_Display_Init();
    sleep_ms(200);

    PDU_Display_UpdateStatus("System initialized.");
    mcp_display_write_pin(FAULT_LED, 0);
    sleep_ms(1000);
    PDU_Display_UpdateStatus("System ready.");



	
    // --- Main loop on Core 0 ---
    while (true) {
        // Core 0 is dedicated to tasks other than Ethernet.
        sleep_ms(1000);
    }
    
    return 0;
}

void core1_task(void) {
    // Core 1: Initialize the Ethernet interface (W5500)
    if (w5500_init() != 0) {
        printf("Ethernet initialization failed.\n");
    } else {
        printf("Ethernet initialized successfully.\n");
    }
    
    // Loop to process Ethernet communication.
    while (true) {
        w5500_http_server_task(HTTP_SOCKET);
        sleep_ms(10);
    }
}

void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name) {
    printf("Scanning %s...\n", bus_name);
    bool device_found = false;
    uint8_t dummy = 0;
    // Common I2C address range: 0x08 to 0x77
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // A dummy write: we send one dummy byte.
        // Some devices might not expect a byte, but for scanning purposes this usually works.
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

// Activates a relay (0-7) and turns on the corresponding LED on the display board.
void activate_relay(uint8_t relay_number) {
    if (relay_number > 7) {
        printf("Invalid relay number %d. Must be between 0 and 7.\n", relay_number);
        return;
    }

    // Turn on the relay on the relay board.
    mcp_relay_write_pin(relay_number, 1);
    // Turn on the corresponding LED on the display board.
    mcp_display_write_pin(relay_number, 1);
}