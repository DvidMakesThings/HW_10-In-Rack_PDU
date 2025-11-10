/* File: tasks/HealthTask.h */
#pragma once
/**
 * @file HealthTask.h
 * @brief System health monitor: watchdog, heartbeats, diagnostics (reset-safe).
 *
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
 * Notes:
 *  - Do NOT arm another watchdog elsewhere. Let HealthTask manage it.
 *  - You can adjust HEALTH_WARMUP_MS and HEALTH_SILENCE_MS below.
 */

#include "../CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stable heartbeat IDs (extend as needed, up to 16 default) */
typedef enum {
    HEALTH_ID_LOGGER = 0,
    HEALTH_ID_CONSOLE = 1,
    HEALTH_ID_STORAGE = 2,
    HEALTH_ID_BUTTON = 3,
    HEALTH_ID_SWITCH = 4,
    HEALTH_ID_NET = 5,
    HEALTH_ID_METER = 6,
    HEALTH_ID_MAX = 16
} health_id_t;

/** Start HealthTask (creates the task; watchdog is armed internally after warmup). */
void HealthTask_Start(void);

/** Register a task handle and name for diagnostics (call right after xTaskCreate). */
void Health_RegisterTask(health_id_t id, TaskHandle_t h, const char *name);

/** Mark a task as alive (call once per loop in each task). */
void Health_Heartbeat(health_id_t id);

/** Record a long blocking event (e.g., waited >50 ms on a mutex/queue). */
void Health_RecordBlocked(const char *tag, uint32_t waited_ms);

#ifdef __cplusplus
}
#endif
