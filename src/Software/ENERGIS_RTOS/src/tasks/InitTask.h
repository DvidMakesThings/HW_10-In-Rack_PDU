/**
 * @file InitTask.h
 * @author DvidMakesThings - David Sipos
 * @brief Hardware bring-up sequencing task header
 *
 * @version 1.0.0
 * @date 2025-11-08
 * @details
 * InitTask orchestrates the complete system bring-alive sequence:
 * 1. Hardware initialization (GPIO, I2C, SPI, ADC)
 * 2. Peripheral probing (MCP23017, EEPROM, W5500, HLW8032)
 * 3. Driver initialization
 * 4. Task creation in dependency order
 * 5. Self-deletion when complete
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef INIT_TASK_H
#define INIT_TASK_H

#include "../CONFIG.h"

/**
 * @brief Create and start the InitTask
 *
 * This function creates the InitTask which will run at highest priority
 * during system boot to initialize all hardware and create subsystem tasks.
 *
 * @note Call this from main() BEFORE vTaskStartScheduler()
 * @note InitTask will delete itself after system is fully operational
 */
void InitTask_Create(void);

#endif /* INIT_TASK_H */