/**
 * @file SwitchTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks09  9. Switch Task
 * @brief Minimal, synchronous relay/display control using MCP23017.
 * @{
 * @version 2.0.0
 * @date 2025-12-17
 *
 * @details
 * Simple, human-readable C for reliable relay control:
 * - Single global mutex serializes all MCP23017 access
 * - Direct calls into MCP driver; no queues
 * - Display mirror is best-effort with a short backoff on failures
 *
 * Determinism:
 * - All APIs are synchronous/blocking and return detailed result codes
 * - SNMP paths call Switch_* and get immediate success/failure
 * - No hidden work; every operation completes before returning
 *
 * Thread-safety:
 * - Mutex protects all public operations
 * - Selection writes are gated by manual panel activity
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ENERGIS_SWITCHTASK_H
#define ENERGIS_SWITCHTASK_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/* ==================== Configuration Knobs ==================== */

/**
 * @brief Maximum time to wait for mutex acquisition (ms).
 * @details All synchronous operations will fail if mutex cannot be acquired
 * within this timeout.
 */
#ifndef SWITCH_MUTEX_TIMEOUT_MS
#define SWITCH_MUTEX_TIMEOUT_MS 1000
#endif

/**
 * @brief Task maintenance period (ms).
 * @details SwitchTask runs periodic maintenance (cache sync, health heartbeat).
 */
#ifndef SWITCH_TASK_PERIOD_MS
#define SWITCH_TASK_PERIOD_MS 100
#endif

/**
 * @brief Periodic hardware sync interval (ms).
 * @details How often the background task syncs cache from hardware.
 */
#ifndef SWITCH_HW_SYNC_INTERVAL_MS
#define SWITCH_HW_SYNC_INTERVAL_MS 5000
#endif

/**
 * @brief Bring-up wait timeout for Storage_IsReady() in milliseconds.
 */
#ifndef SWITCH_WAIT_STORAGE_READY_MS
#define SWITCH_WAIT_STORAGE_READY_MS 5000
#endif

/* ==================== Result Codes ==================== */

/**
 * @brief Switch operation result codes.
 * @details Provides detailed status for deterministic error handling.
 */
typedef enum {
    SWITCH_OK = 0,              /**< Operation successful */
    SWITCH_ERR_NOT_INIT,        /**< Subsystem not initialized */
    SWITCH_ERR_INVALID_CHANNEL, /**< Channel out of range (0-7) */
    SWITCH_ERR_MUTEX_TIMEOUT,   /**< Could not acquire mutex */
    SWITCH_ERR_I2C_FAIL,        /**< I2C communication failed */
    SWITCH_ERR_VERIFY_FAIL,     /**< Read-back verification failed */
    SWITCH_ERR_OVERCURRENT,     /**< Operation rejected due to overcurrent lockout */
    SWITCH_ERR_NULL_PARAM       /**< NULL parameter provided */
} switch_result_t;

/* ==================== Public API ==================== */

/**
 * @brief Initialize and start the SwitchTask.
 *
 * Creates the global mutex, initializes hardware state, and spawns the
 * background maintenance task. Must be called during system bring-up
 * after MCP23017 initialization.
 *
 * @param enable Set true to create/start; false to skip deterministically.
 * @return pdPASS on success (or when skipped), pdFAIL on error/timeout.
 */
BaseType_t SwitchTask_Init(bool enable);

/**
 * @brief Check if SwitchTask is ready for operations.
 *
 * @return true if initialized and ready, false otherwise.
 */
bool Switch_IsReady(void);

/* ---------- Synchronous State Reads (DETERMINISTIC) ---------- */

/**
 * @brief Get current relay channel state from hardware.
 *
 * SYNCHRONOUS: Blocks until hardware is read and result returned.
 * Takes mutex, reads hardware via I2C, releases mutex.
 *
 * @param channel Channel number (0-7)
 * @param out_state Pointer to receive current state (true=ON, false=OFF)
 * @return SWITCH_OK on success, error code otherwise.
 *
 * @note This reads DIRECTLY from hardware, not from cache.
 */
