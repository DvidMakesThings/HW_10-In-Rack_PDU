/**
 * @file src/tasks/HealthTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks08 8. Health
 * @ingroup tasks
 * @brief System health monitor: watchdog, heartbeats, diagnostics (reset-safe).
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details Implements a FreeRTOS-based health monitoring task that manages the
 * hardware watchdog, tracks heartbeats from registered tasks, logs diagnostics,
 * and records long blocking events for post-mortem analysis.
 * Key behavior:
 *  - Graceful ARMING: watchdog is enabled only after a warmup period and after
 *    at least one heartbeat from each registered task (or warmup timeout).
 *  - LIVENESS WINDOW: each task must heartbeat at least once within
 *    HEALTH_SILENCE_MS; otherwise the watchdog is NOT fed.
 *  - DIAGNOSTICS: prints stack high-water marks and free heap periodically,
 *    and keeps a small ring of "blocked" events for post-mortem.
 *
 * Integration:
 *  1) Call Health_RegisterTask(id, handle, "name") after xTaskCreate for each task.
 *  2) Call HealthTask_Start() once everything is created (at end of InitTask).
 *  3) Inside each task loop, call Health_Heartbeat(HEALTH_ID_xxx) once per loop.
 *  4) Optionally call Health_RecordBlocked("tag", waited_ms) on long waits.
 * 
 * @note Do NOT arm another watchdog elsewhere. Let HealthTask manage it.
 * You can adjust HEALTH_WARMUP_MS and HEALTH_SILENCE_MS below.
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#pragma once

#include "../CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOGTASKBEAT_MS 500
#define NETTASKBEAT_MS 500
#define METERTASKBEAT_MS 500
#define STORAGETASKBEAT_MS 500
#define BUTTONTASKBEAT_MS 500
#define CONSOLETASKBEAT_MS 500

/**
 * @enum health_id_t
 * @brief Stable heartbeat IDs for tasks supervised by HealthTask.
 *
 * Extend this enumeration as needed up to HEALTH_ID_MAX. Each ID maps to a bit
 * in the internal required-mask policy. By default, all IDs < HEALTH_ID_MAX are
 * considered required unless HEALTH_REQUIRED_MASK is customized at build time.
 */
typedef enum {
    HEALTH_ID_LOGGER = 0,  /**< Logger task. */
    HEALTH_ID_CONSOLE = 1, /**< Console (CLI) task. */
    HEALTH_ID_STORAGE = 2, /**< Storage/EEPROM task. */
    HEALTH_ID_BUTTON = 3,  /**< Front-panel button scanner task. */
    // HEALTH_ID_SWITCH = 4,  /**< Relay/switch control task. */
    HEALTH_ID_NET = 5,   /**< Network stack task (SNMP/HTTP). */
    HEALTH_ID_METER = 6, /**< Metering task (HLW8032). */
    HEALTH_ID_MAX = 16   /**< Maximum number of tracked IDs (compile-time cap). */
} health_id_t;

/**
 * @brief Start the HealthTask (idempotent).
 *
 * Creates the FreeRTOS task if not yet created. The watchdog is armed inside
 * HealthTask after warmup and initial heartbeats policy.
 */
void HealthTask_Start(void);

/**
 * @brief Register a task handle and human-readable name for diagnostics.
 *
 * @param id   Enumerated health ID for the task.
 * @param h    FreeRTOS task handle.
 * @param name ASCII task name; stored by pointer (must remain valid).
 *
 * On first registration the last-seen timestamp is primed to now() to avoid
 * bogus negative deltas in early logs.
 */
void Health_RegisterTask(health_id_t id, TaskHandle_t h, const char *name);

/**
 * @brief Mark a task as alive (heartbeat).
 *
 * @param id Enumerated health ID for the task.
 *
 * Call this once per main loop iteration or at a reasonable cadence shorter
 * than HEALTH_SILENCE_MS to keep the watchdog fed by HealthTask.
 */
void Health_Heartbeat(health_id_t id);

/**
 * @brief Record a long blocking event for later diagnostics.
 *
 * @param tag       Short ASCII tag identifying the blocked site.
 * @param waited_ms Wait duration in milliseconds (clamped to 65535).
 *
 * These records are kept in a small ring buffer and dumped periodically.
 */
void Health_RecordBlocked(const char *tag, uint32_t waited_ms);

/**
 * @brief Immediate, Health-owned reboot with context.
 *
 * @param reason Short reason string (may be NULL). Used for logs only.
 *
 * @details
 * Stores a reboot marker in RP2040 watchdog scratch so the next boot can
 * report that the reset was intentional (not a stale-bite). Also snapshots
 * early-boot info, then triggers the RP2040 watchdog reset.
 */
void Health_RebootNow(const char *reason);

#ifdef __cplusplus
}
#endif

/** @} */