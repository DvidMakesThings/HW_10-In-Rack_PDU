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

#include "button_driver.h"
#include "drivers/MCP23017_selection_driver.h"

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */
#ifndef SELECT_BLINK_MS
#define SELECT_BLINK_MS 250u
#endif

#ifndef SELECT_WINDOW_MS
#define SELECT_WINDOW_MS 10000u
#endif

#ifndef SELECT_TIMER_TICK_MS
#define SELECT_TIMER_TICK_MS 50 /* internal timer cadence */
#endif

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
 *
 * @param rt Timer handle (unused).
 * @return true to keep the timer running.
 */
static bool _sel_timer_cb(repeating_timer_t *rt) {
    (void)rt;
    const uint32_t now = _now_ms();

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
    if (HAS_SCREEN) {
        PDU_Display_UpdateSelection(selected_row);
    }
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
    if (HAS_SCREEN) {
        PDU_Display_UpdateSelection(selected_row);
    }
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

    INFO_PRINT("Button SET short: row=%u want=%u changed=%u\r\n", (unsigned)selected_row,
               (unsigned)want, (unsigned)changed);

    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X)\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask);
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
 * press, so counted as short.
 * - SET: open window ONLY if the press resolves as a SHORT (released before LONGPRESS_DT).
 *        A LONG press NEVER opens the window.
 */
void button_isr(uint gpio, uint32_t events) {
    const uint32_t now = _now_ms();

    /* ---------------- PLUS (▶, debounced FALL only) ---------------- */
    if (gpio == BUT_PLUS) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (_since(now, s_plus.last_edge_ms, DEBOUNCE_MS) &&
                _since(now, s_plus.last_action_ms, POST_GUARD_MS)) {

                if (!s_sel_active) {
                    /* First interaction when idle → open window, no step */
                    _sel_window_start(now);
                } else {
                    /* Window already active → step RIGHT (increment) and refresh timer */
                    _select_right();
                    _sel_window_start(now);
                }

                s_plus.last_action_ms = now;
                last_press_time = now;
            }
            s_plus.last_edge_ms = now;
        }
        if (events & GPIO_IRQ_EDGE_RISE) {
            s_plus.last_edge_ms = now;
        }
        return;
    }

    /* ---------------- MINUS (◀, debounced FALL only) ---------------- */
    if (gpio == BUT_MINUS) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (_since(now, s_minus.last_edge_ms, DEBOUNCE_MS) &&
                _since(now, s_minus.last_action_ms, POST_GUARD_MS)) {

                if (!s_sel_active) {
                    /* First interaction when idle → open window, no step */
                    _sel_window_start(now);
                } else {
                    /* Window already active → step LEFT (decrement) and refresh timer */
                    _select_left();
                    _sel_window_start(now);
                }

                s_minus.last_action_ms = now;
                last_press_time = now;
            }
            s_minus.last_edge_ms = now;
        }
        if (events & GPIO_IRQ_EDGE_RISE) {
            s_minus.last_edge_ms = now;
        }
        return;
    }

    /* ---------------- SET (press/release + long-timer) ---------------- */
    if (gpio == BUT_SET) {
        /* FALL: press -> latch, arm alarm; DO NOT open window here. */
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (_since(now, s_set.last_edge_ms, DEBOUNCE_MS) && !s_set.is_pressed) {
                s_set.is_pressed = true;
                s_set.long_fired = false;
                s_set.press_start_ms = now;
                s_set.last_edge_ms = now;
                last_press_time = now;

                /* Cancel any stale alarm, then arm a new one for LONGPRESS_DT */
                if (s_set.long_alarm_id >= 0) {
                    cancel_alarm(s_set.long_alarm_id);
                    s_set.long_alarm_id = -1;
                }
                s_set.long_alarm_id =
                    add_alarm_in_ms((int64_t)LONGPRESS_DT, _set_long_alarm_cb, NULL, true);
            }
            return;
        }

        /* RISE: release -> decide SHORT vs LONG outcome */
        if (events & GPIO_IRQ_EDGE_RISE) {
            if (!_since(now, s_set.last_edge_ms, DEBOUNCE_MS)) {
                return; /* bounce */
            }
            s_set.last_edge_ms = now;

            if (!s_set.is_pressed) {
                return; /* stray rise */
            }

            /* Disarm pending long alarm if any (button released before long fires) */
            if (s_set.long_alarm_id >= 0) {
                cancel_alarm(s_set.long_alarm_id);
                s_set.long_alarm_id = -1;
            }

            /* Latch and clear pressed state */
            const bool was_long_fired = s_set.long_fired;
            s_set.is_pressed = false;
            s_set.long_fired = false;

            if (!was_long_fired) {
                /* SHORT press outcome */
                if (!s_sel_active) {
                    /* First SHORT when idle → open window and blink; suppress short action */
                    _sel_window_start(now);
                    s_set_open_only_this_press = true;
                } else {
                    /* Window already open → perform normal short action and refresh timer */
                    _set_short_action();
                    s_sel_last_ms = now;
                    s_set_open_only_this_press = false;
                }
            } else {
                /* LONG press already handled in alarm callback. Never open/blink. */
                s_set_open_only_this_press = false;
            }

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

    selected_row &= 0x07u;
    if (HAS_SCREEN) {
        PDU_Display_UpdateSelection(selected_row);
    }

    /* Start internal timer so blink/timeout works without external polling */
    add_repeating_timer_ms((int32_t)SELECT_TIMER_TICK_MS, _sel_timer_cb, NULL, &s_sel_timer);
}

/** @} @} */
