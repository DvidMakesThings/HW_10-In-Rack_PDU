/**
 * @file src/tasks/HealthTask.c
 * @author DvidMakesThings - David Sipos
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

#include "../CONFIG.h"

/* ---------- Tag + Logging ---------- */
#ifndef HEALTH_TAG
#define HEALTH_TAG "[Health]"
#endif

#define HEALTH_PRINT_WDT_FEED 1

#ifndef HEALTH_DBG
#ifdef DEBUG_PRINT_HEALTH
#define HEALTH_DBG(fmt, ...) DEBUG_PRINT_HEALTH("%s " fmt, HEALTH_TAG, ##__VA_ARGS__)
#else
#define HEALTH_DBG(fmt, ...)                                                                       \
    do {                                                                                           \
        (void)0;                                                                                   \
    } while (0)
#endif
#endif

#ifndef HEALTH_INFO
#ifdef INFO_PRINT
#define HEALTH_INFO(fmt, ...) INFO_PRINT("%s " fmt, HEALTH_TAG, ##__VA_ARGS__)
#else
#define HEALTH_INFO(fmt, ...)
do {
} while (0)
#endif
#endif

#ifndef HEALTH_WRN
#ifdef WARNING_PRINT
#define HEALTH_WRN(fmt, ...) WARNING_PRINT("%s " fmt, HEALTH_TAG, ##__VA_ARGS__)
#else
#define HEALTH_WRN(fmt, ...) HEALTH_DBG("WARN: " fmt, ##__VA_ARGS__)
#endif
#endif

#ifndef HEALTH_ERR
#ifdef ERROR_PRINT
#define HEALTH_ERR(fmt, ...) ERROR_PRINT("%s " fmt, HEALTH_TAG, ##__VA_ARGS__)
#else
#define HEALTH_ERR(fmt, ...) HEALTH_DBG("ERR: " fmt, ##__VA_ARGS__)
#endif
#endif

/* ---------- Tunables ---------- */
#ifndef HEALTH_PERIOD_MS
#define HEALTH_PERIOD_MS 250u
#endif

#ifndef HEALTH_LOG_PERIOD_MS
#define HEALTH_LOG_PERIOD_MS 60000u
#endif

#ifndef HEALTH_WARMUP_MS
#define HEALTH_WARMUP_MS 2000u
#endif

#ifndef HEALTH_SILENCE_MS
#define HEALTH_SILENCE_MS 3000u
#endif

#ifndef HEALTH_BLOCK_RING_SIZE
#define HEALTH_BLOCK_RING_SIZE 32u
#endif

#ifndef HEALTH_REQUIRED_MASK
#if (HEALTH_ID_MAX >= 32)
#define HEALTH_REQUIRED_MASK (0xFFFFFFFFu)
#else
// #define HEALTH_REQUIRED_MASK ((HEALTH_ID_MAX == 0) ? 0u : ((1u << HEALTH_ID_MAX) - 1u))
#define HEALTH_REQUIRED_MASK                                                                       \
    ((1u << HEALTH_ID_LOGGER) | (1u << HEALTH_ID_CONSOLE) | (1u << HEALTH_ID_STORAGE) |            \
     (1u << HEALTH_ID_BUTTON) | (1u << HEALTH_ID_NET) | (1u << HEALTH_ID_METER))
#endif
#endif

/* ---------- Persisted reboot info (RP2040 watchdog scratch regs) ---------- */
#ifndef HEALTH_SCRATCH_MAGIC
#define HEALTH_SCRATCH_MAGIC (0x484C5448u)
#endif

#define HSCR_MAGIC 0
#define HSCR_CAUSE 1
#define HSCR_STALE 2
#define HSCR_MAXDT 3
#define HSCR_TIMEOUT 4
#define HSCR_TNOW 5
#define HSCR_REQMSK 6
#define HSCR_FLAGS 7

static inline void hscr_clear(void) {
#if PICO_RP2040
    for (int i = 0; i < 8; ++i)
        watchdog_hw->scratch[i] = 0u;
#endif
}

static inline void hscr_store_stale(uint32_t stale_mask, uint32_t maxdt, uint32_t silence_ms,
                                    uint32_t tnow, uint32_t req_mask, bool armed,
                                    bool warmup_elapsed) {
#if PICO_RP2040
    watchdog_hw->scratch[HSCR_MAGIC] = HEALTH_SCRATCH_MAGIC;
    watchdog_hw->scratch[HSCR_CAUSE] = 1u;
    watchdog_hw->scratch[HSCR_STALE] = stale_mask;
    watchdog_hw->scratch[HSCR_MAXDT] = maxdt;
    watchdog_hw->scratch[HSCR_TIMEOUT] = silence_ms;
    watchdog_hw->scratch[HSCR_TNOW] = tnow;
    watchdog_hw->scratch[HSCR_REQMSK] = req_mask;
    watchdog_hw->scratch[HSCR_FLAGS] = (armed ? 1u : 0u) | (warmup_elapsed ? 2u : 0u);
#else
    (void)stale_mask;
    (void)maxdt;
    (void)silence_ms;
    (void)tnow;
    (void)req_mask;
    (void)armed;
    (void)warmup_elapsed;
#endif
}

