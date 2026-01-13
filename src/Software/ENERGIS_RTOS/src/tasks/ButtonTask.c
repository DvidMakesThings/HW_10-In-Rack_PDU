/**
 * @file src/tasks/ButtonTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.2.0
 * @date 2025-12-09
 *
 * @brief Button task implementation with debouncing and selection window control.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

extern bool Storage_IsReady(void);

#define BUTTON_TASK_TAG "[BUTTONTASK]"

/* Module State Variables */

/** Global queue for button events consumed by higher layers. */
QueueHandle_t q_btn = NULL;

/** Current selected output channel index (0-7). */
static volatile uint8_t s_selected = 0;

/** Task initialization completion flag for boot sequencing. */
static volatile bool s_btn_ready = false;

/** Timer handle for selection LED blinking. */
static TimerHandle_t s_blink_timer = NULL;

/** Selection window active state flag. */
static volatile bool s_window_active = false;

/** Timestamp of last button press for window timeout calculation. */
static volatile uint32_t s_last_press_ms = 0;

/**
 * Blink request flag set by timer callback, processed by main task loop.
 * Decouples timer ISR from I2C operations to prevent bus collisions.
 */
static volatile bool s_blink_pending = false;

/** Current selection LED state (true=on, false=off). */
static volatile bool s_blink_state = false;

/** Timestamp of last LED blink update for rate limiting. */
static volatile uint32_t s_last_blink_ms = 0;

/* Debounce State Management */

/**
 * @brief Button debouncer state structure.
 *
 * Maintains state for software button debouncing algorithm. Tracks both
 * raw and stable button levels to detect valid transitions after the
 * debounce period expires.
 */
typedef struct {
    bool stable;             /**< Current stable debounced level (true=high, false=low). */
    bool prev_stable;        /**< Previous stable level for edge detection. */
    uint32_t last_change_ms; /**< Timestamp of last raw level transition. */
    uint32_t stable_since;   /**< Timestamp when current stable level was confirmed. */
    bool latched_press;      /**< Press event pending short/long resolution (SET/PWR only). */
} deb_t;

/**
 * @brief Debouncer edge detection result.
 *
 * Returned by debouncer update function to indicate detected transitions
 * after debouncing is complete.
 */
typedef struct {
    bool rose; /**< True if debounced rising edge (low to high) occurred. */
    bool fell; /**< True if debounced falling edge (high to low) occurred. */
} deb_edge_t;

/** Debouncer state for PLUS button. */
static deb_t s_plus;

/** Debouncer state for MINUS button. */
static deb_t s_minus;

/** Debouncer state for SET button. */
static deb_t s_set;

/** Debouncer state for PWR button. */
static deb_t s_pwr;

/* Private Helper Functions */

/**
 * @brief Get current system time in milliseconds.
 *
 * Provides monotonic millisecond timestamp for debouncing and timing operations.
 * Wraps button driver timing function.
 *
 * @return Milliseconds elapsed since system boot.
 */
static inline uint32_t now_ms(void) { return ButtonDrv_NowMs(); }

/**
 * @brief Initialize debouncer state.
 *
 * Sets up a debouncer with the current raw button level and timestamp.
 * Must be called before using the debouncer in the update loop.
 *
 * @param[out] d     Pointer to debouncer state structure to initialize.
 * @param[in]  level Initial raw button level (true=high/unpressed, false=low/pressed).
 * @param[in]  now   Current timestamp in milliseconds.
 */
static void deb_init(deb_t *d, bool level, uint32_t now) {
    d->stable = level;
    d->prev_stable = level;
    d->last_change_ms = now;
    d->stable_since = now;
    d->latched_press = false;
}

/**
 * @brief Update debouncer with new raw button reading.
 *
 * Implements software debouncing by requiring a stable level for DEBOUNCE_MS
 * before accepting a transition. Detects rising and falling edges after
 * debouncing completes.
 *
 * @param[in,out] d   Pointer to debouncer state.
 * @param[in]     raw Current raw button level from GPIO.
 * @param[in]     now Current timestamp in milliseconds.
 *
 * @return Structure with edge flags indicating detected transitions.
 */
