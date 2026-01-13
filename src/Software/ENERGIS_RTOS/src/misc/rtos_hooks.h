/**
 * @file src/misc/rtos_hooks.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup misc4 4. RTOS Hooks Module
 * @ingroup misc
 * @brief FreeRTOS hook implementations and scheduler diagnostics
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * This module provides implementations of FreeRTOS application hooks for fault
 * handling and system monitoring. It captures fault context into watchdog scratch
 * registers for post-mortem analysis and implements an Idle task canary for
 * scheduler liveness detection.
 *
 * Provided functionality:
 * - FreeRTOS hook implementations (Idle, StackOverflow, MallocFailed, Assert)
 * - HardFault exception handler with context capture
 * - Idle task canary for scheduler stall detection
 * - Fault breadcrumb storage in watchdog scratch registers
 *
 * All fault hooks capture minimal context and initiate a controlled reboot to
 * allow post-mortem analysis via the boot diagnostics system.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/**
 * @name Public API
 * @ingroup misc4
 * @{
 */
/**
 * @brief FreeRTOS Idle task hook for scheduler liveness monitoring.
 *
 * This hook is called by the FreeRTOS Idle task during each iteration of its
 * main loop. It increments a global canary counter and records the current
 * RTOS time, providing a mechanism for other tasks (typically HealthTask) to
 * detect scheduler stalls or excessive blocking.
 *
 * The hook must execute quickly and never block, as blocking the Idle task
 * can prevent lower-priority cleanup operations.
 *
 * @return None
 *
 * @note Requires configUSE_IDLE_HOOK=1 in FreeRTOSConfig.h
 * @note Runs at Idle priority; higher priority tasks will preempt it
 */
void vApplicationIdleHook(void);

/**
 * @brief Read current Idle task canary value.
 *
 * Returns the current value of the monotonic counter incremented by the
 * Idle hook. Used for scheduler liveness detection by comparing successive
 * samples.
 *
 * @return Current canary value (32-bit monotonic counter)
 *
 * @note Wraps at 32-bit boundary; use delta calculation for comparisons
 */
uint32_t RTOS_IdleCanary_Read(void);

/**
 * @brief Read timestamp of last Idle hook execution.
 *
 * Returns the RTOS time (in milliseconds) when the Idle hook was last
 * executed. Useful for determining how recently the Idle task has run.
 *
 * @return Milliseconds since scheduler start of last Idle execution
 *
 * @note Time is derived from FreeRTOS tick count
 */
uint32_t RTOS_IdleCanary_LastMs(void);

/**
 * @brief Compute canary value change since previous sample.
 *
 * Calculates the difference between the current canary value and a
 * previously saved value. Handles 32-bit wraparound correctly.
 *
 * A zero delta indicates the Idle task has not run since the previous
 * sample, suggesting possible scheduler stall or continuous CPU loading.
 *
 * @param prev_value Previously sampled canary value
 *
 * @return Change in canary value (32-bit modulo arithmetic)
 *
 * @note Typical usage: sample canary, delay, sample again, compute delta
 */
uint32_t RTOS_IdleCanary_Delta(uint32_t prev_value);

/**
 * @brief Get current RTOS time in milliseconds.
 *
 * Returns the current FreeRTOS tick count converted to milliseconds.
 * Provides a monotonic time reference for timing and scheduling.
 *
 * @return Milliseconds since scheduler start
 *
 * @note Resolution depends on configTICK_RATE_HZ configuration
 * @note Wraps at 32-bit boundary (approximately 49.7 days at 1ms tick)
 */
uint32_t RTOS_TicksMs(void);
/** @} */

/** @} */