static inline uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

/* ---------- Types ---------- */
typedef struct {
    uint32_t tick_ms;
    char tag[24];
    uint16_t waited_ms;
} block_evt_t;

typedef struct {
    TaskHandle_t handle;
    const char *name;
    uint32_t last_seen_ms;
    bool registered;
} task_meta_t;

/* ---------- State ---------- */
static task_meta_t s_meta[HEALTH_ID_MAX];
static block_evt_t s_block_ring[HEALTH_BLOCK_RING_SIZE];
static volatile uint32_t s_block_w = 0;
static TaskHandle_t s_health_handle = NULL;
static bool s_watchdog_armed = false;
static uint32_t s_start_ms = 0;

/* ---------- Introspection helpers ---------- */
#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
static void log_stack_watermarks(void) {
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (s_meta[i].registered && s_meta[i].handle) {
            UBaseType_t hw = uxTaskGetStackHighWaterMark(s_meta[i].handle);
            HEALTH_DBG("Stack-HW %-10s : %lu Words\r\n", s_meta[i].name ? s_meta[i].name : "task",
                       (unsigned long)hw);
        }
    }
}
#else
static void log_stack_watermarks(void) {}
#endif

#if (configUSE_MALLOC_FAILED_HOOK == 1)
extern size_t xPortGetFreeHeapSize(void);
#endif

/**************************************************************************/
/** @brief Milliseconds since last watchdog feed recorded by Health. */
static uint32_t s_last_wdt_feed_ms = 0;
/** @brief Guard to emit at most one pre-bark warning per silence window. */
static bool s_prebark_emitted = false;

/** @brief Margin before timeout to emit a pre-bark warning (ms). */
#ifndef HEALTH_PREBARK_MS
#define HEALTH_PREBARK_MS 300u
#endif

/**
 * @brief Record a watchdog feed and reset the pre-bark guard.
 *
 * Call this instead of raw watchdog_update() wherever the Health policy
 * decides to feed the watchdog. It still performs the hardware feed, but
 * also stores @p now_ms so we can warn if we approach the timeout.
 *
 * @param now_ms Monotonic time in milliseconds (e.g. to_ms_since_boot()).
 */
static inline void Health_OnWdtFeed(uint32_t now_ms) {
    watchdog_update();
    s_last_wdt_feed_ms = now_ms;
    s_prebark_emitted = false;
}

/**
 * @brief Emit a one-shot warning when remaining time is below a margin.
 *
 * Call this once per Health tick (e.g. where you print the periodic
 * "Summary"). If the watchdog is armed and the time since the last feed
 * encroaches within HEALTH_PREBARK_MS of HEALTH_SILENCE_MS, it prints a
 * concise line with per-task deltas to help pinpoint slow offenders.
 *
 * This function never re-arms or feeds the watchdog; it only logs.
 *
 * @param now_ms Monotonic time in milliseconds (e.g. to_ms_since_boot()).
 */
