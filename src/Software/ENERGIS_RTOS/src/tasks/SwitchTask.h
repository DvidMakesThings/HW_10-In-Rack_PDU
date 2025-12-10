/**
 * @file src/tasks/SwitchTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks09 9. Switch Task
 * @ingroup tasks
 * @brief FreeRTOS-based relay/switch control task for ENERGIS.
 * @{
 *
 * @version 1.0.0
 * @date 2025-12-10
 *
 * @details
 * Provides non-blocking, RTOS-cooperative relay control for all 8 channels.
 * SwitchTask owns all MCP23017 relay operations to prevent I2C bus contention
 * and watchdog starvation during SNMP stress testing.
 *
 * Key Features:
 * - Non-blocking command queueing with configurable timeout
 * - Single task ownership of MCP relay hardware
 * - Atomic cached state for instant reads (no I2C access)
 * - Bulk operations for all channels
 * - Toggle support for button interactions
 * - Periodic hardware sync for state verification
 *
 * Architecture:
 * - SwitchTask is the SOLE owner of mcp_set_channel_state/mcp_get_channel_state
 * - All other tasks use Switch_* API which queues commands
 * - Reads return cached state immediately (no blocking)
 * - Writes are enqueued and processed by SwitchTask
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef ENERGIS_SWITCHTASK_H
#define ENERGIS_SWITCHTASK_H

#include <stdbool.h>
#include <stdint.h>

/* ==================== Configuration Knobs ==================== */

/** @brief Command queue depth (handles bursts of up to 32 commands) */
#ifndef SWITCH_CMD_QUEUE_SIZE
#define SWITCH_CMD_QUEUE_SIZE 32
#endif

/** @brief Delay between relay command executions (ms) to prevent I2C overload */
#ifndef SWITCH_CMD_PACE_MS
#define SWITCH_CMD_PACE_MS 10
#endif

/** @brief Task scan period when idle (ms) */
#ifndef SWITCH_TASK_PERIOD_MS
#define SWITCH_TASK_PERIOD_MS 50
#endif

/** @brief Periodic hardware sync interval (ms) - verify cache matches hardware */
#ifndef SWITCH_HW_SYNC_INTERVAL_MS
#define SWITCH_HW_SYNC_INTERVAL_MS 5000
#endif

/** @brief Bring-up wait timeout for Storage_IsReady() in milliseconds */
#ifndef SWITCH_WAIT_STORAGE_READY_MS
#define SWITCH_WAIT_STORAGE_READY_MS 5000
#endif

/* ==================== Command Types ==================== */

/**
 * @brief Switch control command types
 */
typedef enum {
    SWITCH_CMD_SET_CHANNEL,  /**< Set single channel state */
    SWITCH_CMD_TOGGLE,       /**< Toggle single channel */
    SWITCH_CMD_ALL_ON,       /**< Turn all channels ON */
    SWITCH_CMD_ALL_OFF,      /**< Turn all channels OFF */
    SWITCH_CMD_SET_MASK,     /**< Set multiple channels via bitmask */
    SWITCH_CMD_SYNC_FROM_HW, /**< Force sync cached state from hardware */
} switch_cmd_type_t;

/**
 * @brief Switch control command structure
 */
typedef struct {
    switch_cmd_type_t type; /**< Command type */
    uint8_t channel;        /**< Channel number (0-7) for single-channel ops */
    uint8_t state;          /**< Desired state (1=ON, 0=OFF) */
    uint8_t mask;           /**< Bitmask for SET_MASK command */
} switch_cmd_t;

/* ==================== Public API ==================== */

/**
 * @brief Initialize and start the SwitchTask.
 *
 * Creates the command queue, initializes cached state from hardware,
 * and spawns the SwitchTask. Must be called during system bring-up
 * after MCP23017 initialization.
 *
 * @param enable Set true to create/start; false to skip deterministically.
 * @return pdPASS on success (or when skipped), pdFAIL on error/timeout.
 */
BaseType_t SwitchTask_Init(bool enable);

/**
 * @brief Check if SwitchTask is ready for commands.
 *
 * @return true if initialized and ready, false otherwise.
 */
bool Switch_IsReady(void);

/* ---------- Non-blocking State Reads ---------- */

/**
 * @brief Get current relay channel state (instant, non-blocking).
 *
 * Reads the cached channel state without any I2C access. Safe to call
 * from any task context including time-critical paths.
 *
 * @param channel Channel number (0-7)
 * @param out_state Pointer to receive current state (true=ON, false=OFF)
 * @return true on success, false if channel invalid or out_state is NULL
 *
 * @note Returns cached state which is periodically synced from hardware.
 *       May not reflect very recent queued commands that haven't executed yet.
 */
bool Switch_GetState(uint8_t channel, bool *out_state);

/**
 * @brief Get all 8 channel states as a bitmask (instant, non-blocking).
 *
 * Returns the cached state of all channels in a single byte where
 * bit N represents channel N (bit 0 = channel 0, etc.).
 *
 * @param out_mask Pointer to receive 8-bit state mask (bit=1 means ON)
 * @return true on success, false if out_mask is NULL
 */
bool Switch_GetAllStates(uint8_t *out_mask);

/* ---------- Non-blocking State Writes ---------- */

/**
 * @brief Set single relay channel state (non-blocking).
 *
 * Enqueues a command to set the specified channel to the desired state.
 * Returns immediately without waiting for hardware operation to complete.
 *
 * @param channel Channel number (0-7)
 * @param state Desired state (true=ON, false=OFF)
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full or invalid params
 *
 * @note For SNMP handlers, use timeout_ms=0 to prevent blocking.
 *       For UI operations, timeout_ms=100 provides good responsiveness.
 */
bool Switch_SetChannel(uint8_t channel, bool state, uint32_t timeout_ms);

/**
 * @brief Toggle single relay channel (non-blocking).
 *
 * Enqueues a command to toggle the specified channel (ON->OFF or OFF->ON).
 * Uses cached state to determine new state.
 *
 * @param channel Channel number (0-7)
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full or invalid params
 */
bool Switch_Toggle(uint8_t channel, uint32_t timeout_ms);

/**
 * @brief Turn all relay channels ON (non-blocking).
 *
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_AllOn(uint32_t timeout_ms);

/**
 * @brief Turn all relay channels OFF (non-blocking).
 *
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_AllOff(uint32_t timeout_ms);

/**
 * @brief Set multiple channels via bitmask (non-blocking).
 *
 * Sets each channel to the state indicated by the corresponding bit
 * in the mask. Bit N controls channel N (bit=1 means ON).
 *
 * @param mask 8-bit state mask for all channels
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_SetMask(uint8_t mask, uint32_t timeout_ms);

/**
 * @brief Force synchronization of cached state from hardware.
 *
 * Enqueues a command to re-read all channel states from the MCP hardware
 * and update the cache. Useful after power events or error recovery.
 *
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_SyncFromHardware(uint32_t timeout_ms);

#endif /* ENERGIS_SWITCHTASK_H */

/** @} */