switch_result_t Switch_GetState(uint8_t channel, bool *out_state);

/**
 * @brief Get all 8 channel states from hardware.
 *
 * SYNCHRONOUS: Blocks until all hardware states are read.
 * Returns bitmask where bit N represents channel N (bit=1 means ON).
 *
 * @param out_mask Pointer to receive 8-bit state mask
 * @return SWITCH_OK on success, error code otherwise.
 *
 * @note This reads DIRECTLY from hardware, not from cache.
 */
switch_result_t Switch_GetAllStates(uint8_t *out_mask);

/* ---------- Synchronous State Writes (DETERMINISTIC) ---------- */

/**
 * @brief Set single relay channel state with verification.
 *
 * SYNCHRONOUS: Blocks until operation completes and is verified.
 * 1. Acquires mutex
 * 2. Checks overcurrent lockout
 * 3. Writes relay state via I2C
 * 4. Mirrors state to display LED
 * 5. Reads back and verifies
 * 6. Releases mutex
 * 7. Returns result
 *
 * @param channel Channel number (0-7)
 * @param state Desired state (true=ON, false=OFF)
 * @return SWITCH_OK on success, error code otherwise.
 */
switch_result_t Switch_SetChannel(uint8_t channel, bool state);

/**
 * @brief Toggle single relay channel with verification.
 *
 * SYNCHRONOUS: Blocks until operation completes and is verified.
 * Reads current state, inverts it, writes and verifies.
 *
 * @param channel Channel number (0-7)
 * @return SWITCH_OK on success, error code otherwise.
 */
switch_result_t Switch_Toggle(uint8_t channel);

/**
 * @brief Turn all relay channels ON with verification.
 *
 * SYNCHRONOUS: Blocks until all channels are set and verified.
 *
 * @return SWITCH_OK on success, error code otherwise.
 */
switch_result_t Switch_AllOn(void);

/**
 * @brief Turn all relay channels OFF with verification.
 *
 * SYNCHRONOUS: Blocks until all channels are set and verified.
 *
 * @return SWITCH_OK on success, error code otherwise.
 */
switch_result_t Switch_AllOff(void);

/**
 * @brief Set multiple channels via bitmask with verification.
 *
 * SYNCHRONOUS: Blocks until all channels are set and verified.
 * Bit N controls channel N (bit=1 means ON).
 *
 * @param mask 8-bit state mask for all channels
 * @return SWITCH_OK on success, error code otherwise.
 */
switch_result_t Switch_SetMask(uint8_t mask);

/* ---------- Legacy API Wrappers (for backward compatibility) ---------- */

/**
 * @brief Legacy wrapper for Switch_GetState.
 *
 * @param channel Channel number (0-7)
 * @param out_state Pointer to receive state
 * @return true on success, false on failure.
 */
bool Switch_GetStateCompat(uint8_t channel, bool *out_state);

/**
 * @brief Legacy wrapper for Switch_SetChannel.
 * @deprecated Use Switch_SetChannel() which returns detailed result code.
 *
 * @param channel Channel number (0-7)
 * @param state Desired state (true=ON, false=OFF)
 * @param timeout_ms Ignored in v2.0 (kept for API compatibility)
 * @return true on success, false on failure.
 */
bool Switch_SetChannelCompat(uint8_t channel, bool state, uint32_t timeout_ms);

/**
 * @brief Legacy wrapper for Switch_Toggle.
 * @deprecated Use Switch_Toggle() which returns detailed result code.
 */
bool Switch_ToggleCompat(uint8_t channel, uint32_t timeout_ms);

/**
 * @brief Legacy wrapper for Switch_AllOn.
 * @deprecated Use Switch_AllOn() which returns detailed result code.
 */
bool Switch_AllOnCompat(uint32_t timeout_ms);

/**
 * @brief Legacy wrapper for Switch_AllOff.
 * @deprecated Use Switch_AllOff() which returns detailed result code.
 */
bool Switch_AllOffCompat(uint32_t timeout_ms);

/**
 * @brief Legacy wrapper for Switch_SetMask.
 * @deprecated Use Switch_SetMask() which returns detailed result code.
 */