static inline void Health_PreBarkCheck(uint32_t now_ms) {
    if (!s_watchdog_armed) {
        return;
    }

    const uint32_t since_feed = now_ms - s_last_wdt_feed_ms;

    /* Warn once when we are inside the last HEALTH_PREBARK_MS before the bite. */
    if (since_feed + HEALTH_PREBARK_MS >= HEALTH_SILENCE_MS) {
        if (!s_prebark_emitted) {
            const uint32_t rem =
                (since_feed >= HEALTH_SILENCE_MS) ? 0u : (HEALTH_SILENCE_MS - since_feed);

            /* Per-task dt snapshots (only for required tasks that are registered). */
            uint32_t dtL = 0, dtC = 0, dtS = 0, dtB = 0, dtN = 0, dtM = 0;

            if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_LOGGER))
                dtL = s_meta[HEALTH_ID_LOGGER].last_seen_ms
                          ? (now_ms - s_meta[HEALTH_ID_LOGGER].last_seen_ms)
                          : 0xFFFFFFFFu;
            if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_CONSOLE))
                dtC = s_meta[HEALTH_ID_CONSOLE].last_seen_ms
                          ? (now_ms - s_meta[HEALTH_ID_CONSOLE].last_seen_ms)
                          : 0xFFFFFFFFu;
            if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_STORAGE))
                dtS = s_meta[HEALTH_ID_STORAGE].last_seen_ms
                          ? (now_ms - s_meta[HEALTH_ID_STORAGE].last_seen_ms)
                          : 0xFFFFFFFFu;
            if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_BUTTON))
                dtB = s_meta[HEALTH_ID_BUTTON].last_seen_ms
                          ? (now_ms - s_meta[HEALTH_ID_BUTTON].last_seen_ms)
                          : 0xFFFFFFFFu;
            if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_NET))
                dtN = s_meta[HEALTH_ID_NET].last_seen_ms
                          ? (now_ms - s_meta[HEALTH_ID_NET].last_seen_ms)
                          : 0xFFFFFFFFu;
            if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_METER))
                dtM = s_meta[HEALTH_ID_METER].last_seen_ms
                          ? (now_ms - s_meta[HEALTH_ID_METER].last_seen_ms)
                          : 0xFFFFFFFFu;

            ERROR_PRINT("[WDT-PREBARK] since_last_feed=%lu ms, remain=%lu ms | "
                        "Logger=%s%lu  Console=%s%lu  Storage=%s%lu  Button=%s%lu  Net=%s%lu  "
                        "Meter=%s%lu\r\n",
                        (unsigned long)since_feed, (unsigned long)rem,
                        (dtL == 0xFFFFFFFFu ? "NEVER/" : ""),
                        (unsigned long)(dtL == 0xFFFFFFFFu ? 0u : dtL),
                        (dtC == 0xFFFFFFFFu ? "NEVER/" : ""),
                        (unsigned long)(dtC == 0xFFFFFFFFu ? 0u : dtC),
                        (dtS == 0xFFFFFFFFu ? "NEVER/" : ""),
                        (unsigned long)(dtS == 0xFFFFFFFFu ? 0u : dtS),
                        (dtB == 0xFFFFFFFFu ? "NEVER/" : ""),
                        (unsigned long)(dtB == 0xFFFFFFFFu ? 0u : dtB),
                        (dtN == 0xFFFFFFFFu ? "NEVER/" : ""),
                        (unsigned long)(dtN == 0xFFFFFFFFu ? 0u : dtN),
                        (dtM == 0xFFFFFFFFu ? "NEVER/" : ""),
                        (unsigned long)(dtM == 0xFFFFFFFFu ? 0u : dtM));

            s_prebark_emitted = true;
        }
    }
}
/**************************************************************************/

static void log_heap_free(void) {
#if (configUSE_MALLOC_FAILED_HOOK == 1)
    size_t freeb = xPortGetFreeHeapSize();
    HEALTH_INFO("HEAP FREE=%lu bytes\r\n", (unsigned long)freeb);
#endif
}

static void dump_block_ring(void) {
    uint32_t w = s_block_w;
    uint32_t n = (w > HEALTH_BLOCK_RING_SIZE) ? HEALTH_BLOCK_RING_SIZE : w;
    if (n == 0)
        return;

    HEALTH_DBG("recent blocked events (newest last):\r\n");
    uint32_t start = (w >= n) ? (w - n) : 0;
    for (uint32_t i = 0; i < n; ++i) {
        const block_evt_t *e = &s_block_ring[(start + i) % HEALTH_BLOCK_RING_SIZE];
        HEALTH_DBG("  t=%lu ms tag=%s waited=%u ms\r\n", (unsigned long)e->tick_ms,
                   e->tag[0] ? e->tag : "-", (unsigned)e->waited_ms);
    }
}

/* ---------- Policy helpers (mask-based) ---------- */
static inline bool id_in_required(uint8_t id) { return (HEALTH_REQUIRED_MASK & (1u << id)) != 0; }

static bool required_tasks_have_heartbeat_once(void) {
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (!s_meta[i].registered)
            continue;
        if (!id_in_required((uint8_t)i))
            continue;
        if (s_meta[i].last_seen_ms == 0)
            return false;
    }
    return true;
}

static uint32_t max_dt_required(uint32_t now_ms_) {
    uint32_t maxdt = 0;
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (!s_meta[i].registered)
            continue;
        if (!id_in_required((uint8_t)i))
            continue;

        uint32_t last = s_meta[i].last_seen_ms;

        if (!s_watchdog_armed && last == 0)
            continue;

        if (s_watchdog_armed && last == 0)
            return HEALTH_SILENCE_MS + 1u;

        uint32_t dt = (now_ms_ >= last) ? (now_ms_ - last) : 0u;
        if (dt > maxdt)
            maxdt = dt;
    }
    return maxdt;
}

static uint32_t stale_mask_required(uint32_t now_ms_) {
    uint32_t mask = 0;
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (!s_meta[i].registered)
            continue;
        if (!id_in_required((uint8_t)i))
            continue;

        uint32_t last = s_meta[i].last_seen_ms;

        if (last == 0) {
            if (s_watchdog_armed)
                mask |= (1u << i);
            continue;
        }

        uint32_t dt = (now_ms_ >= last) ? (now_ms_ - last) : 0u;
        if (dt > HEALTH_SILENCE_MS)
            mask |= (1u << i);
    }
    return mask;
}

