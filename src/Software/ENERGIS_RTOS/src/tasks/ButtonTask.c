/**
 * @file button_task.c
 * @author DvidMakesThings - David Sipos
 * @defgroup tasks02 2. Button Task Driver
 * @ingroup tasks
 * @brief FreeRTOS-based button scanner + debouncer for ENERGIS.
 * @{
 *
 * @version 1.0.1
 * @date 2025-11-08
 * @details
 * 1. Polls PLUS/MINUS/SET GPIOs at 5 ms cadence (configurable).
 * 2. Debounces with DEBOUNCE_MS and resolves SET short/long with LONGPRESS_DT.
 * 3. Maintains a 10 s "selection window" with 250 ms blinking on selection row.
 * 4. Emits events on q_btn for higher layers; also performs the classic actions:
 *      PLUS  -> move selection RIGHT (wrap)  [only after window is open]
 *      MINUS -> move selection LEFT  (wrap)  [only after window is open]
 *      SET short -> toggle selected relay (opens window if it was idle)
 *      SET long  -> clear error LED; never opens the window
 * 5. Non-blocking and ISR-free; suitable for RP2040 + FreeRTOS.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* External storage gate (provided by StorageTask module) */
extern bool Storage_IsReady(void);

/* -------------------- Globals ------------------------------------------------ */

/**
 * @brief Global queue for button events consumed by higher layers.
 */
QueueHandle_t q_btn = NULL;

/**
 * @brief Current selected row index (0..7). Used by the task for actions.
 */
static volatile uint8_t s_selected = 0;

/**
 * @brief Internal READY latch for deterministic bring-up.
 */
static volatile bool s_btn_ready = false;

/**
 * @brief Blink timer handle for selection indicator.
 */
static TimerHandle_t s_blink_timer = NULL;

/**
 * @brief Selection window state.
 */
static volatile bool s_window_active = false;
static volatile uint32_t s_last_press_ms = 0;

/* -------------------- Debounce state ---------------------------------------- */

/**
 * @brief Button debouncer state machine.
 */
typedef struct {
    bool stable;             /**< Debounced logic level (true=high, false=low). */
    bool prev_stable;        /**< Previous debounced level (for edge detection). */
    uint32_t last_change_ms; /**< Time of last raw transition. */
    uint32_t stable_since;   /**< Time since level became stable. */
    bool latched_press;      /**< Tracks pending SET short/long resolution. */
} deb_t;

/**
 * @brief Debounce edge flags.
 */
typedef struct {
    bool rose; /**< Debounced low->high transition. */
    bool fell; /**< Debounced high->low transition. */
} deb_edge_t;

/**
 * @brief Debouncer instances for PLUS/MINUS/SET.
 */
static deb_t s_plus, s_minus, s_set;

/* -------------------- Local helpers ----------------------------------------- */

/**
 * @brief Monotonic milliseconds helper.
 * @return Milliseconds since boot.
 */
static inline uint32_t now_ms(void) { return ButtonDrv_NowMs(); }

/**
 * @brief Initialize a debouncer with the current raw level.
 * @param d       Debouncer state.
 * @param level   Current raw level (true=high).
 * @param now     Current time in ms.
 */
static void deb_init(deb_t *d, bool level, uint32_t now) {
    d->stable = level;
    d->prev_stable = level;
    d->last_change_ms = now;
    d->stable_since = now;
    d->latched_press = false;
}

/**
 * @brief Update debouncer with a new raw level.
 * @param d       Debouncer state.
 * @param raw     New raw level (true=high).
 * @param now     Current time in ms.
 * @return deb_edge_t Edge flags if a debounced transition occurred.
 */
static deb_edge_t deb_update(deb_t *d, bool raw, uint32_t now) {
    deb_edge_t e = (deb_edge_t){false, false};

    if (raw != d->stable) {
        /* candidate transition; require DEBOUNCE_MS stable */
        if ((now - d->last_change_ms) >= (uint32_t)DEBOUNCE_MS) {
            d->prev_stable = d->stable;
            d->stable = raw;
            d->stable_since = now;
            if (!d->prev_stable && d->stable)
                e.rose = true;
            if (d->prev_stable && !d->stable)
                e.fell = true;
        }
    } else {
        /* refresh anchor while raw remains equal to debounced */
        d->last_change_ms = now;
    }
    return e;
}

/**
 * @brief Emit a button event into q_btn (non-blocking).
 * @param kind Event kind.
 */
static inline void emit(btn_event_kind_t kind) {
    if (!q_btn)
        return;
    btn_event_t ev = {.kind = kind, .t_ms = now_ms(), .sel = s_selected};
    (void)xQueueSend(q_btn, &ev, 0);
}

/* ----- Selection window control (debounced, task-owned) --------------------- */

static inline void window_open(uint32_t now) {
    s_window_active = true;
    s_last_press_ms = now;
    ButtonDrv_SelectShow(s_selected, true);
}

static inline void window_refresh(uint32_t now) {
    if (s_window_active)
        s_last_press_ms = now;
}

static inline void window_close(void) {
    s_window_active = false;
    ButtonDrv_SelectAllOff();
}

/**
 * @brief Blink timer callback toggles selection LED and enforces timeout.
 *
 * Notes:
 *  - This callback no longer reads buttons. All open/refresh/close decisions
 *    are made by the scanner task based on debounced edges.
 */
static void vBlinkTimerCb(TimerHandle_t xTimer) {
    (void)xTimer;
    static bool on = false;

    uint32_t now = now_ms();

    /* Timeout handling */
    if (s_window_active && (now - s_last_press_ms) >= (uint32_t)SELECT_WINDOW_MS) {
        window_close();
        on = false;
        return;
    }

    /* Blink only while window is open */
    if (s_window_active) {
        on = !on;
        ButtonDrv_SelectShow(s_selected, on);
    } else {
        on = false;
        ButtonDrv_SelectAllOff();
    }
}

