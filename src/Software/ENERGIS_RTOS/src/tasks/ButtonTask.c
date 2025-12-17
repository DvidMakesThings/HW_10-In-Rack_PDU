/**
 * @file src/tasks/button_task.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2.0
 * @date 2025-12-09
 * @details
 * 1. Polls PLUS/MINUS/SET/PWR GPIOs at 5 ms cadence (configurable).
 * 2. Debounces with DEBOUNCE_MS and resolves SET/PWR short/long with LONGPRESS_DT.
 * 3. Maintains a 10 s "selection window" with 250 ms blinking on selection row.
 * 4. Emits events on q_btn for higher layers; also performs the classic actions:
 * PLUS  -> move selection RIGHT (wrap)  [only after window is open]
 * MINUS -> move selection LEFT  (wrap)  [only after window is open]
 * SET short -> toggle selected relay (opens window if it was idle)
 * SET long  -> clear error LED; never opens the window
 * PWR long  -> enter STANDBY mode
 * PWR short (in STANDBY) -> exit STANDBY mode
 *
 * Version 1.2.0 changes:
 * - FIX: Moved I2C blink operations from timer callback to main task loop.
 * - Timer now only sets a flag; ButtonTask main loop handles actual LED toggling.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* External storage gate (provided by StorageTask module) */
extern bool Storage_IsReady(void);

#define BUTTON_TASK_TAG "[BUTTONTASK]"

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

/**
 * @brief Blink request flag - set by timer callback, cleared by task after processing.
 * @details This decouples the timer callback from I2C operations. The timer only
 * sets this flag; the main task loop queues LED changes via SwitchTask.
 */
static volatile bool s_blink_pending = false;

/**
 * @brief Current blink state (on/off) for selection LED.
 */
static volatile bool s_blink_state = false;

/**
 * @brief Last blink update timestamp for rate limiting in main loop.
 */
static volatile uint32_t s_last_blink_ms = 0;

/* -------------------- Debounce state ---------------------------------------- */

/**
 * @brief Button debouncer state machine.
 */
typedef struct {
    bool stable;             /**< Debounced logic level (true=high, false=low). */
    bool prev_stable;        /**< Previous debounced level (for edge detection). */
    uint32_t last_change_ms; /**< Time of last raw transition. */
    uint32_t stable_since;   /**< Time since level became stable. */
    bool latched_press;      /**< Tracks pending SET/PWR short/long resolution. */
} deb_t;

/**
 * @brief Debounce edge flags.
 */
typedef struct {
    bool rose; /**< Debounced low->high transition. */
    bool fell; /**< Debounced high->low transition. */
} deb_edge_t;

/**
 * @brief Debouncer instances for PLUS/MINUS/SET/PWR.
 */
static deb_t s_plus, s_minus, s_set, s_pwr;

/* ##################################################################### */
/*                           INTERNAL HELPERS                            */
/* ##################################################################### */

/**
 * @brief Monotonic milliseconds helper.
 *
 * @return Milliseconds since boot.
 */
static inline uint32_t now_ms(void) { return ButtonDrv_NowMs(); }

/**
 * @brief Initialize a debouncer with the current raw level.
 *
 * @param d       Debouncer state.
 * @param level   Current raw level (true=high).
 * @param now     Current time in ms.
 * @return None
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
 *
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
 *
 * @param kind Event kind.
 * @return None
 */
static inline void emit(btn_event_kind_t kind) {
    if (!q_btn) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTONTASK, 0x0);
        ERROR_PRINT_CODE(errorcode, "%s Button event queue not initialized\r\n", BUTTON_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }
    btn_event_t ev = {.kind = kind, .t_ms = now_ms(), .sel = s_selected};
    (void)xQueueSend(q_btn, &ev, 0);
}

/* ----- Selection window control (debounced, task-owned) --------------------- */

/**
 * @brief Open the selection window and start blinking.
 *
 * @param now Current time in ms.
 * @return None
 */
static inline void window_open(uint32_t now) {
    s_window_active = true;
    s_last_press_ms = now;
    s_blink_state = true;
    s_last_blink_ms = now;
    /* Enable manual selection activity: allow 0x23 writes during user interaction */
    Switch_SetManualPanelActive(true);
    ButtonDrv_SelectShow(s_selected, true);
}

/**
 * @brief Refresh the selection window timeout.
 *
 * @param now Current time in ms.
 * @return None
 */
static inline void window_refresh(uint32_t now) {
    if (s_window_active)
        s_last_press_ms = now;
}

/**
 * @brief Close the selection window and turn off LEDs.
 *
 * @return None
 */
static inline void window_close(void) {
    s_window_active = false;
    s_blink_state = false;
    ButtonDrv_SelectAllOff();
    /* Disable manual selection activity after clearing LEDs */
    Switch_SetManualPanelActive(false);
}

/**
 * @brief Blink timer callback - ONLY sets a flag, NO I2C operations.
 *
 * @param xTimer Timer handle (unused).
 * @return None
 *
 * @details CRITICAL: Timer callbacks run from the timer daemon task context.
 * Performing blocking I2C operations here causes bus collisions with other
 * tasks (e.g., NetTask SNMP) using the same I2C bus, leading to watchdog
 * starvation during stress tests. The actual LED update is queued from the ButtonTask main loop
 * when s_blink_pending is set. SwitchTask performs the MCP23017 I2C operation.
 */