/**
 * @brief Print the heartbeat status of all required tasks, one line per task.
 *
 * @param current_time Monotonic timestamp in milliseconds to use as "now".
 *
 * @details
 * For each health ID that is marked required, this function prints exactly one log line:
 *  - "<name>: PRE-ARM" if no heartbeat has ever been seen and the watchdog is not armed yet.
 *  - "<name>: NEVER"  if no heartbeat has ever been seen but the watchdog is already armed.
 *  - "<name>: <dt> ms" where dt = current_time - last_seen_ms, if at least one heartbeat arrived.
 *
 * The task name is taken from the registration metadata; if missing, "task" is used.
 * This function does not assemble multi-line strings; it emits one line per task for readability.
 */
static void log_required_status(uint32_t current_time) {
    HEALTH_DBG("Required beats:\r\n");

    for (int task_id = 0; task_id < HEALTH_ID_MAX; ++task_id) {
        if (!id_in_required((uint8_t)task_id)) {
            continue;
        }

        const char *task_name = s_meta[task_id].name ? s_meta[task_id].name : "task";

        if (s_meta[task_id].last_seen_ms == 0u) {
            if (s_watchdog_armed) {
                HEALTH_INFO("  %s: NEVER\r\n", task_name);
            } else {
                HEALTH_INFO("  %s: PRE-ARM\r\n", task_name);
            }
        } else {
            uint32_t dt = current_time - s_meta[task_id].last_seen_ms;
            HEALTH_INFO("  %s: %lu ms\r\n", task_name, (unsigned long)dt);
        }
    }
}

static void report_stale(uint32_t now_ms_) {
    uint32_t stale_mask = stale_mask_required(now_ms_);
    if (stale_mask == 0u)
        return;

    char line[200];
    size_t p = 0;
    p += (size_t)snprintf(line + p, sizeof(line) - p, "REBOOT PENDING; stale: ");

    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if ((stale_mask & (1u << i)) == 0)
            continue;

        const char *nm = s_meta[i].name ? s_meta[i].name : "task";
        uint32_t last = s_meta[i].last_seen_ms;

        if (last == 0) {
            p += (size_t)snprintf(line + p, sizeof(line) - p, "%s:NEVER ", nm);
        } else {
            uint32_t dt = (now_ms_ >= last) ? (now_ms_ - last) : 0u;
            p += (size_t)snprintf(line + p, sizeof(line) - p, "%s:%lu ", nm, (unsigned long)dt);
        }

        if (p >= sizeof(line))
            break;
    }

    HEALTH_ERR("%s\r\n", line);

    uint32_t maxdt = max_dt_required(now_ms_);
    hscr_store_stale(stale_mask, maxdt, HEALTH_SILENCE_MS, now_ms_, (uint32_t)HEALTH_REQUIRED_MASK,
                     s_watchdog_armed, ((now_ms_ - s_start_ms) >= HEALTH_WARMUP_MS));
}

static bool hscr_load_is_health_reboot(uint32_t *stale_mask, uint32_t *max_dt_ms,
                                       uint32_t *silence_ms, uint32_t *t_now_ms, uint32_t *req_mask,
                                       uint32_t *flags) {
#if PICO_RP2040
    if (watchdog_caused_reboot() && watchdog_hw->scratch[HSCR_MAGIC] == HEALTH_SCRATCH_MAGIC &&
        watchdog_hw->scratch[HSCR_CAUSE] == 1u) {
        if (stale_mask)
            *stale_mask = watchdog_hw->scratch[HSCR_STALE];
        if (max_dt_ms)
            *max_dt_ms = watchdog_hw->scratch[HSCR_MAXDT];
        if (silence_ms)
            *silence_ms = watchdog_hw->scratch[HSCR_TIMEOUT];
        if (t_now_ms)
            *t_now_ms = watchdog_hw->scratch[HSCR_TNOW];
        if (req_mask)
            *req_mask = watchdog_hw->scratch[HSCR_REQMSK];
        if (flags)
            *flags = watchdog_hw->scratch[HSCR_FLAGS];
        return true;
    }
    return false;
#else
    (void)stale_mask;
    (void)max_dt_ms;
    (void)silence_ms;
    (void)t_now_ms;
    (void)req_mask;
    (void)flags;
    return false;
#endif
}