/* -------------------- Task --------------------------------------------------- */

/**
 * @brief Button scanning task: debounce, selection window, and actions.
 *
 * Behavior rules implemented:
 * - PLUS/MINUS: on first valid FALL when idle → open window only (no step).
 * - PLUS/MINUS: when window active → step right/left and refresh window timer.
 * - SET: open window ONLY if the press resolves as SHORT; LONG never opens it.
 * - SET short: toggles output; also opens window if it was idle.
 *
 * @param arg Unused.
 */
static void vButtonTask(void *arg) {
    (void)arg;

    ButtonDrv_InitGPIO();

    /* Initialize debouncers with current levels */
    uint32_t t0 = now_ms();
    deb_init(&s_plus, ButtonDrv_ReadPlus(), t0);
    deb_init(&s_minus, ButtonDrv_ReadMinus(), t0);
    deb_init(&s_set, ButtonDrv_ReadSet(), t0);

    /* Start with selection LEDs off; blink timer will drive visibility */
    ButtonDrv_SelectAllOff();
    s_window_active = false;

    const TickType_t scan_ticks = pdMS_TO_TICKS(BTN_SCAN_PERIOD_MS);

    for (;;) {
        uint32_t t = now_ms();

        /* Sample raw levels (true = not pressed) */
        bool r_plus = ButtonDrv_ReadPlus();
        bool r_minus = ButtonDrv_ReadMinus();
        bool r_set = ButtonDrv_ReadSet();
        (void)r_plus;
        (void)r_minus;
        (void)r_set;

        /* Debounce -> edges */
        deb_edge_t e_plus = deb_update(&s_plus, ButtonDrv_ReadPlus(), t);
        deb_edge_t e_minus = deb_update(&s_minus, ButtonDrv_ReadMinus(), t);
        deb_edge_t e_set = deb_update(&s_set, ButtonDrv_ReadSet(), t);

        /* PLUS */
        if (e_plus.fell) {
            if (!s_window_active) {
                window_open(t); /* open only, no step */
            } else {
                ButtonDrv_SelectRight((uint8_t *)&s_selected, true);
                window_refresh(t);
            }
            emit(BTN_EV_PLUS_FALL);
        }
        if (e_plus.rose) {
            emit(BTN_EV_PLUS_RISE);
        }

        /* MINUS */
        if (e_minus.fell) {
            if (!s_window_active) {
                window_open(t); /* open only, no step */
            } else {
                ButtonDrv_SelectLeft((uint8_t *)&s_selected, true);
                window_refresh(t);
            }
            emit(BTN_EV_MINUS_FALL);
        }
        if (e_minus.rose) {
            emit(BTN_EV_MINUS_RISE);
        }

        /* SET short/long */
        if (e_set.fell) {
            s_set.latched_press = true; /* candidate for long */
            /* Do NOT open window here; wait until we know if short */
        }
        if (!s_set.stable && s_set.latched_press) {
            uint32_t held_ms = (uint32_t)(t - s_set.stable_since);
            if (held_ms >= (uint32_t)LONGPRESS_DT) {
                /* Long press resolves here: never open the window */
                s_set.latched_press = false;
                ButtonDrv_DoSetLong();
                emit(BTN_EV_SET_LONG);
                /* If window was open already, we can optionally close it;
                   spec says long never opens; it does not forbid closing. */
                window_close();
            }
        }
        if (e_set.rose) {
            if (s_set.latched_press) {
                /* Short press resolved */
                s_set.latched_press = false;
                if (!s_window_active) {
                    window_open(t); /* Open on short if idle */
                } else {
                    window_refresh(t);
                }
                ButtonDrv_DoSetShort(s_selected);
                emit(BTN_EV_SET_SHORT);
            }
        }

        vTaskDelay(scan_ticks);
    }
}

/* -------------------- Public API -------------------------------------------- */

/**
 * @brief Create and start the ButtonTask with an enable gate (bring-up step 4/6).
 *
 * @param enable Set true to create/start; false to skip deterministically.
 * @return pdPASS on success (or when skipped), pdFAIL on error/timeout.
 */
BaseType_t ButtonTask_Init(bool enable) {
    s_btn_ready = false;

    if (!enable) {
        return pdPASS;
    }

    /* Bring-up gate: wait for Storage config to be ready */
    TickType_t t0 = xTaskGetTickCount();
    const TickType_t to = pdMS_TO_TICKS(BUTTON_WAIT_STORAGE_READY_MS);
    while (!Storage_IsReady()) {
        if ((xTaskGetTickCount() - t0) >= to) {
            return pdFAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Create event queue if missing */
    if (!q_btn) {
        q_btn = xQueueCreate(32, sizeof(btn_event_t));
        if (!q_btn)
            return pdFAIL;
    }

    /* Create and start blink timer */
    if (!s_blink_timer) {
        s_blink_timer =
            xTimerCreate("btn_blink", pdMS_TO_TICKS(SELECT_BLINK_MS), pdTRUE, NULL, vBlinkTimerCb);
        if (!s_blink_timer)
            return pdFAIL;
        if (xTimerStart(s_blink_timer, 0) != pdPASS)
            return pdFAIL;
    }

    /* Spawn the scanner task */
    if (xTaskCreate(vButtonTask, "ButtonTask", 1024, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        return pdFAIL;
    }

    s_btn_ready = true;
    return pdPASS;
}

/**
 * @brief Ready-state query for deterministic boot sequencing.
 * @return true if ButtonTask_Init(true) completed successfully, else false.
 */
bool Button_IsReady(void) { return s_btn_ready; }

/** @} */
