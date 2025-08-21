/**
 * @file CAT24C512_driver.h
 * @author DvidMakesThings - David Sipos
 * @brief Driver for CAT24C512 I2C EEPROM
 * @version 1.0
 * @date 2025-03-03
 * 
 * Header file for the CAT24C512 EEPROM driver. The CAT24C512 is a 512Kbit (64KB) I2C EEPROM.
 * This driver provides functions for initialization, reading and writing data in various
 * formats, and utility functions for debugging EEPROM contents.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CAT24C512_DRIVER_H
#define CAT24C512_DRIVER_H

#include "../CONFIG.h"
#include <stdint.h>

/**
 * @brief Initializes the CAT24C512 EEPROM.
 * 
 * Configures I2C1 interface with a 400 kHz clock frequency (Fast Mode) for communication
 * with the CAT24C512 EEPROM. Sets up the appropriate GPIO pins as SDA and SCL and enables
 * internal pull-up resistors on these pins.
 */
void CAT24C512_Init(void);

/**
 * @brief Writes a byte to the EEPROM.
 * 
 * Writes a single byte to the specified address in the EEPROM.
 * The function handles the 16-bit addressing and includes a delay
 * to allow the write cycle to complete.
 * 
 * @param addr 16-bit memory address (0x0000 - 0xFFFF).
 * @param data Byte to write to the specified address.
 * @return 0 if successful, -1 if the I2C write operation failed.
 */
int CAT24C512_WriteByte(uint16_t addr, uint8_t data);

/**
 * @brief Reads a byte from the EEPROM.
 * 
 * Reads a single byte from the specified address in the EEPROM.
 * Sends the 16-bit address to the EEPROM with a repeated start condition,
 * then reads the data byte from the device.
 * 
 * @param addr 16-bit memory address (0x0000 - 0xFFFF).
 * @return The byte read from the specified address.
 */
uint8_t CAT24C512_ReadByte(uint16_t addr);

/**
 * @brief Writes a buffer of data to the EEPROM.
 * 
 * Writes a sequence of bytes to the EEPROM starting at the specified address.
 * Automatically handles page boundaries by breaking the write operation into
 * multiple chunks of appropriate size. Includes delays between page writes
 * to allow the write cycle to complete.
 * 
 * @param addr 16-bit starting memory address (0x0000 - 0xFFFF).
 * @param data Pointer to the source data buffer.
 * @param len Number of bytes to write.
 * @return 0 if successful, -1 if any I2C write operation failed.
 */
int CAT24C512_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Reads a buffer of data from the EEPROM.
 * 
 * Reads a sequence of bytes from the EEPROM starting at the specified address.
 * Sends the 16-bit address followed by a repeated start condition, then
 * reads the requested number of bytes into the provided buffer.
 * 
 * @param addr 16-bit starting memory address (0x0000 - 0xFFFF).
 * @param buffer Pointer to the destination buffer where read data will be stored.
 * @param len Number of bytes to read (buffer must be at least this size).
 */
void CAT24C512_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len);

/**
 * @brief Dumps the entire EEPROM contents to a buffer.
 * 
 * Reads the entire 64KB of the EEPROM memory and stores it in the provided buffer.
 * Useful for backing up EEPROM contents or for debugging purposes.
 * 
 * @param buffer Pointer to a buffer of at least 65536 bytes (64KB) to store the EEPROM contents.
 *               The caller must ensure the buffer is large enough.
 */
void CAT24C512_Dump(uint8_t *buffer);

/**
 * @brief Dumps the EEPROM contents in a formatted hex table.
 * 
 * Reads the entire EEPROM memory and outputs its contents as a formatted
 * hexadecimal table to the standard output (typically UART). The output is
 * organized as 16-byte rows with address headers and column headers for easier
 * reading. Output is delimited by "EE_DUMP_START" and "EE_DUMP_END" markers.
 */
void CAT24C512_DumpFormatted(void);

#endif // CAT24C512_DRIVER_H
