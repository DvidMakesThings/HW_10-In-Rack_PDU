/**
 * @file src/tasks/NetTask.h
 * @author DvidMakesThings - David Sipos
 * 
 * @defgroup tasks06 6. Network Task
 * @ingroup tasks
 * @brief Network task handling HTTP server and W5500 Ethernet controller
 * @{
 * 
 * @version 1.1.0
 * @date 2025-11-07
 *
 * @details NetTask owns all W5500 operations. It waits for StorageTask to signal that
 * configuration is ready, reads the network config, brings up the W5500, then
 * runs the web server loop.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef NETTASK_H
#define NETTASK_H

#include "../CONFIG.h"

/**
 * @brief Network task stack size
 */
#define NET_TASK_STACK_SIZE 1024

/**
 * @brief Network task cycle time in milliseconds
 */
#define NET_TASK_CYCLE_MS 10

/**
 * @brief Starts the network task with a deterministic enable gate.
 *
 * @details
 * - Deterministic boot order step 5/6. Waits for Storage to be READY.
 * - If @p enable is false, network is skipped and marked NOT ready.
 * - Task still internally waits for cfg ready (storage_wait_ready) as before.
 *
 * @instructions
 * Call after ButtonTask_Init(true) in the bringup sequence:
 *   NetTask_Init(true);
 * Query readiness via Net_IsReady().
 *
 * @param enable Gate that allows or skips starting this subsystem.
 * @return pdPASS if task created successfully, pdFAIL otherwise.
 */
BaseType_t NetTask_Init(bool enable);

/**
 * @brief Network subsystem readiness query.
 *
 * @details
 * READY when the Net task handle exists (robust against latch desync).
 *
 * @return true if netTaskHandle is non-NULL, else false.
 */
bool Net_IsReady(void);

#endif // NETTASK_H

/** @} */