static deb_edge_t deb_update(deb_t *d, bool raw, uint32_t now) {
    deb_edge_t e = (deb_edge_t){false, false};

    /* Check if raw level differs from stable debounced level */
    if (raw != d->stable) {
        /* Candidate transition detected, check if stable long enough */
        if ((now - d->last_change_ms) >= (uint32_t)DEBOUNCE_MS) {
            /* Debounce period elapsed, accept transition */
            d->prev_stable = d->stable;
            d->stable = raw;
            d->stable_since = now;

            /* Detect edge direction */
            if (!d->prev_stable && d->stable)
                e.rose = true;
            if (d->prev_stable && !d->stable)
                e.fell = true;
        }
    } else {
        /* Raw level matches stable, reset debounce timer */
        d->last_change_ms = now;
    }
    return e;
}

/**
 * @brief Publish button event to event queue.
 *
 * Creates a button event structure with the specified type and current state,
 * then publishes it non-blocking to the global event queue for consumption
 * by other tasks.
 *
 * @param[in] kind Button event type to publish.
 */
static inline void emit(btn_event_kind_t kind) {
    /* Validate queue handle */
    if (!q_btn) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTONTASK, 0x0);
        ERROR_PRINT_CODE(errorcode, "%s Button event queue not initialized\r\n", BUTTON_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    /* Build and send event */
    btn_event_t ev = {.kind = kind, .t_ms = now_ms(), .sel = s_selected};
    (void)xQueueSend(q_btn, &ev, 0);
}

/**
 * @brief Activate selection window and enable LED feedback.
 *
 * Opens the selection window, initializes the blink state, and notifies
 * the switch task to allow manual panel interaction. LED is turned on
 * immediately at the selected channel.
 *
 * @param[in] now Current timestamp in milliseconds.
 */
static inline void window_open(uint32_t now) {
    s_window_active = true;
    s_last_press_ms = now;
    s_blink_state = true;
    s_last_blink_ms = now;

    /* Allow manual panel writes during selection mode */
    Switch_SetManualPanelActive(true);
    ButtonDrv_SelectShow(s_selected, true);
}

/**
 * @brief Extend selection window timeout.
 *
 * Updates the last activity timestamp to prevent window timeout. Called
 * on each button interaction while window is active.
 *
 * @param[in] now Current timestamp in milliseconds.
 */
static inline void window_refresh(uint32_t now) {
    if (s_window_active)
        s_last_press_ms = now;
}

/**
 * @brief Deactivate selection window and disable LED feedback.
 *
 * Closes the selection window, turns off all selection LEDs, and notifies
 * the switch task to resume normal panel operation.
 */
static inline void window_close(void) {
    s_window_active = false;
    s_blink_state = false;
    ButtonDrv_SelectAllOff();

    /* Resume normal panel operation */
    Switch_SetManualPanelActive(false);
}

/**
 * @brief Blink timer callback function.
 *
 * Periodic timer callback that requests LED blink update by setting a flag.
 * Does not perform any I2C operations directly to avoid bus conflicts with
 * other tasks using the shared I2C bus. The main task loop processes the
 * blink request in a safe context.
 *
 * @param[in] xTimer Timer handle (unused).
 *
 * @note Runs in timer daemon task context, not ButtonTask context.
 * @note I2C operations from timer context cause bus collisions and watchdog issues.
 */
static void vBlinkTimerCb(TimerHandle_t xTimer) {
    (void)xTimer;

    /* Request blink update, processed by main task loop */
    s_blink_pending = true;
}

/**
 * @brief Process LED blink logic and window timeout.
 *
 * Handles selection LED blinking and window timeout management in ButtonTask
 * context where I2C operations are safe. Toggles LED state, checks for window
 * timeout, and queues LED updates through button driver to switch task.
 *
 * @param[in] now Current timestamp in milliseconds.
 *
 * @note Called from main ButtonTask loop only.
 * @note Rate-limited to prevent I2C bus flooding.
 */
