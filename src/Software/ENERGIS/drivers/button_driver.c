/**
 * @file button_driver.c
 * @defgroup drivers Drivers
 * @brief All hardware drivers for the Energis PDU.
 * @{
 *
 * @defgroup driver1 1. Button Driver
 * @ingroup drivers
 * @brief Handles button inputs and actions for the Energis PDU.
 * @{
 * @version 1.7.2
 * @date 2025-09-06
 *
 * @details This module manages the hardware button interactions for the ENERGIS PDU.
 * It implements a robust per-button edge-state debouncer to avoid double triggers on short presses.
 * PLUS/MINUS act on a debounced falling edge. SET distinguishes short vs. long press by duration.
 *
 * Selection-window additions (blinking row):
 * - First interaction when idle:
 *      • PLUS/MINUS: open a 10 s selection window and start 250 ms blink (no step on that first
 * press). • SET:        only open the window and start blinking if the press is SHORT (i.e.,
 * released before LONGPRESS_DT). A LONG press does NOT open/blink.
 * - While window is open:
 *      • PLUS  steps RIGHT  (increment, wrap 0→7) and refreshes timer.
 *      • MINUS steps LEFT   (decrement, wrap 7→0) and refreshes timer.
 *      • SET short toggles selected relay and refreshes timer.
 * - Window auto-closes after 10 s of inactivity; blinking LEDs turn OFF.
 */

#include "../CONFIG.h"

/* --------------------------------------------------------------------------
 * Private state
 * -------------------------------------------------------------------------- */

/** @brief PLUS timing state (fall-only action). */
static btn_edge_t s_plus = {0};
/** @brief MINUS timing state (fall-only action). */
static btn_edge_t s_minus = {0};
/** @brief SET press/release + long-press timing state. */
static btn_set_t s_set = {.long_alarm_id = -1};

/**
 * @brief Currently selected row (0–7) mapping to relay channels 1–8.
 *
 * @details Defined here, declared in header.
 */
volatile uint8_t selected_row = 0;

/**
 * @brief Timestamp (ms) of the last *press* (FALL) observed for any button.
 *
 * @details Defined here, declared in header. Updated on debounced FALL events.
 */
volatile uint32_t last_press_time = 0;

/* ---------- Selection window state ---------- */
static volatile bool s_sel_active = false;    /* true while window is open */
static volatile uint32_t s_sel_last_ms = 0;   /* last interaction (PLUS/MINUS/SET short) */
static volatile uint32_t s_blink_last_ms = 0; /* last blink toggle time */
static volatile bool s_blink_on = false;      /* current blink state */
static repeating_timer_t s_sel_timer;         /* internal timer for blink/timeout */

/* For SET: remember if this short press should be suppressed (first-open) */
static bool s_set_open_only_this_press = false;

/* --------------------- ESD/EMI guard counters (per button) ---------------- */
typedef struct {
    uint32_t win_start_ms;
    uint32_t edge_count;
} esd_counter_t;

static esd_counter_t s_esd_plus = {0};
static esd_counter_t s_esd_minus = {0};
static esd_counter_t s_esd_set = {0};

/* -------------------------- Deferred debug logging ------------------------- */
#if BTN_LOG_ENABLE
typedef enum {
    BLK_PLUS_FALL_OK,
    BLK_PLUS_FALL_DBNC,
    BLK_PLUS_FALL_ESD,
    BLK_PLUS_RISE_OK,
    BLK_PLUS_RISE_ESD,

    BLK_MINUS_FALL_OK,
    BLK_MINUS_FALL_DBNC,
    BLK_MINUS_FALL_ESD,
    BLK_MINUS_RISE_OK,
    BLK_MINUS_RISE_ESD,

    BLK_SET_FALL_OK,
    BLK_SET_FALL_DBNC,
    BLK_SET_FALL_ESD,
    BLK_SET_RISE_OK,
    BLK_SET_RISE_DBNC,
    BLK_SET_RISE_ESD,
    BLK_SET_RISE_STRAY,

    BLK_RATE_PLUS_DROP,
    BLK_RATE_MINUS_DROP,
    BLK_RATE_SET_DROP
} btn_log_kind_t;

