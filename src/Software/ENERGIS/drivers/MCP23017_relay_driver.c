/**
 * @file MCP23017_relay_driver.c
 * @author David Sipos (DvidMakesThings)
 * @brief Driver for the Relay Board's MCP23017 I/O Expander.
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "MCP23017_relay_driver.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Internal helper to read a register from the MCP23017.
static uint8_t mcp_relay_read_register(uint8_t reg) {
    uint8_t value;
    i2c_write_blocking(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, &reg, 1, true);
    i2c_read_blocking(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, &value, 1, false);
    return value;
}

// Internal helper to write a register on the MCP23017.
static void mcp_relay_write_register(uint8_t reg, uint8_t value) {
    uint8_t data[2] = { reg, value };
    i2c_write_blocking(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, data, 2, false);
}

void mcp_relay_write_reg(uint8_t reg, uint8_t value) {
    mcp_relay_write_register(reg, value);
}

uint8_t mcp_relay_read_reg(uint8_t reg) {
    return mcp_relay_read_register(reg);
}

void mcp_relay_init(void) {
    // Perform a hardware reset on the MCP23017 using MCP_REL_RST.
    // Assume that startup_init() has already been called to initialize the GPIO.
    gpio_put(MCP_REL_RST, 0);    // Assert reset.
    sleep_ms(100);               // Hold reset (adjust delay as needed)
    gpio_put(MCP_REL_RST, 1);    // Release reset.
    sleep_ms(100);               // Wait for the device to initialize.

    // Configure all pins as outputs by writing 0x00 to IODIRA and IODIRB.
    mcp_relay_write_register(MCP23017_IODIRA, 0x00);
    mcp_relay_write_register(MCP23017_IODIRB, 0x00);
}

void mcp_relay_set_direction(uint8_t pin, uint8_t direction) {
    uint8_t reg = (pin < 8) ? MCP23017_IODIRA : MCP23017_IODIRB;
    uint8_t bit = pin % 8;
    uint8_t current = mcp_relay_read_register(reg);
    if (direction) {
        current |= (1 << bit);  // Set as input.
    } else {
        current &= ~(1 << bit); // Set as output.
    }
    mcp_relay_write_register(reg, current);
}

void mcp_relay_write_pin(uint8_t pin, uint8_t value) {
    uint8_t reg = (pin < 8) ? MCP23017_OLATA : MCP23017_OLATB;
    uint8_t bit = pin % 8;
    uint8_t current = mcp_relay_read_register(reg);
    if (value) {
        current |= (1 << bit);  // Set pin high.
    } else {
        current &= ~(1 << bit); // Set pin low.
    }
    mcp_relay_write_register(reg, current);
}

uint8_t mcp_relay_read_pin(uint8_t pin) {
    uint8_t reg = (pin < 8) ? MCP23017_GPIOA : MCP23017_GPIOB;
    uint8_t bit = pin % 8;
    uint8_t value = mcp_relay_read_register(reg);
    return (value >> bit) & 0x01;
}