static void process_blink(uint32_t now) {
    /* Check if timer requested blink update */
    if (!s_blink_pending) {
        return;
    }
    s_blink_pending = false;

    /* Rate limit blink updates */
    if ((now - s_last_blink_ms) < (SELECT_BLINK_MS / 2)) {
        return;
    }
    s_last_blink_ms = now;

    /* Check for window timeout */
    if (s_window_active && (now - s_last_press_ms) >= (uint32_t)SELECT_WINDOW_MS) {
        window_close();
        return;
    }

    /* Toggle LED state while window is active */
    if (s_window_active) {
        s_blink_state = !s_blink_state;
        ButtonDrv_SelectShow(s_selected, s_blink_state);
    } else {
        /* Ensure LEDs are off when window is closed */
        s_blink_state = false;
        ButtonDrv_SelectAllOff();
    }
}

/**
 * @brief Read power button GPIO state.
 *
 * Reads the current state of the power button GPIO pin. Button uses active-low
 * logic where low indicates pressed state.
 *
 * @return true if button is not pressed (pin high), false if pressed (pin low).
 */
static inline bool read_pwr_button(void) { return gpio_get(BUT_PWR) ? true : false; }

/**
 * @brief Main button scanning and event processing task.
 *
 * Continuously polls button GPIOs, performs debouncing, manages the selection
 * window, and executes button actions. Handles all button logic including
 * short/long press detection, power state control, and event publishing.
 *
 * Button Actions:
 * - PLUS/MINUS (first press): Open selection window without changing selection
 * - PLUS/MINUS (window active): Move selection and refresh timeout
 * - SET (short): Toggle selected relay output
 * - SET (long): Clear error/warning logs and fault LED
 * - PWR (long in RUN mode): Enter standby mode
 * - PWR (short in STANDBY): Exit standby mode
 *
 * @param[in] pvParameters Task parameters (unused).
 */
