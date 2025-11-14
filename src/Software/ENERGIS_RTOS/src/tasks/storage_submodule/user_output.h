/**
 * @file src/tasks/storage_submodule/user_output.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage03 3. User Output
 * @ingroup storage
 * @brief Relay power-on state persistence
 * @{
 * 
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Manages relay power-on states in EEPROM. Stores default ON/OFF states
 * for all 8 relay channels.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef USER_OUTPUT_H
#define USER_OUTPUT_H

#include "../../CONFIG.h"

/**
 * @brief Write user output (relay states) to EEPROM.
 * @param data Pointer to relay state array (8 bytes: 0=OFF, 1=ON)
 * @param len Must be <= EEPROM_USER_OUTPUT_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds or write error
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len);

/**
 * @brief Read user output (relay states) from EEPROM.
 * @param data Destination buffer for relay states
 * @param len Must be <= EEPROM_USER_OUTPUT_SIZE
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on bounds error
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len);

#endif /* USER_OUTPUT_H */

/** @} */