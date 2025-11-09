/**
 * @file NetTask.h
 * @author DvidMakesThings - David Sipos
 * @brief Network task handling HTTP server and W5500 Ethernet controller
 * @date 2025-11-07
 *
 * @details This task manages all network operations including HTTP server
 *          processing and W5500 Ethernet controller communication. It runs
 *          continuously, processing HTTP requests and maintaining network
 *          connectivity. SPI access to W5500 is protected by spiMtx.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef NETTASK_H
#define NETTASK_H

#include "../CONFIG.h"

/**
 * @brief Network task priority
 */
#define NET_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

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
 * Call after ButtonTask_Init(true) in your bringup sequence:
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

/**
 * @}
 * @}
 */