static void vButtonTask(void *pvParameters) {
    (void)pvParameters;

    /* Initialize power button GPIO with pull-up */
    gpio_init(BUT_PWR);
    gpio_pull_up(BUT_PWR);
    gpio_set_dir(BUT_PWR, false);

    /* Initialize all button debouncers with current GPIO levels */
    uint32_t t0 = now_ms();
    deb_init(&s_plus, ButtonDrv_ReadPlus(), t0);
    deb_init(&s_minus, ButtonDrv_ReadMinus(), t0);
    deb_init(&s_set, ButtonDrv_ReadSet(), t0);
    deb_init(&s_pwr, read_pwr_button(), t0);

    /* Initialize selection window state */
    ButtonDrv_SelectAllOff();
    s_window_active = false;
    s_blink_pending = false;
    s_blink_state = false;
    s_last_blink_ms = t0;

    const TickType_t scan_ticks = pdMS_TO_TICKS(BTN_SCAN_PERIOD_MS);
    uint32_t hb_btn_ms = now_ms();

    /* Main event loop */
    for (;;) {
        uint32_t now = now_ms();

        /* Send periodic heartbeat to health monitor */
        if ((now - hb_btn_ms) >= (uint32_t)BUTTONTASKBEAT_MS) {
            hb_btn_ms = now;
            Health_Heartbeat(HEALTH_ID_BUTTON);
        }

        /* Update standby LED animation if needed */
        Power_ServiceStandbyLED();

        /* Check current power state for conditional button handling */
        power_state_t pwr_state = Power_GetState();

        /* Handle LED blinking and window timeout */
        process_blink(now);

        /* Update all button debouncers with current GPIO states */
        deb_edge_t e_plus = deb_update(&s_plus, ButtonDrv_ReadPlus(), now);
        deb_edge_t e_minus = deb_update(&s_minus, ButtonDrv_ReadMinus(), now);
        deb_edge_t e_set = deb_update(&s_set, ButtonDrv_ReadSet(), now);
        deb_edge_t e_pwr = deb_update(&s_pwr, read_pwr_button(), now);

        /* Power button processing (active in all power states) */
        if (e_pwr.fell) {
            /* Button pressed, start tracking for long press */
            s_pwr.latched_press = true;
        }

        /* Detect long press while button is held */
        if (!s_pwr.stable && s_pwr.latched_press) {
            uint32_t held_ms = (uint32_t)(now - s_pwr.stable_since);
            if (held_ms >= (uint32_t)LONGPRESS_DT) {
                /* Long press threshold reached */
                s_pwr.latched_press = false;
                if (pwr_state == PWR_STATE_RUN) {
                    /* Enter standby from normal operation */
                    DEBUG_PRINT("[ButtonTask] PWR long press detected, entering STANDBY\r\n");
                    Power_EnterStandby();
                    window_close();
                }
            }
        }

        if (e_pwr.rose) {
            /* Button released, check if short press */
            if (s_pwr.latched_press) {
                s_pwr.latched_press = false;
                if (pwr_state == PWR_STATE_STANDBY) {
                    /* Short press in standby: wake up */
                    DEBUG_PRINT("[ButtonTask] PWR short press detected, exiting STANDBY\r\n");
                    Power_ExitStandby();
                }
            }
        }

        /* Remaining buttons only active in normal operation mode */
        if (pwr_state != PWR_STATE_RUN) {
            vTaskDelay(scan_ticks);
            continue;
        }

        /* PLUS button processing */
        if (e_plus.fell) {
            if (!s_window_active) {
                /* First press opens window without changing selection */
                window_open(now);
            } else {
                /* Subsequent presses move selection right */
                ButtonDrv_SelectRight((uint8_t *)&s_selected, true);
                window_refresh(now);
            }
            emit(BTN_EV_PLUS_FALL);
        }
        if (e_plus.rose) {
            emit(BTN_EV_PLUS_RISE);
        }

        /* MINUS button processing */
        if (e_minus.fell) {
            if (!s_window_active) {
                /* First press opens window without changing selection */
                window_open(now);
            } else {
                /* Subsequent presses move selection left */
                ButtonDrv_SelectLeft((uint8_t *)&s_selected, true);
                window_refresh(now);
            }
            emit(BTN_EV_MINUS_FALL);
        }
        if (e_minus.rose) {
            emit(BTN_EV_MINUS_RISE);
        }

        /* SET button short/long press handling */
        if (e_set.fell) {
            /* Button pressed, start tracking for long press */
            s_set.latched_press = true;
        }

        /* Detect long press while button is held */
        if (!s_set.stable && s_set.latched_press) {
            uint32_t held_ms = (uint32_t)(now - s_set.stable_since);
            if (held_ms >= (uint32_t)LONGPRESS_DT) {
                /* Long press: clear error logs and fault LED */
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
            /* Button released, check if short press */
            if (s_set.latched_press) {
                s_set.latched_press = false;
                if (!s_window_active) {
                    /* First interaction opens window only */
                    window_open(now);
                } else {
                    /* Window active: toggle selected relay */
                    window_refresh(now);
                    ButtonDrv_DoSetShort(s_selected);
                    emit(BTN_EV_SET_SHORT);
                }
            }
        }

        vTaskDelay(scan_ticks);
    }
}

/* Public API Implementation */

/** See buttontask.h for detailed documentation. */
BaseType_t ButtonTask_Init(bool enable) {
    s_btn_ready = false;

    /* Skip initialization if disabled */
    if (!enable) {
        return pdPASS;
    }

    /* Initialize button GPIOs */
    ButtonDrv_InitGPIO();

    /* Wait for StorageTask readiness with timeout */
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

    /* Initialize power management subsystem */
    Power_Init();

    /* Create button event queue */
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

    /* Create LED blink timer */
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
        if (xTimerStart(s_blink_timer, pdMS_TO_TICKS(10)) != pdPASS)
            return pdFAIL;
    }

    /* Create main button scanning task */
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

/** See buttontask.h for detailed documentation. */
bool Button_IsReady(void) { return s_btn_ready; }
