/**
 * @file src/tasks/LoggerTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks05 5. Logger Task
 * @ingroup tasks
 * @brief Centralized logging subsystem with queue-based message handling.
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * This module implements a centralized logging system for the PDU firmware using
 * a FreeRTOS task and message queue architecture. It provides thread-safe logging
 * from any task context with optional muting capability for critical sections.
 *
 * Architecture:
 * - Queue-based message passing decouples log generation from output
 * - Single consumer task (LoggerTask) handles all USB-CDC output
 * - Non-blocking sends prevent logging from blocking application tasks
 * - Configurable queue depth and message size
 * - Mute functionality for reducing noise during debug operations
 *
 * Key Features:
 * - Thread-safe logging from any task or ISR context
 * - Printf-style formatting with variadic arguments
 * - Automatic message truncation to prevent buffer overflows
 * - Non-blocking queue operations (messages dropped if queue full)
 * - Nested mute/unmute support for critical sections
 * - Force-log capability to bypass mute state
 * - Integrated with health monitoring for watchdog supervision
 *
 * Usage Pattern:
 * 1. Initialize early in boot sequence via LoggerTask_Init(true)
 * 2. Use log_printf() from any task for normal logging
 * 3. Use Logger_MutePush()/MutePop() to temporarily suppress logs
 * 4. Use log_printf_force() to log even when muted
 *
 * Output:
 * - All logs directed to USB-CDC (stdout via stdio_init_all)
 * - 1-second delay after init for USB enumeration
 * - Immediate console availability for early boot diagnostics
 *
 * @note LoggerTask should be initialized first in boot sequence to capture
 *       all subsequent initialization logs.
 * @note Messages are truncated to LOGGER_MSG_MAX bytes if too long.
 * @note Queue full condition silently drops messages (check LOGGER_QUEUE_LEN).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef LOGGER_H
#define LOGGER_H

#include "../CONFIG.h"

/**
 * @name Public API
 * @{
 */
/**
 * @brief Query logger task readiness status.
 * @ingroup tasks05
 *
 * Provides a thread-safe method to check whether LoggerTask has completed
 * initialization successfully. Used for deterministic boot sequencing to
 * ensure logging is available before other subsystems start.
 *
 * @return true if LoggerTask_Init(true) completed successfully and queue is created.
 * @return false if LoggerTask_Init() was not called, called with enable=false,
 *         or initialization failed.
 *
 * @note Based on log queue existence, providing immediate readiness indication.
 * @note Safe to call from any task context or early in boot before scheduler starts.
 */
bool Logger_IsReady(void);

/**
 * @brief Initialize and start the logger task.
 * @ingroup tasks05
 *
 * Creates the LoggerTask FreeRTOS task and message queue for centralized logging.
 * Should be called first in the boot sequence to capture all initialization logs
 * from subsequent subsystems.
 *
 * Initialization Sequence:
 * 1. Creates log message queue (LOGGER_QUEUE_LEN deep)
 * 2. Spawns LoggerTask with configured priority and stack size
 * 3. Task initializes USB-CDC and begins processing queued messages
 *
 * @param[in] enable Set true to initialize and start task, false to skip
 *                   initialization deterministically without side effects.
 *
 * @return pdPASS on successful initialization or when skipped (enable=false).
 * @return pdFAIL if initialization fails (queue creation, task creation).
 *
 * @note Call before all other tasks in boot sequence (step 1/6).
 * @note Idempotent: safe to call multiple times, creates resources only once.
 * @note Logs error codes to error logger on failure (if error logger available).
 */
BaseType_t LoggerTask_Init(bool enable);

/**
 * @brief Log formatted message to output.
 * @ingroup tasks05
 *
 * Thread-safe printf-style logging function that formats and queues a message
 * for output by LoggerTask. Messages are silently dropped if logger is not
 * initialized, queue is full, or logger is muted.
 *
 * @param[in] fmt Printf-style format string.
 * @param[in] ... Variable arguments matching format specifiers.
 *
 * @note Non-blocking: returns immediately even if queue is full (message dropped).
 * @note Messages truncated to LOGGER_MSG_MAX bytes including null terminator.
 * @note Suppressed when Logger_MutePush() active; use log_printf_force() to override.
 * @note Safe to call from any task context; not safe from ISR (use ISR-safe alternative).
 */
void log_printf(const char *fmt, ...);

/**
 * @brief Enter logger mute section.
 * @ingroup tasks05
 *
 * Suppresses all log_printf() output by incrementing an internal mute depth counter.
 * Useful for reducing log noise during intensive debug operations or repetitive tasks.
 * Supports nesting: each MutePush() must be matched by a MutePop().
 *
 * @note Thread-safe: uses critical section for atomic counter increment.
 * @note Does not affect log_printf_force() which always outputs.
 * @note Mute state is global; affects all tasks calling log_printf().
 */
void Logger_MutePush(void);

/**
 * @brief Exit logger mute section.
 * @ingroup tasks05
 *
 * Decrements the mute depth counter. When counter reaches zero, log_printf()
 * resumes normal operation. Counter saturates at zero (won't underflow).
 *
 * @note Thread-safe: uses critical section for atomic counter decrement.
 * @note Must be paired with Logger_MutePush() for each mute level.
 * @note Safe to call even if not muted (no-op when counter already zero).
 */
void Logger_MutePop(void);

/**
 * @brief Log message bypassing mute state.
 * @ingroup tasks05
 *
 * Identical to log_printf() but ignores the mute counter, ensuring message
 * is logged even during muted sections. Useful for critical diagnostics that
 * must always appear regardless of mute state.
 *
 * @param[in] fmt Printf-style format string.
 * @param[in] ... Variable arguments matching format specifiers.
 *
 * @note Same non-blocking behavior and truncation rules as log_printf().
 * @note Still requires logger to be initialized (checked internally).
 */
void log_printf_force(const char *fmt, ...);
/** @} */

#endif /* LOGGER_H */

/** @} */