static void print_health_reboot_brief(void) {
    uint32_t stale, maxdt, silence, tnow, req, flags;
    if (hscr_load_is_health_reboot(&stale, &maxdt, &silence, &tnow, &req, &flags)) {
        HEALTH_ERR("PREVIOUS REBOOT by Health: stale_mask=0x%08lx max_dt=%lu ms silence=%lu ms "
                   "flags=0x%lx req_mask=0x%08lx at t=%lu ms\r\n",
                   (unsigned long)stale, (unsigned long)maxdt, (unsigned long)silence,
                   (unsigned long)flags, (unsigned long)req, (unsigned long)tnow);
        if (stale) {
            char line[192];
            size_t p = 0;
            p += (size_t)snprintf(line + p, sizeof(line) - p, "Offenders: ");
            for (int i = 0; i < HEALTH_ID_MAX; ++i) {
                if ((stale & (1u << i)) == 0)
                    continue;
                const char *nm = s_meta[i].name ? s_meta[i].name : "task";
                p += (size_t)snprintf(line + p, sizeof(line) - p, "%s ", nm);
                if (p >= sizeof(line))
                    break;
            }
            HEALTH_ERR("%s\r\n", line);
        }
        hscr_clear();
    }
}

/**
 * @brief Health supervision worker task with strict arm gating and idle-canary checks.
 *
 * Periodically evaluates heartbeats of all registered tasks, logs stack-watermarks,
 * heap usage and recent blocking events, and supervises the hardware watchdog.
 * The watchdog ARMS ONLY AFTER BOTH conditions hold:
 *   (1) warmup window elapsed, AND
 *   (2) every required task produced at least one heartbeat.
 *
 * Additionally, emits a one-shot "pre-bark" ERROR_PRINT when we are within
 * HEALTH_PREBARK_MS of HEALTH_SILENCE_MS since the last feed, with per-task dt.
 *
 * @param arg Unused task parameter.
 */
