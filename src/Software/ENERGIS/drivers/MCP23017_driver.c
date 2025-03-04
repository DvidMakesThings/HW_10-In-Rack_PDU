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

 #include "MCP23017_driver.h"
 #include "pico/stdlib.h"
 #include "hardware/i2c.h"
 #include "../CONFIG.h"
 
 /**
  * @brief Writes a value to a register of the MCP23017.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr The I2C address of the MCP23017.
  * @param reg The register address.
  * @param value The value to write.
  */
 static void MCP23017_WriteRegister(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t reg, uint8_t value) {
     uint8_t data[2] = { reg, value };
     i2c_write_blocking(i2c_bus, i2c_addr, data, 2, false);
 }
 
 /**
  * @brief Reads a value from a register of the MCP23017.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr The I2C address of the MCP23017.
  * @param reg The register address.
  * @return The read value.
  */
 static uint8_t MCP23017_ReadRegister(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t reg) {
     uint8_t value;
     i2c_write_blocking(i2c_bus, i2c_addr, &reg, 1, true);
     i2c_read_blocking(i2c_bus, i2c_addr, &value, 1, false);
     return value;
 }
 
 /**
  * @brief Initializes the MCP23017 by setting all pins as outputs.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr The I2C address of the MCP23017.
  */
 void MCP23017_Init(i2c_inst_t *i2c_bus, uint8_t i2c_addr) {
     MCP23017_WriteRegister(i2c_bus, i2c_addr, MCP23017_IODIRA, 0x00); // All outputs on bank A
     MCP23017_WriteRegister(i2c_bus, i2c_addr, MCP23017_IODIRB, 0x00); // All outputs on bank B
 }
 
 /**
  * @brief Sets the direction of a single pin.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr The I2C address of the MCP23017.
  * @param pin The pin number (0-15).
  * @param direction 0 = Output, 1 = Input.
  */
 void MCP23017_SetDirection(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t pin, uint8_t direction) {
     uint8_t reg = (pin < 8) ? MCP23017_IODIRA : MCP23017_IODIRB;
     uint8_t bit = pin % 8;
     uint8_t current_value = MCP23017_ReadRegister(i2c_bus, i2c_addr, reg);
 
     if (direction)
         current_value |= (1 << bit);  // Set as input
     else
         current_value &= ~(1 << bit); // Set as output
 
     MCP23017_WriteRegister(i2c_bus, i2c_addr, reg, current_value);
 }
 
 /**
  * @brief Writes a value to a specific pin.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr The I2C address of the MCP23017.
  * @param pin The pin number (0-15).
  * @param value 0 = Low, 1 = High.
  */
 void MCP23017_WritePin(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t pin, uint8_t value) {
     uint8_t reg = (pin < 8) ? MCP23017_OLATA : MCP23017_OLATB;
     uint8_t bit = pin % 8;
     uint8_t current_value = MCP23017_ReadRegister(i2c_bus, i2c_addr, reg);
 
     if (value)
         current_value |= (1 << bit);  // Set high
     else
         current_value &= ~(1 << bit); // Set low
 
     MCP23017_WriteRegister(i2c_bus, i2c_addr, reg, current_value);
 }
 
 /**
  * @brief Reads the state of a specific pin.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr The I2C address of the MCP23017.
  * @param pin The pin number (0-15).
  * @return 0 = Low, 1 = High.
  */
 uint8_t MCP23017_ReadPin(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t pin) {
     uint8_t reg = (pin < 8) ? MCP23017_GPIOA : MCP23017_GPIOB;
     uint8_t bit = pin % 8;
     return (MCP23017_ReadRegister(i2c_bus, i2c_addr, reg) >> bit) & 1;
 }