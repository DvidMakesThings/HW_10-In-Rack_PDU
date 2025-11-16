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
 * @version 1.0.0
 * @date 2025-11-08
 *
 * @details InitTask runs at highest priority during system boot to:
 * 1. Initialize all hardware in proper sequence
 * 2. Probe peripherals to verify communication
 * 3. Create subsystem tasks in dependency order
 * 4. Wait for subsystems to report ready
 * 5. Delete itself when system is fully operational
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

#endif /* INIT_TASK_H */

/** @} */
/** @} */