static void health_task(void *arg) {
    (void)arg;
    s_start_ms = now_ms();

    print_health_reboot_brief();

    const TickType_t period_ticks = pdMS_TO_TICKS(HEALTH_PERIOD_MS);
    const TickType_t log_period = (HEALTH_LOG_PERIOD_MS ? pdMS_TO_TICKS(HEALTH_LOG_PERIOD_MS) : 0);
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t last_log = last_wake;

    /* Idle-canary sampling */
    uint32_t idle_prev = RTOS_IdleCanary_Read();
    uint32_t idle_stall_ms = 0u;

    HEALTH_INFO(
        "task started, cfg: period=%u ms, warmup=%u ms, silence=%u ms, req_mask=0x%08lx\r\n",
        (unsigned)HEALTH_PERIOD_MS, (unsigned)HEALTH_WARMUP_MS, (unsigned)HEALTH_SILENCE_MS,
        (unsigned long)HEALTH_REQUIRED_MASK);

    for (;;) {
        vTaskDelayUntil(&last_wake, period_ticks);
        uint32_t t = now_ms();

        /* Scheduler liveness via Idle canary */
        {
            uint32_t idle_now = RTOS_IdleCanary_Read();
            uint32_t d = idle_now - idle_prev;
            idle_prev = idle_now;
            if (d == 0u) {
                if (idle_stall_ms < 0xFFFFFFFFu - HEALTH_PERIOD_MS) {
                    idle_stall_ms += HEALTH_PERIOD_MS;
                }
            } else {
                idle_stall_ms = 0u;
            }
            if (idle_stall_ms >= (HEALTH_PERIOD_MS * 3u)) {
                HEALTH_WRN("scheduler idle stalled ~%lu ms (idle_last_ms=%lu)\r\n",
                           (unsigned long)idle_stall_ms, (unsigned long)RTOS_IdleCanary_LastMs());
            }
        }

        /* Periodic summary/log dump */
        if (log_period && (xTaskGetTickCount() - last_log) >= log_period) {
            const uint32_t elapsed = t - s_start_ms;
            const uint32_t maxdt = max_dt_required(t);
            const uint32_t remain = (maxdt >= HEALTH_SILENCE_MS) ? 0u : (HEALTH_SILENCE_MS - maxdt);

            log_printf("\r\n");
            HEALTH_INFO("**** %usec Report at %lu sec ****\r\n", HEALTH_LOG_PERIOD_MS / 1000,
                        (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS / 1000));
            HEALTH_INFO("Summary:\r\n");
            HEALTH_INFO("Elapsed since boot = %lu ms\r\n", (unsigned long)elapsed);
            HEALTH_INFO("WD Armed = %u\r\n", s_watchdog_armed ? 1u : 0u);
            HEALTH_INFO("Max dt = %lu ms\r\n", (unsigned long)maxdt);
            HEALTH_INFO("Remaining time until WD reset = %lu ms\r\n", (unsigned long)remain);
            HEALTH_INFO("Idle stall = %lu ms\r\n", (unsigned long)idle_stall_ms);
            log_required_status(t);
            log_stack_watermarks();
            log_heap_free();
            dump_block_ring();
            last_log = xTaskGetTickCount();
        }

        /* Strict arm gating: require BOTH warmup elapsed AND first beats from all required tasks */
        if (!s_watchdog_armed) {
            const bool warmup_elapsed = (t - s_start_ms) >= HEALTH_WARMUP_MS;
            const bool all_required_beaten = required_tasks_have_heartbeat_once();
            if (warmup_elapsed && all_required_beaten) {
                HEALTH_WRN("########### GRACE PERIOD ENDED ############\r\n");
                HEALTH_WRN("############# ARMING WATCHDOG #############\r\n");
                HEALTH_INFO("\tAll required tasks confirmed\r\n");
                HEALTH_INFO("\tTask Timeout = %u ms\r\n\r\n", (unsigned)HEALTH_SILENCE_MS);
                // WARNING_PRINT("############# WATCHDOG DISABLED FOR DEVELOPMENT BUILD
                // #############\r\n");
                watchdog_enable(HEALTH_SILENCE_MS, 1);
                watchdog_update();
                CrashLog_RecordWdtFeed(t);
                s_watchdog_armed = true;

                /* start pre-bark window tracking */
                s_last_wdt_feed_ms = t;
                s_prebark_emitted = false;

#if defined(HEALTH_PRINT_WDT_FEED) && (HEALTH_PRINT_WDT_FEED)
                HEALTH_DBG("[WDT] FEED at %lums (armed)\r\n", (unsigned long)t);
#endif
                HEALTH_DBG("watchdog armed\r\n");
            } else {
                if (!warmup_elapsed) {
                    HEALTH_INFO("Waiting warmup: %lu ms remain\r\n",
                                (unsigned long)(HEALTH_WARMUP_MS - (t - s_start_ms)));
                }
                if (!all_required_beaten) {
                    char line[192];
                    size_t p = 0;
                    p += (size_t)snprintf(line + p, sizeof(line) - p, "waiting first beats: ");
                    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
                        if (!id_in_required((uint8_t)i))
                            continue;
                        if (s_meta[i].last_seen_ms == 0) {
                            const char *nm = s_meta[i].name ? s_meta[i].name : "task";
                            p += (size_t)snprintf(line + p, sizeof(line) - p, "%s ", nm);
                            if (p >= sizeof(line))
                                break;
                        }
                    }
                    HEALTH_WRN("%s\r\n", line);
                }
                continue;
            }
        }

        /* One-shot pre-bark: warn once when we are close to the bite */
        if (s_watchdog_armed) {
            const uint32_t since_feed = t - s_last_wdt_feed_ms;
            if (!s_prebark_emitted && (since_feed + HEALTH_PREBARK_MS) >= HEALTH_SILENCE_MS) {
                const uint32_t rem =
                    (since_feed >= HEALTH_SILENCE_MS) ? 0u : (HEALTH_SILENCE_MS - since_feed);

                uint32_t dtL = 0, dtC = 0, dtS = 0, dtB = 0, dtN = 0, dtM = 0;
                if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_LOGGER))
                    dtL = s_meta[HEALTH_ID_LOGGER].last_seen_ms
                              ? (t - s_meta[HEALTH_ID_LOGGER].last_seen_ms)
                              : 0xFFFFFFFFu;
                if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_CONSOLE))
                    dtC = s_meta[HEALTH_ID_CONSOLE].last_seen_ms
                              ? (t - s_meta[HEALTH_ID_CONSOLE].last_seen_ms)
                              : 0xFFFFFFFFu;
                if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_STORAGE))
                    dtS = s_meta[HEALTH_ID_STORAGE].last_seen_ms
                              ? (t - s_meta[HEALTH_ID_STORAGE].last_seen_ms)
                              : 0xFFFFFFFFu;
                if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_BUTTON))
                    dtB = s_meta[HEALTH_ID_BUTTON].last_seen_ms
                              ? (t - s_meta[HEALTH_ID_BUTTON].last_seen_ms)
                              : 0xFFFFFFFFu;
                if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_NET))
                    dtN = s_meta[HEALTH_ID_NET].last_seen_ms
                              ? (t - s_meta[HEALTH_ID_NET].last_seen_ms)
                              : 0xFFFFFFFFu;
                if (HEALTH_REQUIRED_MASK & (1u << HEALTH_ID_METER))
                    dtM = s_meta[HEALTH_ID_METER].last_seen_ms
                              ? (t - s_meta[HEALTH_ID_METER].last_seen_ms)
                              : 0xFFFFFFFFu;

                ERROR_PRINT("[WDT-PREBARK] since_last_feed=%lu ms, remain=%lu ms | "
                            "Logger=%s%lu Console=%s%lu Storage=%s%lu Button=%s%lu "
                            "Net=%s%lu Meter=%s%lu\r\n",
                            (unsigned long)since_feed, (unsigned long)rem,
                            (dtL == 0xFFFFFFFFu ? "NEVER/" : ""),
                            (unsigned long)(dtL == 0xFFFFFFFFu ? 0u : dtL),
                            (dtC == 0xFFFFFFFFu ? "NEVER/" : ""),
                            (unsigned long)(dtC == 0xFFFFFFFFu ? 0u : dtC),
                            (dtS == 0xFFFFFFFFu ? "NEVER/" : ""),
                            (unsigned long)(dtS == 0xFFFFFFFFu ? 0u : dtS),
                            (dtB == 0xFFFFFFFFu ? "NEVER/" : ""),
                            (unsigned long)(dtB == 0xFFFFFFFFu ? 0u : dtB),
                            (dtN == 0xFFFFFFFFu ? "NEVER/" : ""),
                            (unsigned long)(dtN == 0xFFFFFFFFu ? 0u : dtN),
                            (dtM == 0xFFFFFFFFu ? "NEVER/" : ""),
                            (unsigned long)(dtM == 0xFFFFFFFFu ? 0u : dtM));
                s_prebark_emitted = true;
            }
        }

        /* When armed: feed if every required task is within silence bound */
        uint32_t maxdt = max_dt_required(t);
        if (maxdt <= HEALTH_SILENCE_MS) {
            uint32_t remain = HEALTH_SILENCE_MS - maxdt;
            watchdog_update();
            CrashLog_RecordWdtFeed(t);

            /* keep pre-bark book-keeping in sync with actual feed */
            s_last_wdt_feed_ms = t;
            s_prebark_emitted = false;

#if defined(HEALTH_PRINT_WDT_FEED) && (HEALTH_PRINT_WDT_FEED)
            HEALTH_DBG("[WDT] FEED at %lums (remain=%lums, max_dt=%lums)\r\n", (unsigned long)t,
                       (unsigned long)remain, (unsigned long)maxdt);
#endif

            for (int i = 0; i < HEALTH_ID_MAX; ++i) {
                if (!id_in_required((uint8_t)i) || !s_meta[i].registered)
                    continue;
                uint32_t last = s_meta[i].last_seen_ms;
                if (!last) {
                    HEALTH_WRN("Misbehaving: %s NEVER heartbeated\r\n",
                               s_meta[i].name ? s_meta[i].name : "task");
                    continue;
                }
                uint32_t dt = (t >= last) ? (t - last) : 0u;
                if (dt >= (HEALTH_SILENCE_MS * 3u) / 4u) {
                    HEALTH_WRN("Misbehaving: %s dt = %lu ms\r\n",
                               s_meta[i].name ? s_meta[i].name : "task", (unsigned long)dt);
                }
            }
        } else {
            report_stale(t);
        }
    }
}