static void vBlinkTimerCb(TimerHandle_t xTimer) {
    (void)xTimer;

    /* Only set the flag - actual I2C work happens in task context */
    s_blink_pending = true;
}

/**
 * @brief Process blink logic in task context (safe for I2C operations).
 *
 * @param now Current time in ms.
 * @return None
 *
 * @details Called from ButtonTask main loop to handle selection LED blinking.
 * ButtonTask does not perform any I2C operations. LED updates are queued to
 * SwitchTask which owns the MCP23017 I2C bus operations.
 */
static void process_blink(uint32_t now) {
    /* Only process if timer fired */
    if (!s_blink_pending) {
        return;
    }
    s_blink_pending = false;

    /* Rate limit to prevent I2C flooding */
    if ((now - s_last_blink_ms) < (SELECT_BLINK_MS / 2)) {
        return;
    }
    s_last_blink_ms = now;

    /* Timeout handling - close window if expired */
    if (s_window_active && (now - s_last_press_ms) >= (uint32_t)SELECT_WINDOW_MS) {
        window_close();
        return;
    }

    /* Blink only while window is open */
    if (s_window_active) {
        s_blink_state = !s_blink_state;
        ButtonDrv_SelectShow(s_selected, s_blink_state);
    } else {
        s_blink_state = false;
        ButtonDrv_SelectAllOff();
    }
}

/**
 * @brief Read BUT_PWR GPIO state.
 *
 * @return true if pin is HIGH (not pressed), false if LOW (pressed, active-low).
 */
static inline bool read_pwr_button(void) { return gpio_get(BUT_PWR) ? true : false; }

/**
 * @brief Button scanning task: debounce, selection window, and actions.
 *
 * Behavior rules implemented:
 * - PLUS/MINUS: on first valid FALL when idle → open window only (no step).
 * - PLUS/MINUS: when window active → step right/left and refresh window timer.
 * - SET: short = toggle relay; long = clear error LED.
 * - PWR: long = enter STANDBY mode; short (in STANDBY) = exit STANDBY mode.
 *
 * @param pvParameters Unused.
 * @return None
 */
