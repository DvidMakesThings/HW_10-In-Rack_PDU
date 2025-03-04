/**
 * @file MCP23017_driver.h
 * @author David Sipos (DvidMakesThings)
 * @brief Driver for MCP23017 I/O Expander
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

 #ifndef MCP23017_DRIVER_H
 #define MCP23017_DRIVER_H
 
 #include <stdint.h>
 #include "hardware/i2c.h"
 #include "../CONFIG.h"  // Include CONFIG.h for MCP23017 register definitions
 
 /**
  * @brief Initializes the MCP23017 by setting all pins as outputs.
  * @param i2c_bus The I2C bus to use (e.g., MCP23017_RELAY_I2C or MCP23017_DISPLAY_I2C).
  * @param i2c_addr I2C address of the MCP23017.
  */
 void MCP23017_Init(i2c_inst_t *i2c_bus, uint8_t i2c_addr);
 
 /**
  * @brief Sets the direction of a single pin.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr I2C address of the MCP23017.
  * @param pin The pin number (0-15).
  * @param direction 0 = Output, 1 = Input.
  */
 void MCP23017_SetDirection(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t pin, uint8_t direction);
 
 /**
  * @brief Writes a value to a specific pin.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr I2C address of the MCP23017.
  * @param pin The pin number (0-15).
  * @param value 0 = Low, 1 = High.
  */
 void MCP23017_WritePin(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t pin, uint8_t value);
 
 /**
  * @brief Reads the state of a specific pin.
  * @param i2c_bus The I2C bus to use.
  * @param i2c_addr I2C address of the MCP23017.
  * @param pin The pin number (0-15).
  * @return 0 = Low, 1 = High.
  */
 uint8_t MCP23017_ReadPin(i2c_inst_t *i2c_bus, uint8_t i2c_addr, uint8_t pin);
 
 #endif // MCP23017_DRIVER_H