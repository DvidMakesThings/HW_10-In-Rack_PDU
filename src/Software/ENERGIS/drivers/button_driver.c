/**
 * @file button_driver.c
 * @author DvidMakesThings - David Sipos
 * @defgroup drivers Drivers
 * @brief All hardware drivers for the Energis PDU.
 * @{
 *
 * @defgroup driver1 1. Button Driver
 * @ingroup drivers
 * @brief Handles button inputs and actions for the Energis PDU.
 * @{
 * @version 1.1
 * @date 2025-08-27
 *
 * @details This module manages the hardware button interactions for the ENERGIS PDU.
 * It implements a robust per-button edge-state debouncer to avoid double triggers on short presses.
 * PLUS/MINUS act on a debounced falling edge. SET distinguishes short vs. long press by duration.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "button_driver.h"

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

/**
 * @brief Move selection up (wrap 7→0) and refresh display if present.
 *
 * @details "Up" increases the row number: 0→1→…→7→0.
 * Does not toggle any relays.
 */
static void _select_up(void) {
    selected_row = (selected_row == 7u) ? 0u : (uint8_t)(selected_row + 1u);
    if (HAS_SCREEN) {
        PDU_Display_UpdateSelection(selected_row);
    }
    DEBUG_PRINT("Button PLUS: selected channel: %u\n", (unsigned)selected_row + 1);
}

/**
 * @brief Move selection down (wrap 0→7) and refresh display if present.
 *
 * @details "Down" decreases the row number: 7→6→…→0→7.
 * Does not toggle any relays.
 */
static void _select_down(void) {
    selected_row = (selected_row == 0u) ? 7u : (uint8_t)(selected_row - 1u);
    if (HAS_SCREEN) {
        PDU_Display_UpdateSelection(selected_row);
    }
    DEBUG_PRINT("Button MINUS: selected channel: %u\n", (unsigned)selected_row + 1);
}

/**
 * @brief Execute SET short-press action: toggle the selected relay.
 *
 * @details Reads current relay state, toggles via set_relay_state_with_tag(),
 * logs changes, and warns if dual asymmetry is detected/persisting.
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
}

/**
 * @brief Execute SET long-press action: clear the error LED.
 *
 * @details Resets FAULT_LED using mcp_display_write_pin() and logs the action.
 */
static void _set_long_action(void) {
    setError(false);
    INFO_PRINT("Button SET long press: Cleared error status\r\n");
}

/**
 * @brief Alarm callback to fire the SET long-press action at LONGPRESS_DT.
 *
 * @details
 * - Runs exactly LONGPRESS_DT after a debounced FALL of SET.
 * - If SET is still held and long not yet fired, performs long action
 *   and marks @ref s_set.long_fired = true.
 * - Always disarms itself (one-shot).
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
        _set_long_action();
    }
    return 0; /* one-shot */
}

/* --------------------------------------------------------------------------
 * Interrupt Service Routine
 * -------------------------------------------------------------------------- */

/**
 * @brief Unified GPIO interrupt handler for PLUS, MINUS, and SET buttons.
 *
 * @details
 * - PLUS/MINUS: act on debounced FALL, with post-action guard (POST_GUARD_MS).
 *               RISE updates only the last_edge_ms timestamp.
 * - SET:        FALL latches press start, arms a one-shot alarm for LONGPRESS_DT.
 *               The long action fires at the alarm time if still held.
 *               RISE performs short action only if long hasn't already fired
 *               and duration exceeded debounce.
 *
 * @param gpio   GPIO that triggered the interrupt (BUT_PLUS / BUT_MINUS / BUT_SET).
 * @param events GPIO_IRQ_EDGE_FALL and/or GPIO_IRQ_EDGE_RISE.
 */
void button_isr(uint gpio, uint32_t events) {
    const uint32_t now = _now_ms();

    /* ---------------- PLUS (debounced FALL only) ---------------- */
    if (gpio == BUT_PLUS) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (_since(now, s_plus.last_edge_ms, DEBOUNCE_MS) &&
                _since(now, s_plus.last_action_ms, POST_GUARD_MS)) {
                _select_up();
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

    /* ---------------- MINUS (debounced FALL only) ---------------- */
    if (gpio == BUT_MINUS) {
        if (events & GPIO_IRQ_EDGE_FALL) {
            if (_since(now, s_minus.last_edge_ms, DEBOUNCE_MS) &&
                _since(now, s_minus.last_action_ms, POST_GUARD_MS)) {
                _select_down();
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
        /* FALL: press -> latch, arm alarm */
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

        /* RISE: release -> short action only if long hasn't fired */
        if (events & GPIO_IRQ_EDGE_RISE) {
            if (!_since(now, s_set.last_edge_ms, DEBOUNCE_MS)) {
                return; /* bounce */
            }
            s_set.last_edge_ms = now;

            if (!s_set.is_pressed) {
                return; /* stray rise */
            }

            /* Disarm pending long alarm if any (button released early) */
            if (s_set.long_alarm_id >= 0) {
                cancel_alarm(s_set.long_alarm_id);
                s_set.long_alarm_id = -1;
            }

            s_set.is_pressed = false;
            const uint32_t dt = (uint32_t)(now - s_set.press_start_ms);

            if (!s_set.long_fired) {
                if (dt >= (uint32_t)DEBOUNCE_MS) {
                    _set_short_action();
                }
            }
            /* If long_fired == true, do nothing on release. */
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
 *
 * @details
 * - Registers a single shared ISR (button_isr) for PLUS, MINUS, and SET.
 * - Enables both edges for all buttons (we act on FALL for +/-; SET uses both).
 * - Normalizes selected_row to [0,7] and updates the display if present.
 * - Ensures SET long-press alarm is disarmed at startup.
 */
void button_driver_init(void) {
    s_plus = (btn_edge_t){0};
    s_minus = (btn_edge_t){0};
    s_set = (btn_set_t){.is_pressed = false,
                        .long_fired = false,
                        .press_start_ms = 0,
                        .last_edge_ms = 0,
                        .long_alarm_id = -1};

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
}

/** @} @} */