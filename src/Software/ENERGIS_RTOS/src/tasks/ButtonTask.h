/**
 * @file src/tasks/button_task.h
 * @author DvidMakesThings - David Sipos
 * 
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
 * PLUS  -> move selection RIGHT (wrap)  [only after window is open]
 * MINUS -> move selection LEFT  (wrap)  [only after window is open]
 * SET short -> toggle selected relay (opens window if it was idle)
 * SET long  -> clear error LED; never opens the window
 * 5. Non-blocking and ISR-free; suitable for RP2040 + FreeRTOS.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ENERGIS_BUTTONTASK_H
#define ENERGIS_BUTTONTASK_H

#include "../CONFIG.h"

/* ---------- Configuration knobs (override via -D or before include) ---------- */
/** @brief Button scanning period in milliseconds */
#ifndef BTN_SCAN_PERIOD_MS
#define BTN_SCAN_PERIOD_MS 10u
#endif

/** @brief Selection LED blink period in milliseconds */
#ifndef SELECT_BLINK_MS
#define SELECT_BLINK_MS 250u
#endif

/** @brief Selection window timeout in milliseconds */
#ifndef SELECT_WINDOW_MS
#define SELECT_WINDOW_MS 10000u
#endif

/** @brief Bring-up wait timeout for Storage_IsReady() in milliseconds */
#ifndef BUTTON_WAIT_STORAGE_READY_MS
#define BUTTON_WAIT_STORAGE_READY_MS 5000u
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button events emitted to q_btn.
 */
typedef enum {
    BTN_EV_PLUS_FALL = 1u,  /**< PLUS pressed (debounced high->low)  */
    BTN_EV_PLUS_RISE = 2u,  /**< PLUS released (low->high)           */
    BTN_EV_MINUS_FALL = 3u, /**< MINUS pressed                        */
    BTN_EV_MINUS_RISE = 4u, /**< MINUS released                       */
    BTN_EV_SET_SHORT = 5u,  /**< SET short press resolved on release  */
    BTN_EV_SET_LONG = 6u    /**< SET long press resolved while held   */
} btn_event_kind_t;

/**
 * @brief Payload for button events.
 */
typedef struct {
    btn_event_kind_t kind; /**< Event kind. */
    uint32_t t_ms;         /**< Event timestamp (ms). */
    uint8_t sel;           /**< Selected row index (0..7). */
} btn_event_t;

/**
 * @brief Global queue where ButtonTask publishes events.
 */
extern QueueHandle_t q_btn;

/**
 * @brief Create and start the ButtonTask with an enable gate (bring-up step 4/6).
 *
 * @param enable Set true to create/start; false to skip deterministically.
 * @return pdPASS on success (or when skipped), pdFAIL on error/timeout.
 */
BaseType_t ButtonTask_Init(bool enable);

/**
 * @brief Ready-state query for deterministic boot sequencing.
 * 
 * @return true if ButtonTask_Init(true) completed successfully, else false.
 */
bool Button_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* ENERGIS_BUTTONTASK_H */

/** @} */