typedef struct {
    uint32_t t_ms;
    btn_log_kind_t kind;
} btn_log_t;

static volatile uint16_t s_log_w = 0, s_log_r = 0;
static btn_log_t s_log_ring[BTN_LOG_CAP];

static inline void _btnlog_push(btn_log_kind_t k, uint32_t now_ms) {
    uint16_t w = s_log_w;
    uint16_t nxt = (uint16_t)((w + 1u) % BTN_LOG_CAP);
    if (nxt == s_log_r) {
        /* drop oldest */
        s_log_r = (uint16_t)((s_log_r + 1u) % BTN_LOG_CAP);
    }
    s_log_ring[w].t_ms = now_ms;
    s_log_ring[w].kind = k;
    s_log_w = nxt;
}

static void _btnlog_drain(void) {
    while (s_log_r != s_log_w) {
        btn_log_t e = s_log_ring[s_log_r];
        s_log_r = (uint16_t)((s_log_r + 1u) % BTN_LOG_CAP);
        switch (e.kind) {
        case BLK_PLUS_FALL_OK:
            printf("[IRQ] PLUS FALL ok @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_PLUS_FALL_DBNC:
            printf("[IRQ] PLUS FALL ignored (debounce/guard) @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_PLUS_FALL_ESD:
            printf("[ESD] PLUS FALL rejected @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_PLUS_RISE_OK:
            printf("[IRQ] PLUS RISE ok @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_PLUS_RISE_ESD:
            printf("[ESD] PLUS RISE rejected @%lu\n", (unsigned long)e.t_ms);
            break;

        case BLK_MINUS_FALL_OK:
            printf("[IRQ] MINUS FALL ok @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_MINUS_FALL_DBNC:
            printf("[IRQ] MINUS FALL ignored (debounce/guard) @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_MINUS_FALL_ESD:
            printf("[ESD] MINUS FALL rejected @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_MINUS_RISE_OK:
            printf("[IRQ] MINUS RISE ok @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_MINUS_RISE_ESD:
            printf("[ESD] MINUS RISE rejected @%lu\n", (unsigned long)e.t_ms);
            break;

        case BLK_SET_FALL_OK:
            printf("[IRQ] SET FALL ok (press latched) @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_SET_FALL_DBNC:
            printf("[IRQ] SET FALL ignored (debounce/already pressed) @%lu\n",
                   (unsigned long)e.t_ms);
            break;
        case BLK_SET_FALL_ESD:
            printf("[ESD] SET FALL rejected @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_SET_RISE_OK:
            printf("[IRQ] SET RISE ok (short or long resolved) @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_SET_RISE_DBNC:
            printf("[IRQ] SET RISE ignored (debounce) @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_SET_RISE_ESD:
            printf("[ESD] SET RISE rejected @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_SET_RISE_STRAY:
            printf("[IRQ] SET RISE stray (no press) @%lu\n", (unsigned long)e.t_ms);
            break;

        case BLK_RATE_PLUS_DROP:
            printf("[ESD] PLUS drop: rate limit @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_RATE_MINUS_DROP:
            printf("[ESD] MINUS drop: rate limit @%lu\n", (unsigned long)e.t_ms);
            break;
        case BLK_RATE_SET_DROP:
            printf("[ESD] SET drop: rate limit @%lu\n", (unsigned long)e.t_ms);
            break;
        default:
            break;
        }
    }
}
#else
static inline void _btnlog_push(int k, uint32_t now_ms) {
    (void)k;
    (void)now_ms;
}
static inline void _btnlog_drain(void) {}
#endif

/* --------------------------------------------------------------------------
 * Private helpers
 * -------------------------------------------------------------------------- */

/**
 * @brief Get current time (ms) since boot.
 * @return Milliseconds since boot as uint32_t.
 */
static inline uint32_t _now_ms(void) { return (uint32_t)to_ms_since_boot(get_absolute_time()); }

/**
 * @brief Check if @p now - @p then >= @p win (ms).
 *
 * @param now  Current timestamp in ms.
 * @param then Earlier timestamp in ms.
 * @param win  Required elapsed window in ms.
 * @return true if elapsed >= win, else false.
 */
static inline bool _since(uint32_t now, uint32_t then, uint32_t win) {
    return (uint32_t)(now - then) >= win;
}

/* ------------------- ESD guards: stable-level vote + rate limit ----------- */

/**
 * @brief Confirm the GPIO level by taking N quick samples and requiring
 *        >= ESD_REQUIRED_AGREE to match @p expect_level.
 *
 * @return true if confirmed, false if likely EMI glitch.
 */
static inline bool _esd_confirm_level(uint gpio, uint8_t expect_level) {
    uint8_t agree = 0;
    for (uint i = 0; i < ESD_READS_PER_EDGE; i++) {
        uint8_t v = gpio_get(gpio) ? 1u : 0u;
        if (v == expect_level)
            agree++;
        if (i + 1u < ESD_READS_PER_EDGE)
            busy_wait_us(ESD_INTER_SAMPLE_US);
    }
    return (agree >= ESD_REQUIRED_AGREE);
}

/**
 * @brief Per-button edge-rate limiter (very loose, kills only EMI storms).
 *        Returns true if allowed, false if to be ignored.
 */
static inline bool _esd_edge_allowed(esd_counter_t *ctr, uint32_t now_ms) {
    if ((now_ms - ctr->win_start_ms) >= 1000u) {
        ctr->win_start_ms = now_ms;
        ctr->edge_count = 0;
    }
    ctr->edge_count++;
    return (ctr->edge_count <= ESD_MAX_EDGE_RATE_HZ);
}

/* ---------------- Selection row helpers (do not touch status LEDs) --------- */

/**
 * @brief Turn OFF all selection LEDs (synchronously).
 */
static inline void _sel_all_off(void) {
    mcp_selection_write_mask(0, 0xFFu, 0x00u);
    mcp_selection_write_mask(1, 0xFFu, 0x00u);
}

/**
 * @brief Show current blink state on the currently selected channel only.
 *        Ensures all other selection LEDs are OFF.
 *
 * @param on 1 to light the current selected channel LED, 0 to turn it off.
 */
static inline void _sel_show_current_only(uint8_t on) {
    _sel_all_off(); /* keep selection row exclusive */
    if (on) {
        mcp_selection_write_pin((uint8_t)selected_row, 1u);
    }
}

/**
 * @brief Activate (or re-activate) the selection window.
 *        Starts blinking immediately (LED ON) if it was idle.
 *
 * @param now_ms Current ms timestamp.
 */
static inline void _sel_window_start(uint32_t now_ms) {
    if (!s_sel_active) {
        s_sel_active = true;
        s_blink_on = true; /* start visibly ON */
        s_blink_last_ms = now_ms;
        _sel_show_current_only(1u); /* light the currently selected channel */
    }
    s_sel_last_ms = now_ms;
}

/**
 * @brief Deactivate selection window and clear all selection LEDs.
 */
static inline void _sel_window_stop(void) {
    if (s_sel_active) {
        _sel_all_off();
        s_sel_active = false;
    }
}

/**
 * @brief Internal timer callback: handles 250 ms blink and 10 s timeout.
 *        Also drains deferred debug logs (low priority).
 *
 * @param rt Timer handle (unused).
 * @return true to keep the timer running.
 */
static bool _sel_timer_cb(repeating_timer_t *rt) {
    (void)rt;
    const uint32_t now = _now_ms();

    /* Low-priority: drain debug logs here (NOT in ISR) */
    _btnlog_drain();

    if (!s_sel_active) {
        return true;
    }

    /* Blink at SELECT_BLINK_MS */
    if (_since(now, s_blink_last_ms, SELECT_BLINK_MS)) {
        s_blink_last_ms = now;
        s_blink_on = !s_blink_on;
        _sel_show_current_only(s_blink_on ? 1u : 0u);
    }

    /* Timeout after SELECT_WINDOW_MS */
    if (_since(now, s_sel_last_ms, SELECT_WINDOW_MS)) {
        _sel_window_stop(); /* ensures LEDs are OFF */
    }

    return true;
}

/* ---------------------- Selection helpers ------------------------- */

/**
 * @brief Move selection LEFT (decrement, wrap 7→0) and refresh display if present.
 * Does not toggle any relays.
 */
static void _select_left(void) {
    selected_row = (selected_row == 0u) ? 7u : (uint8_t)(selected_row - 1u);
    if (s_sel_active) {
        _sel_show_current_only(s_blink_on ? 1u : 0u);
    }
    DEBUG_PRINT("Selection moved LEFT: channel %u\n", (unsigned)selected_row + 1);
}

/**
 * @brief Move selection RIGHT (increment, wrap 0→7) and refresh display if present.
 * Does not toggle any relays.
 */
static void _select_right(void) {
    selected_row = (selected_row == 7u) ? 0u : (uint8_t)(selected_row + 1u);
    if (s_sel_active) {
        _sel_show_current_only(s_blink_on ? 1u : 0u);
    }
    DEBUG_PRINT("Selection moved RIGHT: channel %u\n", (unsigned)selected_row + 1);
}

/**
 * @brief Execute SET short-press action: toggle the selected relay.
 */
static void _set_short_action(void) {
    uint8_t current = mcp_relay_read_pin((uint8_t)selected_row);
    uint8_t want = current ? 0u : 1u;
    uint8_t ab = 0, aa = 0;
    uint8_t changed = set_relay_state_with_tag(BUTTON_TAG, (uint8_t)selected_row, want, &ab, &aa);

    printf("Button SET short: row=%u want=%u changed=%u\r\n", (unsigned)selected_row,
           (unsigned)want, (unsigned)changed);

    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X) by %s\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask, BUTTON_TAG);
    }

    if (s_sel_active) {
        s_sel_last_ms = _now_ms(); /* keep window alive */
    }
}

/**
 * @brief Execute SET long-press action: clear the error LED.
 */
static void _set_long_action(void) {
    setError(false);
    INFO_PRINT("Button SET long press: Cleared error status\r\n");
}

/**
 * @brief Alarm callback to fire the SET long-press action at LONGPRESS_DT.
 *
 * @param id        Alarm id.
 * @param user_data Unused.
 * @return 0 to prevent rescheduling.
 */
static int64_t _set_long_alarm_cb(alarm_id_t id, void *user_data) {
    (void)user_data;
    if (id != s_set.long_alarm_id) {
        return 0; /* stale alarm */
    }
    s_set.long_alarm_id = -1;

    if (s_set.is_pressed && !s_set.long_fired) {
        s_set.long_fired = true;
        /* LONG press: perform action, do NOT open selection window or blink. */
        _set_long_action();
    }
    return 0; /* one-shot */
}

/* --------------------------------------------------------------------------
 * Interrupt Service Routine
 * -------------------------------------------------------------------------- */

/**
 * @brief Unified GPIO interrupt handler for PLUS (▶), MINUS (◀), and SET (●).
 *
 * Mapping inside selection window:
 *   PLUS  → RIGHT  (increment, wrap 0→7)
 *   MINUS → LEFT   (decrement, wrap 7→0)
 *
 * Rules for opening selection window & blinking:
 * - PLUS/MINUS: on first valid FALL when idle → open window only (no step). They have no long
 *   press, so counted as short.
 * - SET: open window ONLY if the press resolves as a SHORT (released before LONGPRESS_DT).
 *        A LONG press NEVER opens the window.
 *
 * ESD hardening (non-invasive):
 * - 5-sample stable-level confirmation per edge (>=4/5 agree). FALL expects level=0, RISE
 * expects 1.
 * - Very loose per-button edge-rate limiter; discards only EMI storms.
 *   If allowed, behavior is identical to the original.
 *
 * NOTE: No printf here. ISR only enqueues tiny debug markers; printing happens in the 50 ms timer.
 */
void button_isr(uint gpio, uint32_t events) {
    const uint32_t now = _now_ms();

    /* Per-button very-loose rate limiting (kills only absurd edge storms) */
    if (gpio == BUT_PLUS) {
        if (!_esd_edge_allowed(&s_esd_plus, now)) {
            _btnlog_push(BLK_RATE_PLUS_DROP, now);
            return;
        }
    } else if (gpio == BUT_MINUS) {
        if (!_esd_edge_allowed(&s_esd_minus, now)) {
            _btnlog_push(BLK_RATE_MINUS_DROP, now);
            return;
        }
    } else if (gpio == BUT_SET) {
        if (!_esd_edge_allowed(&s_esd_set, now)) {
            _btnlog_push(BLK_RATE_SET_DROP, now);
            return;
        }
    }

    /* ---------------- PLUS (▶, debounced FALL only) ---------------- */
    if (gpio == BUT_PLUS) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (!_esd_confirm_level(BUT_PLUS, 0u)) {
                _btnlog_push(BLK_PLUS_FALL_ESD, now);
                return;
            }

            if (_since(now, s_plus.last_edge_ms, DEBOUNCE_MS) &&
                _since(now, s_plus.last_action_ms, POST_GUARD_MS)) {

                if (!s_sel_active) {
                    _sel_window_start(now);
                } else {
                    _select_right();
                    _sel_window_start(now);
                }
                s_plus.last_action_ms = now;
                last_press_time = now;
                _btnlog_push(BLK_PLUS_FALL_OK, now);
            } else {
                _btnlog_push(BLK_PLUS_FALL_DBNC, now);
            }
            s_plus.last_edge_ms = now;
        }
        if (events & GPIO_IRQ_EDGE_RISE) {
            if (_esd_confirm_level(BUT_PLUS, 1u)) {
                s_plus.last_edge_ms = now;
                _btnlog_push(BLK_PLUS_RISE_OK, now);
            } else {
                _btnlog_push(BLK_PLUS_RISE_ESD, now);
            }
        }
        return;
    }

    /* ---------------- MINUS (◀, debounced FALL only) ---------------- */
    if (gpio == BUT_MINUS) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (!_esd_confirm_level(BUT_MINUS, 0u)) {
                _btnlog_push(BLK_MINUS_FALL_ESD, now);
                return;
            }

            if (_since(now, s_minus.last_edge_ms, DEBOUNCE_MS) &&
                _since(now, s_minus.last_action_ms, POST_GUARD_MS)) {

                if (!s_sel_active) {
                    _sel_window_start(now);
                } else {
                    _select_left();
                    _sel_window_start(now);
                }
                s_minus.last_action_ms = now;
                last_press_time = now;
                _btnlog_push(BLK_MINUS_FALL_OK, now);
            } else {
                _btnlog_push(BLK_MINUS_FALL_DBNC, now);
            }
            s_minus.last_edge_ms = now;
        }
        if (events & GPIO_IRQ_EDGE_RISE) {
            if (_esd_confirm_level(BUT_MINUS, 1u)) {
                s_minus.last_edge_ms = now;
                _btnlog_push(BLK_MINUS_RISE_OK, now);
            } else {
                _btnlog_push(BLK_MINUS_RISE_ESD, now);
            }
        }
        return;
    }

    /* ---------------- SET (press/release + long-timer) ---------------- */
    if (gpio == BUT_SET) {
        /* FALL: press -> latch, arm alarm; DO NOT open window here. */
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (!_esd_confirm_level(BUT_SET, 0u)) {
                _btnlog_push(BLK_SET_FALL_ESD, now);
                return;
            }

            if (_since(now, s_set.last_edge_ms, DEBOUNCE_MS) && !s_set.is_pressed) {
                s_set.is_pressed = true;
                s_set.long_fired = false;
                s_set.press_start_ms = now;
                s_set.last_edge_ms = now;
                last_press_time = now;

                if (s_set.long_alarm_id >= 0) {
                    cancel_alarm(s_set.long_alarm_id);
                    s_set.long_alarm_id = -1;
                }
                s_set.long_alarm_id =
                    add_alarm_in_ms((int64_t)LONGPRESS_DT, _set_long_alarm_cb, NULL, true);

                _btnlog_push(BLK_SET_FALL_OK, now);
            } else {
                _btnlog_push(BLK_SET_FALL_DBNC, now);
            }
            return;
        }

        /* RISE: release -> decide SHORT vs LONG outcome */
        if (events & GPIO_IRQ_EDGE_RISE) {
            if (!_esd_confirm_level(BUT_SET, 1u)) {
                _btnlog_push(BLK_SET_RISE_ESD, now);
                return;
            }

            if (!_since(now, s_set.last_edge_ms, DEBOUNCE_MS)) {
                _btnlog_push(BLK_SET_RISE_DBNC, now);
                return; /* bounce */
            }
            s_set.last_edge_ms = now;

            if (!s_set.is_pressed) {
                _btnlog_push(BLK_SET_RISE_STRAY, now);
                return; /* stray rise */
            }

            if (s_set.long_alarm_id >= 0) {
                cancel_alarm(s_set.long_alarm_id);
                s_set.long_alarm_id = -1;
            }

            const bool was_long_fired = s_set.long_fired;
            s_set.is_pressed = false;
            s_set.long_fired = false;

            if (!was_long_fired) {
                /* SHORT press outcome */
                if (!s_sel_active) {
                    _sel_window_start(now);
                    s_set_open_only_this_press = true;
                } else {
                    _set_short_action();
                    s_sel_last_ms = now;
                    s_set_open_only_this_press = false;
                }
            } else {
                s_set_open_only_this_press = false;
            }

            _btnlog_push(BLK_SET_RISE_OK, now);
            return;
        }
    }

    /* Others ignored */
}

/* --------------------------------------------------------------------------
 * Initialization
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialize button GPIO interrupts and synchronize UI selection.
 */
void button_driver_init(void) {
    s_plus = (btn_edge_t){0};
    s_minus = (btn_edge_t){0};
    s_set = (btn_set_t){.is_pressed = false,
                        .long_fired = false,
                        .press_start_ms = 0,
                        .last_edge_ms = 0,
                        .long_alarm_id = -1};

    s_set_open_only_this_press = false;

    /* Reset ESD counters */
    s_esd_plus = (esd_counter_t){0};
    s_esd_minus = (esd_counter_t){0};
    s_esd_set = (esd_counter_t){0};

#if BTN_LOG_ENABLE
    s_log_w = s_log_r = 0;
#endif

    /* Selection window initial state (LEDs off) */
    s_sel_active = false;
    s_sel_last_ms = 0;
    s_blink_last_ms = 0;
    s_blink_on = false;
    _sel_all_off();

    gpio_set_irq_enabled_with_callback(BUT_PLUS, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true,
                                       &button_isr);
    gpio_set_irq_enabled_with_callback(BUT_MINUS, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true,
                                       &button_isr);
    gpio_set_irq_enabled_with_callback(BUT_SET, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true,
                                       &button_isr);

    /* Start internal timer so blink/timeout works without external polling */
    add_repeating_timer_ms((int32_t)SELECT_TIMER_TICK_MS, _sel_timer_cb, NULL, &s_sel_timer);
}

/** @} @} */