static void vButtonTask(void *pvParameters) {
    (void)pvParameters;

    /* Initialize GPIO for PWR button (others done by driver init) */
    gpio_init(BUT_PWR);
    gpio_pull_up(BUT_PWR);
    gpio_set_dir(BUT_PWR, false);

    /* Initialize debouncers with current levels */
    uint32_t t0 = now_ms();
    deb_init(&s_plus, ButtonDrv_ReadPlus(), t0);
    deb_init(&s_minus, ButtonDrv_ReadMinus(), t0);
    deb_init(&s_set, ButtonDrv_ReadSet(), t0);
    deb_init(&s_pwr, read_pwr_button(), t0);

    /* Start with selection LEDs off; blink logic in main loop will drive visibility */
    ButtonDrv_SelectAllOff();
    s_window_active = false;
    s_blink_pending = false;
    s_blink_state = false;
    s_last_blink_ms = t0;

    const TickType_t scan_ticks = pdMS_TO_TICKS(BTN_SCAN_PERIOD_MS);
    uint32_t hb_btn_ms = now_ms();

    for (;;) {
        uint32_t now = now_ms();

        /* Heartbeat */
        if ((now - hb_btn_ms) >= (uint32_t)BUTTONTASKBEAT_MS) {
            hb_btn_ms = now;
            Health_Heartbeat(HEALTH_ID_BUTTON);
        }

        /* Service standby LED animation if in standby */
        Power_ServiceStandbyLED();

        /* Query current power state */
        power_state_t pwr_state = Power_GetState();

        /* Process blink logic in task context (safe for I2C) */
        process_blink(now);

        /* Debounce all buttons using current timebase */
        deb_edge_t e_plus = deb_update(&s_plus, ButtonDrv_ReadPlus(), now);
        deb_edge_t e_minus = deb_update(&s_minus, ButtonDrv_ReadMinus(), now);
        deb_edge_t e_set = deb_update(&s_set, ButtonDrv_ReadSet(), now);
        deb_edge_t e_pwr = deb_update(&s_pwr, read_pwr_button(), now);

        /* ===== PWR Button Handling (always active) ===== */
        if (e_pwr.fell) {
            s_pwr.latched_press = true; /* candidate for long */
        }

        /* Long-press detection while still held */
        if (!s_pwr.stable && s_pwr.latched_press) {
            uint32_t held_ms = (uint32_t)(now - s_pwr.stable_since);
            if (held_ms >= (uint32_t)LONGPRESS_DT) {
                s_pwr.latched_press = false;
                if (pwr_state == PWR_STATE_RUN) {
                    /* Long press in RUN mode: enter standby */
                    DEBUG_PRINT("[ButtonTask] PWR long press detected, entering STANDBY\r\n");
                    Power_EnterStandby();
                    window_close(); /* Close selection window on standby entry */
                }
                /* Long press in STANDBY is ignored */
            }
        }

        if (e_pwr.rose) {
            if (s_pwr.latched_press) {
                s_pwr.latched_press = false; /* resolves as short */
                if (pwr_state == PWR_STATE_STANDBY) {
                    /* Short press in STANDBY: exit to RUN mode */
                    DEBUG_PRINT("[ButtonTask] PWR short press detected, exiting STANDBY\r\n");
                    Power_ExitStandby();
                }
                /* Short press in RUN mode has no action */
            }
        }

        /* ===== All other buttons: ONLY active in RUN mode ===== */
        if (pwr_state != PWR_STATE_RUN) {
            /* In STANDBY: ignore all non-PWR buttons */
            vTaskDelay(scan_ticks);
            continue;
        }

        /* PLUS */
        if (e_plus.fell) {
            if (!s_window_active) {
                window_open(now); /* open only, no step */
            } else {
                ButtonDrv_SelectRight((uint8_t *)&s_selected, true);
                window_refresh(now);
            }
            emit(BTN_EV_PLUS_FALL);
        }
        if (e_plus.rose) {
            emit(BTN_EV_PLUS_RISE);
        }

        /* MINUS */
        if (e_minus.fell) {
            if (!s_window_active) {
                window_open(now); /* open only, no step */
            } else {
                ButtonDrv_SelectLeft((uint8_t *)&s_selected, true);
                window_refresh(now);
            }
            emit(BTN_EV_MINUS_FALL);
        }
        if (e_minus.rose) {
            emit(BTN_EV_MINUS_RISE);
        }

        /* SET short/long */
        if (e_set.fell) {
            s_set.latched_press = true; /* candidate for long */
            /* do not open window yet; only on short */
        }

        /* Long-press detection while still held */
        if (!s_set.stable && s_set.latched_press) {
            uint32_t held_ms = (uint32_t)(now - s_set.stable_since);
            if (held_ms >= (uint32_t)LONGPRESS_DT) {
                /* Long press: clear error/warning history and error LED */
                s_set.latched_press = false;
                (void)storage_clear_error_log_async();
                Health_Heartbeat(HEALTH_ID_STORAGE);
                vTaskDelay(pdMS_TO_TICKS(10));
                (void)storage_clear_warning_log_async();
                Health_Heartbeat(HEALTH_ID_STORAGE);
                vTaskDelay(pdMS_TO_TICKS(10));
                Switch_SetFaultLed(false, 0);
                emit(BTN_EV_SET_LONG);
                window_close();
            }
        }

        if (e_set.rose) {
            if (s_set.latched_press) {
                s_set.latched_press = false; /* resolves as short */
                if (!s_window_active) {
                    /* First interaction: open window ONLY, no action */
                    window_open(now);
                    /* no BTN_EV_SET_SHORT emit here since no action performed */
                } else {
                    /* Window already open -> perform short action */
                    window_refresh(now);
                    ButtonDrv_DoSetShort(s_selected);
                    emit(BTN_EV_SET_SHORT);
                }
            }
        }

        vTaskDelay(scan_ticks);
    }
}

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

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
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTONTASK, 0x1);
            ERROR_PRINT_CODE(errorcode,
                             "%s Storage not ready within timeout, cannot start ButtonTask\r\n",
                             BUTTON_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
            return pdFAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Initialize power manager */
    Power_Init();

    /* Create event queue if missing */
    if (!q_btn) {
        q_btn = xQueueCreate(32, sizeof(btn_event_t));
        if (!q_btn) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTONTASK, 0x2);
            ERROR_PRINT_CODE(errorcode, "%s Button event queue create failed\r\n", BUTTON_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
            return pdFAIL;
        }
    }

    /* Create and start blink timer - now only sets a flag, no I2C operations */
    if (!s_blink_timer) {
        s_blink_timer =
            xTimerCreate("btn_blink", pdMS_TO_TICKS(SELECT_BLINK_MS), pdTRUE, NULL, vBlinkTimerCb);
        if (!s_blink_timer) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTONTASK, 0x3);
            ERROR_PRINT_CODE(errorcode, "%s Blink timer create failed\r\n", BUTTON_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
            return pdFAIL;
        }
        /* avoid 0 block time & let lower prio run */
        if (xTimerStart(s_blink_timer, pdMS_TO_TICKS(10)) != pdPASS)
            return pdFAIL;
    }

    /* Spawn the scanner task */
    if (xTaskCreate(vButtonTask, "ButtonTask", 1024, NULL, BUTTONTASK_PRIORITY, NULL) != pdPASS) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTONTASK, 0x4);
        ERROR_PRINT_CODE(errorcode, "%s ButtonTask create failed\r\n", BUTTON_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return pdFAIL;
    }

    s_btn_ready = true;
    return pdPASS;
}

/**
 * @brief Ready-state query for deterministic boot sequencing.
 *
 * @return true if ButtonTask_Init(true) completed successfully, else false.
 */
bool Button_IsReady(void) { return s_btn_ready; }