/* ---------- Public API ---------- */
void HealthTask_Start(void) {
    if (s_health_handle)
        return;
    xTaskCreate(health_task, "Health", 1024, NULL, HEALTHTASK_PRIORITY, &s_health_handle);
    HEALTH_DBG("Start requested, (created=%u)\r\n", s_health_handle ? 1u : 0u);
}

void Health_RegisterTask(health_id_t id, TaskHandle_t h, const char *name) {
    if ((int)id < 0 || (int)id >= HEALTH_ID_MAX)
        return;

    const bool was_registered = s_meta[id].registered;

    s_meta[id].handle = h;
    s_meta[id].name = name ? name : "task";
    s_meta[id].registered = (h != NULL);

    if (!was_registered && s_meta[id].registered) {
        s_meta[id].last_seen_ms = now_ms();
    }

    HEALTH_INFO("%s Registered on ID = %d, Handle = %p\r\n", s_meta[id].name, (int)id, (void *)h);
    HEALTH_INFO("\tRequired = %s\r\n", id_in_required((uint8_t)id) ? "YES" : "NO");

    if (was_registered) {
        HEALTH_WRN("Re-register on ID = %d Name = %s (Keeping last_seen_ms = %lu)\r\n", (int)id,
                   s_meta[id].name, (unsigned long)s_meta[id].last_seen_ms);
    }
}

void Health_Heartbeat(health_id_t id) {
    if ((int)id < 0 || (int)id >= HEALTH_ID_MAX)
        return;
    if (!s_meta[id].registered)
        return;

    uint32_t t = now_ms();
    s_meta[id].last_seen_ms = t;

#if defined(HEALTH_LOG_BEATS) && (HEALTH_LOG_BEATS)
#ifndef HEALTH_BEAT_LOG_MIN_MS
#define HEALTH_BEAT_LOG_MIN_MS 1000u
#endif
    static uint32_t s_last_log[HEALTH_ID_MAX];
    uint32_t last = s_last_log[(int)id];
    if ((t - last) >= HEALTH_BEAT_LOG_MIN_MS) {
        s_last_log[(int)id] = t;
        HEALTH_DBG("Beat ID = %d Name = %s t = %lu ms\r\n", (int)id,
                   s_meta[id].name ? s_meta[id].name : "task", (unsigned long)t);
    }
#endif
}

