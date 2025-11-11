/**
 * @file src/tasks/LoggerTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks03 3. Logger
 * @ingroup tasks
 * @brief Logger task implementation (RTOS version)
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details Implements a FreeRTOS-based logging task that receives log messages
 * via a queue and outputs them to stdio (USB-CDC).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "../CONFIG.h"

/**
 * @brief Query Logger READY state.
 *
 * @details
 * The logger is considered READY once the log queue has been created by
 * LoggerTask_Init() (i.e., @c logQueue != NULL). This is sufficient for
 * deterministic boot gating since the task is spawned immediately after
 * queue creation.
 *
 * @return true if the logger queue exists, false otherwise.
 */
bool Logger_IsReady(void);

/**
 * @brief Initialize and start the Logger task with a deterministic enable gate.
 * @param enable Gate to allow or skip starting this subsystem.
 * @return pdPASS on success (or when skipped), pdFAIL on creation error.
 * @details
 * Boot order step 1. If @p enable is false, the function skips creation and
 * leaves the module NOT ready. When true, it creates @c logQueue (once) and
 * spawns the @c LoggerTask (once). On success the module becomes READY.
 */
BaseType_t LoggerTask_Init(bool enable);

/**
 * @brief Printf-style logging into the Logger queue.
 * @param fmt printf-style format string.
 * @param ... Variadic arguments.
 * @details
 * Formats a message and sends it to @c logQueue created by LoggerTask_Init().
 * The message is truncated to @c LOGGER_MSG_MAX bytes if necessary.
 */
void log_printf(const char *fmt, ...);

#endif /* LOGGER_H */

/** @} */