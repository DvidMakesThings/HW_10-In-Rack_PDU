/* File: misc/rtos_hooks.h  (RP2040-safe, non-blocking hooks) */
#include "../CONFIG.h"

/**
 * @brief FreeRTOS Idle hook. Increments the scheduler canary and records time.
 *
 * Runs in Idle task context and must never block. Signals scheduler liveness
 * by advancing @ref g_rtos_idle_canary and stamping @ref g_rtos_idle_last_ms.
 */
void vApplicationIdleHook(void);

/**
 * @brief Read the current Idle canary value.
 *
 * @return Monotonic counter incremented by the Idle hook.
 */
uint32_t RTOS_IdleCanary_Read(void);

/**
 * @brief Read the last millisecond timestamp observed in the Idle hook.
 *
 * @return Milliseconds since scheduler start when Idle last ran.
 */
uint32_t RTOS_IdleCanary_LastMs(void);

/**
 * @brief Compute canary delta since a previous sample.
 *
 * @param prev_value Previously sampled canary value.
 * @return The difference (current - prev_value), modulo 32-bit.
 */
uint32_t RTOS_IdleCanary_Delta(uint32_t prev_value);

/**
 * @brief Snapshot the current RTOS time in milliseconds.
 *
 * @return Milliseconds since scheduler start.
 */
uint32_t RTOS_TicksMs(void);
