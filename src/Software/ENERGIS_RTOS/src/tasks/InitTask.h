/**
 * @file src/tasks/InitTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks Tasks
 * @brief General task implementations for ENERGIS PDU firmware.
 * @{
 *
 * @defgroup tasks01 1. Init Task - RTOS Initialization
 * @ingroup tasks
 * @brief Hardware bring-up sequencing task implementation
 * @{
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details InitTask runs at highest priority during system boot to:
 * 1. Initialize all hardware in proper sequence
 * 2. Probe peripherals to verify communication
 * 3. Create subsystem tasks in dependency order
 * 4. Wait for subsystems to report ready
 * 5. Apply saved configuration (relay states) on startup
 * 6. Delete itself when system is fully operational
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
 * InitTask will delete itself after system is fully operational
 */
void InitTask_Create(void);

/**
 * @brief Save current relay states as startup configuration.
 *
 * Reads current relay states from hardware and saves them to EEPROM.
 * These saved states will be applied automatically on next boot.
 * Can be called from console commands or web UI.
 *
 * @return true on success, false on error
 */
bool InitTask_SaveCurrentRelayStates(void);

#endif /* INIT_TASK_H */

/** @} */
/** @} */