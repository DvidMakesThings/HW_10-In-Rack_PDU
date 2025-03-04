/**
 * @file main.c
 * @brief 
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"  // Required for set_sys_clock_khz()
#include <stdio.h>          // For printf()
#include "CONFIG.h"                
#include "drivers/CAT24C512_driver.h"      
#include "drivers/ILI9488_driver.h"        
#include "drivers/MCP23017_driver.h"       
#include "drivers/w5500_driver.h"     
#include "PDU_display.h"      // For PDU_Display_Test()     

// Forward declaration for the function that will run on core 1.
void core1_task(void);

int main(void) {
    // Initialize standard I/O (for debugging via USB serial, for example)
    stdio_init_all();
    
    // --- Overclocking ---
    // Overclock the RP2040 to 200 MHz.
    // set_sys_clock_khz() sets the system clock frequency in kHz.
    if (set_sys_clock_khz(200000, true) != 0) {
         printf("Failed to overclock to 200MHz.\n");
    } else {
         printf("Overclocked to 200MHz successfully.\n");
    }
    
    // --- Hardware Initialization ---
    // Initialize the EEPROM (CAT24C512)
    CAT24C512_Init();
    
    // Initialize the TFT LCD display (ILI9488)
    ILI9488_Init();
    
    // Initialize the MCP23017 I/O expanders.
    // One for the relay board and one for the display board.
    MCP23017_Init(i2c0, MCP_RELAY_ADDR);
    MCP23017_Init(i2c1, MCP_DISPLAY_ADDR);
    
    // --- Launch Core 1 for Ethernet and additional tasks ---
    multicore_launch_core1(core1_task);
    
	// Run our UI test procedure:
    PDU_Display_Test();
	
    // --- Main loop on Core 0 ---
    while (true) {
        // Core 0 is dedicated to tasks other than Ethernet.
        printf("Core 0 running...\n");
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
