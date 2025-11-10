/* File: misc/HealthTask.c */
#include "../CONFIG.h"

/* ---------- Tag + Logging ---------- */
#ifndef HEALTH_TAG
#define HEALTH_TAG "[Health]"
#endif

#ifndef HEALTH_DBG
#ifdef DEBUG_PRINT
#define HEALTH_DBG(fmt, ...) DEBUG_PRINT("%s " fmt, HEALTH_TAG, ##__VA_ARGS__)
#else
#define HEALTH_DBG(fmt, ...)                                                                       \
    do {                                                                                           \
        (void)0;                                                                                   \
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
#define HEALTH_PERIOD_MS 1000u
#endif
#ifndef HEALTH_LOG_PERIOD_MS
#define HEALTH_LOG_PERIOD_MS 5000u
#endif
#ifndef HEALTH_WARMUP_MS
#define HEALTH_WARMUP_MS 15000u
#endif
#ifndef HEALTH_SILENCE_MS
#define HEALTH_SILENCE_MS 20000u
#endif
#ifndef HEALTH_BLOCK_RING_SIZE
#define HEALTH_BLOCK_RING_SIZE 32u
#endif

/* Require heartbeats from ALL tasks by default */
#ifndef HEALTH_REQUIRED_MASK
#if (HEALTH_ID_MAX >= 32)
#define HEALTH_REQUIRED_MASK (0xFFFFFFFFu)
#else
#define HEALTH_REQUIRED_MASK ((HEALTH_ID_MAX == 0) ? 0u : ((1u << HEALTH_ID_MAX) - 1u))
#endif
#endif

/* ---------- Persisted reboot info (RP2040 watchdog scratch regs) ---------- */
#ifndef HEALTH_SCRATCH_MAGIC
#define HEALTH_SCRATCH_MAGIC (0x484C5448u) /* 'HLTH' */
#endif
#define HSCR_MAGIC 0
#define HSCR_CAUSE 1   /* 0 = unknown, 1 = stale */
#define HSCR_STALE 2   /* stale_mask at reset decision */
#define HSCR_MAXDT 3   /* max dt among required tasks */
#define HSCR_TIMEOUT 4 /* silence threshold used */
#define HSCR_TNOW 5    /* ms timestamp when decision made */
#define HSCR_REQMSK 6  /* required mask used */
#define HSCR_FLAGS 7   /* bit0: armed, bit1: warmup_elapsed */

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
static inline bool hscr_load_is_health_reboot(uint32_t *stale, uint32_t *maxdt, uint32_t *silence,
                                              uint32_t *tnow, uint32_t *req, uint32_t *flags) {
#if PICO_RP2040
    if (watchdog_caused_reboot() && watchdog_hw->scratch[HSCR_MAGIC] == HEALTH_SCRATCH_MAGIC &&
        watchdog_hw->scratch[HSCR_CAUSE] == 1u) {
        if (stale)
            *stale = watchdog_hw->scratch[HSCR_STALE];
        if (maxdt)
            *maxdt = watchdog_hw->scratch[HSCR_MAXDT];
        if (silence)
            *silence = watchdog_hw->scratch[HSCR_TIMEOUT];
        if (tnow)
            *tnow = watchdog_hw->scratch[HSCR_TNOW];
        if (req)
            *req = watchdog_hw->scratch[HSCR_REQMSK];
        if (flags)
            *flags = watchdog_hw->scratch[HSCR_FLAGS];
        return true;
    }
#else
    (void)stale;
    (void)maxdt;
    (void)silence;
    (void)tnow;
    (void)req;
    (void)flags;
#endif
    return false;
}

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

static inline uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

/* ---------- Introspection helpers ---------- */
#if (INCLUDE_uxTaskGetStackHighWaterMark == 1)
static void log_stack_watermarks(void) {
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (s_meta[i].registered && s_meta[i].handle) {
            UBaseType_t hw = uxTaskGetStackHighWaterMark(s_meta[i].handle);
            HEALTH_DBG("stackHW %-10s : %lu words\r\n", s_meta[i].name ? s_meta[i].name : "task",
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

static void log_heap_free(void) {
#if (configUSE_MALLOC_FAILED_HOOK == 1)
    size_t freeb = xPortGetFreeHeapSize();
    HEALTH_DBG("heap_free=%lu bytes\r\n", (unsigned long)freeb);
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

/* Wrap/clamp-safe max gap over required, respects registration and arming. */
static uint32_t max_dt_required(uint32_t now_ms_) {
    uint32_t maxdt = 0;
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (!s_meta[i].registered)
            continue;
        if (!id_in_required((uint8_t)i))
            continue;

        uint32_t last = s_meta[i].last_seen_ms;

        /* Before arming, a NEVER beat shouldn’t poison max_dt. */
        if (!s_watchdog_armed && last == 0)
            continue;

        /* After arming, NEVER is effectively “infinite”. */
        if (s_watchdog_armed && last == 0)
            return HEALTH_SILENCE_MS + 1u;

        /* Clamp against time warp / wrap. */
        uint32_t dt = (now_ms_ >= last) ? (now_ms_ - last) : 0u;
        if (dt > maxdt)
            maxdt = dt;
    }
    return maxdt;
}

/* Wrap/clamp-safe stale detector for required tasks, same policy as report_stale(). */
static uint32_t stale_mask_required(uint32_t now_ms_) {
    uint32_t mask = 0;
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (!s_meta[i].registered)
            continue;
        if (!id_in_required((uint8_t)i))
            continue;

        uint32_t last = s_meta[i].last_seen_ms;

        /* Only treat NEVER as stale after arming. */
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

static void log_required_status(uint32_t current_time) {
    HEALTH_DBG("required beats:\r\n");

    /* Check each task's status */
    for (int task_id = 0; task_id < HEALTH_ID_MAX; ++task_id) {
        if (!id_in_required((uint8_t)task_id)) {
            continue;
        }

        const char *task_name = s_meta[task_id].name ? s_meta[task_id].name : "task";

        if (s_meta[task_id].last_seen_ms == 0) {
            HEALTH_DBG("  %s: NEVER\r\n", task_name);
        } else {
            uint32_t time_since_last_beat = current_time - s_meta[task_id].last_seen_ms;
            HEALTH_DBG("  %s: %lu ms\r\n", task_name, (unsigned long)time_since_last_beat);
        }
    }
}

/* ADD: strict, wrap-safe stale reporter */
static void report_stale(uint32_t now_ms_) {
    /* Recompute mask from scratch */
    uint32_t stale_mask = 0;
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (!s_meta[i].registered)
            continue;
        if (!id_in_required((uint8_t)i))
            continue;

        uint32_t last = s_meta[i].last_seen_ms;
        /* treat NEVER as stale only after arming; during warmup it's ignored */
        if (last == 0) {
            if (s_watchdog_armed)
                stale_mask |= (1u << i);
            continue;
        }

        uint32_t dt = (now_ms_ >= last) ? (now_ms_ - last) : 0u; /* clamp wrap/backward jumps */
        if (dt > HEALTH_SILENCE_MS)
            stale_mask |= (1u << i);
    }

    /* If nothing is stale after filtering, don't cry wolf. */
    if (stale_mask == 0u)
        return;

    /* Build one clean line, no bogus dt values. */
    char line[200];
    size_t p = 0;
    p += (size_t)snprintf(line + p, sizeof(line) - p, "REBOOT PENDING; stale: ");

    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if ((stale_mask & (1u << i)) == 0)
            continue;

        const char *nm = s_meta[i].name ? s_meta[i].name : "task";
        uint32_t last = s_meta[i].last_seen_ms;

        if (last == 0) {
            /* show 'NEVER' explicitly instead of 0 or underflow garbage */
            p += (size_t)snprintf(line + p, sizeof(line) - p, "%s:NEVER ", nm);
        } else {
            uint32_t dt = (now_ms_ >= last) ? (now_ms_ - last) : 0u;
            p += (size_t)snprintf(line + p, sizeof(line) - p, "%s:%lu ", nm, (unsigned long)dt);
        }

        if (p >= sizeof(line))
            break;
    }

    HEALTH_ERR("%s\r\n", line);

    /* Persist context so next-boot dump names real offenders */
    uint32_t maxdt = max_dt_required(now_ms_);
    uint32_t flags =
        (s_watchdog_armed ? 1u : 0u) | (((now_ms_ - s_start_ms) >= HEALTH_WARMUP_MS) ? 2u : 0u);
    (void)flags; /* flags are encoded by hscr_store_stale’s args below */
    hscr_store_stale(stale_mask, maxdt, HEALTH_SILENCE_MS, now_ms_, (uint32_t)HEALTH_REQUIRED_MASK,
                     s_watchdog_armed, ((now_ms_ - s_start_ms) >= HEALTH_WARMUP_MS));
}

/* ---------- Boot cause reporting ---------- */
static void print_health_reboot_brief(void) {
    uint32_t stale, maxdt, silence, tnow, req, flags;
    if (hscr_load_is_health_reboot(&stale, &maxdt, &silence, &tnow, &req, &flags)) {
        HEALTH_ERR("PREVIOUS REBOOT by Health: stale_mask=0x%08lx max_dt=%lu ms silence=%lu ms "
                   "flags=0x%lx req_mask=0x%08lx at t=%lu ms\r\n",
                   (unsigned long)stale, (unsigned long)maxdt, (unsigned long)silence,
                   (unsigned long)flags, (unsigned long)req, (unsigned long)tnow);

        /* Print each stale offender by name */
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

/* Verbose line used in your logs; keep it consistent and explicit. */
static void Health_LogRequiredStatusVerbose(uint32_t now_ms_) {
    char buf[256];
    size_t p = 0;
    p += (size_t)snprintf(buf + p, sizeof(buf) - p, "required beats: ");
    for (int i = 0; i < HEALTH_ID_MAX; ++i) {
        if (!id_in_required((uint8_t)i) || !s_meta[i].registered)
            continue;

        const char *nm = s_meta[i].name ? s_meta[i].name : "task";
        uint32_t last = s_meta[i].last_seen_ms;

        if (last == 0) {
            p += (size_t)snprintf(buf + p, sizeof(buf) - p,
                                  s_watchdog_armed ? "%s(NEVER), " : "%s(PRE-ARM), ", nm);
        } else {
            uint32_t dt = (now_ms_ >= last) ? (now_ms_ - last) : 0u;
            p += (size_t)snprintf(buf + p, sizeof(buf) - p, "%s(%lu ms), ", nm, (unsigned long)dt);
        }
        if (p >= sizeof(buf))
            break;
    }
    HEALTH_DBG("%s\r\n", buf);
}

static void health_task(void *arg) {
    (void)arg;
    s_start_ms = now_ms();

    print_health_reboot_brief();

    const TickType_t period_ticks = pdMS_TO_TICKS(HEALTH_PERIOD_MS);
    const TickType_t log_period = (HEALTH_LOG_PERIOD_MS ? pdMS_TO_TICKS(HEALTH_LOG_PERIOD_MS) : 0);
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t last_log = last_wake;

    HEALTH_DBG("task started, cfg: period=%u ms, warmup=%u ms, silence=%u ms, req_mask=0x%08lx\r\n",
               (unsigned)HEALTH_PERIOD_MS, (unsigned)HEALTH_WARMUP_MS, (unsigned)HEALTH_SILENCE_MS,
               (unsigned long)HEALTH_REQUIRED_MASK);

    for (;;) {
        vTaskDelayUntil(&last_wake, period_ticks);
        uint32_t t = now_ms();

        if (log_period && (xTaskGetTickCount() - last_log) >= log_period) {
            const uint32_t elapsed = t - s_start_ms;
            const uint32_t maxdt = max_dt_required(t);
            const uint32_t remain = (maxdt >= HEALTH_SILENCE_MS) ? 0u : (HEALTH_SILENCE_MS - maxdt);
            HEALTH_DBG("summary: elapsed=%lu ms armed=%u max_dt=%lu ms remain=%lu ms\r\n",
                       (unsigned long)elapsed, s_watchdog_armed ? 1u : 0u, (unsigned long)maxdt,
                       (unsigned long)remain);
            log_required_status(t);
            Health_LogRequiredStatusVerbose(t); /* additive detail */
            log_stack_watermarks();
            log_heap_free();
            dump_block_ring();
            last_log = xTaskGetTickCount();
        }

        if (!s_watchdog_armed) {
            bool warmup_elapsed = (t - s_start_ms) >= HEALTH_WARMUP_MS;
            if (required_tasks_have_heartbeat_once() || warmup_elapsed) {
                HEALTH_WRN("ARMING WATCHDOG after warmup=%u. Timeout=%u ms\r\n",
                           warmup_elapsed ? 1u : 0u, (unsigned)HEALTH_SILENCE_MS);
                watchdog_enable(HEALTH_SILENCE_MS, 1);
                watchdog_update();
                s_watchdog_armed = true;
                HEALTH_DBG("watchdog armed\r\n");
            } else {
                char line[192];
                size_t p = 0;
                p += (size_t)snprintf(line + p, sizeof(line) - p, "waiting for first beats: ");
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
                continue;
            }
        }

        uint32_t maxdt = max_dt_required(t);
        if (maxdt <= HEALTH_SILENCE_MS) {
            uint32_t remain = HEALTH_SILENCE_MS - maxdt;
            watchdog_update();

            /* warning pass (keeps all current logs, just more signal) */
            for (int i = 0; i < HEALTH_ID_MAX; ++i) {
                if (!id_in_required((uint8_t)i) || !s_meta[i].registered)
                    continue;
                uint32_t last = s_meta[i].last_seen_ms;
                if (!last) {
                    HEALTH_WRN("misbehaving: %s NEVER heartbeated\r\n",
                               s_meta[i].name ? s_meta[i].name : "task");
                    continue;
                }
                uint32_t dt = (t >= last) ? (t - last) : 0u;
                if (dt >= (HEALTH_SILENCE_MS * 3) / 4) {
                    HEALTH_WRN("misbehaving: %s dt=%lu ms\r\n",
                               s_meta[i].name ? s_meta[i].name : "task", (unsigned long)dt);
                }
            }

            HEALTH_DBG("feed: max_dt=%lu ms remain=%lu ms\r\n", (unsigned long)maxdt,
                       (unsigned long)remain);
        } else {
            /* CLEAN stale line; no bogus 0/0xFFFFFFFF, no empty prints */
            report_stale(t);
            /* do not feed: allow watchdog to bite */
        }
    }
}

/* ---------- Public API ---------- */
void HealthTask_Start(void) {
    if (s_health_handle)
        return;
    xTaskCreate(health_task, "Health", 1024, NULL, tskIDLE_PRIORITY + 1, &s_health_handle);
    HEALTH_DBG("start requested (created=%u)\r\n", s_health_handle ? 1u : 0u);
}

void Health_RegisterTask(health_id_t id, TaskHandle_t h, const char *name) {
    if ((int)id < 0 || (int)id >= HEALTH_ID_MAX)
        return;

    const bool was_registered = s_meta[id].registered;

    s_meta[id].handle = h;
    s_meta[id].name = name ? name : "task";
    s_meta[id].registered = (h != NULL);

    /* Prime last_seen to NOW on first register to avoid bogus 0xFFFFFFFF prints.
       (If you want pure NEVER reporting, comment the next line.) */
    if (!was_registered && s_meta[id].registered) {
        s_meta[id].last_seen_ms = now_ms();
    }

    HEALTH_DBG("registered id=%d name=%s handle=%p required=%u\r\n", (int)id, s_meta[id].name,
               (void *)h, id_in_required((uint8_t)id) ? 1u : 0u);

    if (was_registered) {
        HEALTH_WRN("re-register id=%d name=%s (keeping last_seen_ms=%lu)\r\n", (int)id,
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
        HEALTH_DBG("beat id=%d name=%s t=%lu ms\r\n", (int)id,
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
    HEALTH_DBG("blocked tag=%s waited=%u ms (idx=%lu)\r\n", tag ? tag : "-", (unsigned)e->waited_ms,
               (unsigned long)i);
}

/* Print detailed explanation of previous Health reboot with task names. */
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

        HEALTH_ERR("LAST HEALTH REBOOT: max_dt=%lu ms silence=%lu ms req_mask=0x%08lx flags=0x%lx "
                   "t=%lu ms\r\n",
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
