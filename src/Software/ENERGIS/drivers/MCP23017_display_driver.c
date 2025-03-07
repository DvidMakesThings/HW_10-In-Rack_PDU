/**
 * @file MCP23017_driver.c
 * @author David Sipos
 * @brief Implementation of MCP23017 I/O Expander Driver
 * @version 1.1
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

 #include "MCP23017_display_driver.h"
 #include "pico/stdlib.h"
 #include "hardware/i2c.h"
 
 // Internal helper to read a register from the MCP23017.
static uint8_t mcp_display_read_register(uint8_t reg) {
    uint8_t value;
    // Write register address, then read one byte.
    i2c_write_blocking(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, &reg, 1, true);
    i2c_read_blocking(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, &value, 1, false);
    return value;
}

// Internal helper to write a register on the MCP23017.
static void mcp_display_write_register(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, data, 2, false);
}

void mcp_display_write_reg(uint8_t reg, uint8_t value) {
    mcp_display_write_register(reg, value);
}

uint8_t mcp_display_read_reg(uint8_t reg) {
    return mcp_display_read_register(reg);
}

void mcp_display_init(void) {
    // Perform a hardware reset on the MCP23017 using MCP_LCD_RST.
    // Assume that startup_init() has already been called so that MCP_LCD_RST is configured.
    gpio_put(MCP_LCD_RST, 0);    // Assert reset.
    sleep_ms(100);                // Hold reset for 10ms.
    gpio_put(MCP_LCD_RST, 1);    // Release reset.
    sleep_ms(100);                // Wait for the device to initialize.

    // Configure all pins as outputs by writing 0x00 to both IODIRA and IODIRB.
    mcp_display_write_register(MCP23017_IODIRA, 0x00);
    mcp_display_write_register(MCP23017_IODIRB, 0x00);
}

void mcp_display_set_direction(uint8_t pin, uint8_t direction) {
    // Determine the register (IODIRA for pins 0-7, IODIRB for pins 8-15).
    uint8_t reg = (pin < 8) ? MCP23017_IODIRA : MCP23017_IODIRB;
    uint8_t bit = pin % 8;
    uint8_t current = mcp_display_read_register(reg);
    
    if (direction) {
        current |= (1 << bit);  // Set as input.
    } else {
        current &= ~(1 << bit); // Set as output.
    }
    mcp_display_write_register(reg, current);
}

void mcp_display_write_pin(uint8_t pin, uint8_t value) {
    // Write to output latch registers (OLATA for pins 0-7, OLATB for pins 8-15).
    uint8_t reg = (pin < 8) ? MCP23017_OLATA : MCP23017_OLATB;
    uint8_t bit = pin % 8;
    uint8_t current = mcp_display_read_register(reg);
    
    if (value) {
        current |= (1 << bit);  // Set pin high.
    } else {
        current &= ~(1 << bit); // Set pin low.
    }
    mcp_display_write_register(reg, current);
}

uint8_t mcp_display_read_pin(uint8_t pin) {
    // Read from the GPIO registers (GPIOA for pins 0-7, GPIOB for pins 8-15).
    uint8_t reg = (pin < 8) ? MCP23017_GPIOA : MCP23017_GPIOB;
    uint8_t bit = pin % 8;
    uint8_t value = mcp_display_read_register(reg);
    return (value >> bit) & 0x01;
}