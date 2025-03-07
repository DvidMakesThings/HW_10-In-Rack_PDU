/**
 * @file CAT24C512_driver.h
 * @author David Sipos
 * @brief Driver for CAT24C512 I2C EEPROM
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CAT24C512_DRIVER_H
#define CAT24C512_DRIVER_H

#include <stdint.h>
#include "../CONFIG.h"

/**
 * @brief Initializes the CAT24C512 EEPROM.
 * Configures I2C1 interface for communication.
 */
void CAT24C512_Init(void);

/**
 * @brief Writes a byte to the EEPROM.
 * @param addr 16-bit memory address.
 * @param data Byte to write.
 * @return 0 if successful, -1 if failed.
 */
int CAT24C512_WriteByte(uint16_t addr, uint8_t data);

/**
 * @brief Reads a byte from the EEPROM.
 * @param addr 16-bit memory address.
 * @return The read byte.
 */
uint8_t CAT24C512_ReadByte(uint16_t addr);

/**
 * @brief Writes a buffer of data to the EEPROM.
 * @param addr 16-bit memory address.
 * @param data Pointer to data buffer.
 * @param len Number of bytes to write.
 * @return 0 if successful, -1 if failed.
 */
int CAT24C512_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Reads a buffer of data from the EEPROM.
 * @param addr 16-bit memory address.
 * @param buffer Pointer to buffer to store data.
 * @param len Number of bytes to read.
 */
void CAT24C512_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len);

/**
 * @brief Dumps the entire EEPROM contents to a buffer.
 * @param buffer Pointer to a 64 KB buffer (EEPROM size).
 */
void CAT24C512_Dump(uint8_t *buffer);

/**
 * @brief Dumps the EEPROM contents in a formatted hex table.
 * @param output_mode Selects where to send the data:  
 *        - 0 = Save to a file on Flash  
 *        - 1 = Send via Serial (UART)
 */
void CAT24C512_DumpFormatted(void);


#endif // CAT24C512_DRIVER_H
