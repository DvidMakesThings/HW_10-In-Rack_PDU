/**
 * @file src/tasks/InitTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks04 4. Init Task - RTOS Initialization
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
 * @brief Create and start the InitTask.
 * @ingroup tasks04
 * @details
 * Creates the high-priority initialization task responsible for hardware bring-up
 * and subsystem creation. Runs during early boot, then deletes itself once the
 * system reports ready.
 *
 * @note Call from `main()` before `vTaskStartScheduler()`.
 * @note Deterministic sequencing: creates tasks in dependency order and waits for readiness.
 */
void InitTask_Create(void);

/**
 * @brief Save current relay states as startup configuration.
 * @ingroup tasks04
 * @details
 * Reads current relay states from hardware and persists them to EEPROM. The saved
 * states are applied automatically on the next boot.
 *
 * @return true on success, false on error.
 */
bool InitTask_SaveCurrentRelayStates(void);

#endif /* INIT_TASK_H */

/** @} */
/** @} */