bool Switch_SetMaskCompat(uint8_t mask, uint32_t timeout_ms);

/* ---------- MUX Control (for HLW8032) ---------- */

/**
 * @brief Set relay MCP Port B masked bits (MUX control).
 *
 * Used by HLW8032 driver for MUX channel selection.
 * This function is designed to NEVER fail from the caller's perspective:
 * - If mutex is available: executes immediately
 * - If mutex busy or not initialized: stores pending request for later
 *
 * @param mask Bitmask of Port B bits to modify.
 * @param value New values for masked bits.
 * @param timeout_ms Timeout for mutex acquisition (0 = store pending immediately).
 * @return true always (operation executed or queued).
 *
 * @note HLW8032 calls this frequently; coalesced pending ensures no failures.
 */
bool Switch_SetRelayPortBMasked(uint8_t mask, uint8_t value, uint32_t timeout_ms);

/* ---------- Selection LED Control ---------- */

/**
 * @brief Turn off all selection LEDs.
 *
 * SYNCHRONOUS: Blocks until operation completes.
 *
 * @param timeout_ms Ignored (kept for API compatibility).
 * @return true on success, false on failure.
 */
bool Switch_SelectAllOff(uint32_t timeout_ms);

/**
 * @brief Show (or hide) the selection LED for a given index.
 *
 * SYNCHRONOUS: Blocks until operation completes.
 *
 * @param index Selection index (0-7)
 * @param on True to show selected LED, false to just clear all LEDs.
 * @param timeout_ms Ignored (kept for API compatibility).
 * @return true on success, false on failure.
 */
bool Switch_SelectShow(uint8_t index, bool on, uint32_t timeout_ms);

/* ---------- Display LED Control ---------- */

/**
 * @brief Set FAULT LED on display MCP.
 *
 * SYNCHRONOUS: Blocks until operation completes.
 *
 * @param state true=ON, false=OFF
 * @param timeout_ms Ignored (kept for API compatibility).
 * @return true on success, false on failure.
 */
bool Switch_SetFaultLed(bool state, uint32_t timeout_ms);

/**
 * @brief Set POWER GOOD LED on display MCP.
 *
 * SYNCHRONOUS: Blocks until operation completes.
 *
 * @param state true=ON, false=OFF
 * @param timeout_ms Ignored (kept for API compatibility).
 * @return true on success, false on failure.
 */
bool Switch_SetPwrLed(bool state, uint32_t timeout_ms);

/**
 * @brief Set NETWORK LINK LED on display MCP.
 *
 * SYNCHRONOUS: Blocks until operation completes.
 *
 * @param state true=ON, false=OFF
 * @param timeout_ms Ignored (kept for API compatibility).
 * @return true on success, false on failure.
 */
bool Switch_SetEthLed(bool state, uint32_t timeout_ms);

/* ---------- Utility Functions ---------- */

/**
 * @brief Force synchronization of cache from hardware.
 *
 * SYNCHRONOUS: Reads all relay states and updates internal cache.
 *
 * @param timeout_ms Ignored (kept for API compatibility).
 * @return true on success, false on failure.
 */
bool Switch_SyncFromHardware(uint32_t timeout_ms);

/**
 * @brief Get cached state mask (non-blocking, may be stale).
 *
 * @return 8-bit mask of cached relay states.
 *
 * @note For guaranteed current state, use Switch_GetAllStates().
 */
uint8_t Switch_GetCachedMask(void);

/* ---------- Front-Panel Manual Activity Gate ---------- */

/**
 * @brief Enable/disable front-panel (manual) selection updates.
 *
 * When disabled (default), selection MCP (0x23) is not written by
 * Switch_SelectAllOff/Switch_SelectShow. ButtonTask enables this gate
 * during an active manual interaction window and disables it when the
 * window closes, ensuring 0x23 is only addressed on user button actions.
 *
 * @param active true to allow selection writes (manual active), false to block.
 */
void Switch_SetManualPanelActive(bool active);

#endif /* ENERGIS_SWITCHTASK_H */

/** @} */
/** @} */