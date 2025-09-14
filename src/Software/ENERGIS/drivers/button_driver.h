/**
 * @file button_driver.h
 * @author
 * @brief Handles button inputs and actions for the Energis PDU.
 * @version 1.3
 * @date 2025-09-06
 *
 * @details
 * Debounced, ISR-driven button handling for three buttons:
 *  - PLUS  : step selection (wrap) on debounced FALL. (Original behavior)
 *  - MINUS : step selection (wrap) on debounced FALL. (Original behavior)
 *  - SET   : short = toggle selected relay; long = clear error (fires at LONGPRESS_DT).
 *
 * Added (non-breaking):
 * - Selection window & blinking on a dedicated selection LED row:
 *   opens on PLUS/MINUS, blinks at 500 ms, resets on interaction, times out at 10 s,
 *   and turns the selection LEDs OFF. Implemented via internal repeating timer.
 */

#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include "../CONFIG.h"
#include "../PDU_display.h"
#include "drivers/MCP23017_dual_driver.h"
#include "drivers/MCP23017_relay_driver.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

/** @brief Log tag used when toggling via the SET button. */
#define BUTTON_TAG "<Button Handler>"

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

/* ----------------- ESD hardening knobs (conservative, behavior-neutral) --- */
#ifndef ESD_READS_PER_EDGE
#define ESD_READS_PER_EDGE 5u /* number of quick samples */
#endif

#ifndef ESD_INTER_SAMPLE_US
#define ESD_INTER_SAMPLE_US 30u /* spacing between samples (~120–150us total) */
#endif

#ifndef ESD_REQUIRED_AGREE
#define ESD_REQUIRED_AGREE 4u /* require >=4 of 5 to agree on level */
#endif

#ifndef ESD_MAX_EDGE_RATE_HZ
#define ESD_MAX_EDGE_RATE_HZ 100u /* cap on valid edges per second (per button) */
#endif

/* ----------------------- Debug print control (deferred) -------------------- */
#ifndef BTN_LOG_ENABLE
#define BTN_LOG_ENABLE 1
#endif
#ifndef BTN_LOG_CAP
#define BTN_LOG_CAP 64u /* ring size; keep small */
#endif

/**
 * @brief Post-action guard to absorb late contact bounces.
 *
 * @details Effective minimum quiet time after a valid action.
 * If not overridden by the build system, defaults to DEBOUNCE_MS+10.
 */
#ifndef POST_GUARD_MS
#define POST_GUARD_MS (DEBOUNCE_MS + 10u)
#endif

/**
 * @brief Edge-timestamp state for a one-shot, fall-only button.
 *
 * @details Used by PLUS and MINUS: they act on debounced FALL edges only.
 */
typedef struct {
    uint32_t last_edge_ms;   /**< Timestamp (ms) of last observed edge. */
    uint32_t last_action_ms; /**< Timestamp (ms) of last action taken.  */
} btn_edge_t;

/**
 * @brief Press/Release + long-press state for the SET button.
 *
 * @details
 * - FALL (press) latches the start time and arms a one-shot alarm for
 *   @c LONGPRESS_DT to trigger the long-press action immediately.
 * - If the alarm fires while still pressed, long action executes and
 *   @ref long_fired is set to prevent the short action on release.
 * - RISE (release) will perform the short action only if the alarm has
 *   not fired and the press duration exceeded debounce.
 */
typedef struct {
    volatile bool is_pressed;         /**< Latched while button is held low.                 */
    volatile bool long_fired;         /**< True once long action executed for this press.    */
    volatile uint32_t press_start_ms; /**< Timestamp (ms) at FALL edge.                      */
    uint32_t last_edge_ms;            /**< Timestamp (ms) of last observed edge.             */
    alarm_id_t long_alarm_id;         /**< Arm for LONGPRESS_DT; -1 if none/past.            */
} btn_set_t;

/**
 * @brief Currently selected row (0–7) mapping to relay channels 1–8.
 *
 * @details Modified by PLUS/MINUS and read by SET. UI refreshed when HAS_SCREEN.
 */
extern volatile uint8_t selected_row;

/**
 * @brief Timestamp (ms) of the last *press* edge observed for any button.
 *
 * @details Updated on debounced FALL of PLUS, MINUS, or SET. Useful for
 * external modules that want to know when the user last interacted.
 */
extern volatile uint32_t last_press_time;

/**
 * @brief Initialize button GPIO interrupts and synchronize UI selection.
 *
 * @details Starts an internal timer so selection blinking/timeout work without polling.
 */
void button_driver_init(void);

/**
 * @brief Unified GPIO interrupt handler for PLUS, MINUS, and SET.
 *
 * @param gpio   The GPIO that triggered the interrupt (e.g., BUT_PLUS).
 * @param events Bitmask: GPIO_IRQ_EDGE_FALL / GPIO_IRQ_EDGE_RISE.
 */
void button_isr(uint gpio, uint32_t events);

#endif /* BUTTON_DRIVER_H */