void Health_RecordBlocked(const char *tag, uint32_t waited_ms) {
    uint32_t i = __atomic_fetch_add(&s_block_w, 1, __ATOMIC_RELAXED);
    block_evt_t *e = &s_block_ring[i % HEALTH_BLOCK_RING_SIZE];
    e->tick_ms = now_ms();
    e->waited_ms = (uint16_t)(waited_ms > 0xFFFF ? 0xFFFF : waited_ms);
    if (tag) {
        strncpy(e->tag, tag, sizeof(e->tag) - 1);
        e->tag[sizeof(e->tag) - 1] = 0;
    } else {
        e->tag[0] = 0;
    }
    HEALTH_DBG("Blocked tag = %s Waited = %u ms (idx = %lu)\r\n", tag ? tag : "-",
               (unsigned)e->waited_ms, (unsigned long)i);
}

void Health_PrintLastRebootDetailed(void) {
    uint32_t stale, maxdt, silence, tnow, req, flags;
#if PICO_RP2040
    if (watchdog_caused_reboot() && watchdog_hw->scratch[HSCR_MAGIC] == HEALTH_SCRATCH_MAGIC &&
        watchdog_hw->scratch[HSCR_CAUSE] == 1u) {
        stale = watchdog_hw->scratch[HSCR_STALE];
        maxdt = watchdog_hw->scratch[HSCR_MAXDT];
        silence = watchdog_hw->scratch[HSCR_TIMEOUT];
        tnow = watchdog_hw->scratch[HSCR_TNOW];
        req = watchdog_hw->scratch[HSCR_REQMSK];
        flags = watchdog_hw->scratch[HSCR_FLAGS];

        HEALTH_ERR("LAST HEALTH REBOOT: max_dt = %lu ms, Silence = %lu ms, req_mask = 0x%08lx, "
                   "flags = 0x%lx, t = %lu ms\r\n",
                   (unsigned long)maxdt, (unsigned long)silence, (unsigned long)req,
                   (unsigned long)flags, (unsigned long)tnow);

        if (stale) {
            char line[192];
            size_t p = 0;
            p += (size_t)snprintf(line + p, sizeof(line) - p, "Stale tasks: ");
            for (int i = 0; i < HEALTH_ID_MAX; ++i) {
                if ((stale & (1u << i)) == 0)
                    continue;
                const char *nm = s_meta[i].name ? s_meta[i].name : "task";
                p += (size_t)snprintf(line + p, sizeof(line) - p, "%s ", nm);
                if (p >= sizeof(line))
                    break;
            }
            HEALTH_ERR("%s\r\n", line);
        }

        hscr_clear();
    }
#endif
}

/* Add to HealthTask.c */
static void hscr_store_intentional(uint32_t tnow_ms) {
#if PICO_RP2040
    watchdog_hw->scratch[HSCR_MAGIC] = HEALTH_SCRATCH_MAGIC;
    watchdog_hw->scratch[HSCR_CAUSE] = 2u; /* 2 = intentional reboot */
    watchdog_hw->scratch[HSCR_STALE] = 0u;
    watchdog_hw->scratch[HSCR_MAXDT] = 0u;
    watchdog_hw->scratch[HSCR_TIMEOUT] = HEALTH_SILENCE_MS;
    watchdog_hw->scratch[HSCR_TNOW] = tnow_ms;
    watchdog_hw->scratch[HSCR_REQMSK] = (uint32_t)HEALTH_REQUIRED_MASK;
    watchdog_hw->scratch[HSCR_FLAGS] =
        (s_watchdog_armed ? 1u : 0u) | (((tnow_ms - s_start_ms) >= HEALTH_WARMUP_MS) ? 2u : 0u);
#endif
}

/**
 * @brief Immediate, Health-owned reboot with context.
 * @param reason Optional reason for diagnostics.
 */
void Health_RebootNow(const char *reason) {
    uint32_t t = now_ms();
    HEALTH_ERR("INTENTIONAL REBOOT%s%s\r\n", reason ? ": " : "", reason ? reason : "");
    /* Persist context for next boot */
    hscr_store_intentional(t);
    /* Snapshot any crash logs your system wants */
    CrashLog_RecordWdtFeed(t); /* harmless marker; keeps chronology tidy */
    Helpers_EarlyBootSnapshot();
    /* Trigger RP2040 watchdog reset immediately */
    watchdog_reboot(0, 0, 0);
    /* No watchdog_enable() here; reboot is immediate. */
    for (;;) { /* safety spin */
    }
}
