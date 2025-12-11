/**
 * @file src/tasks/OCP.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks10 10. Overcurrent Protection Module
 * @ingroup tasks
 * @brief High-priority overcurrent monitoring and protection system.
 * @{
 *
 * @version 1.0.0
 * @date 2025-12-11
 *
 * @details
 * This module provides real-time overcurrent protection for the ENERGIS PDU.
 * It monitors the total current draw across all 8 channels and implements
 * a tiered response system:
 *
 * **Threshold Levels (relative to ENERGIS_CURRENT_LIMIT_A):**
 * - WARNING (Limit - 1.0A): Raises ERR_SEV_ERROR, logs warning
 * - CRITICAL (Limit - 0.25A): Raises ERR_FATAL_ERROR, trips last channel, locks switching
 * - RECOVERY (Limit - 2.0A): Clears lockout when current falls below this level
 *
 * **Regional Limits:**
 * - EU Version: 10.0A (IEC/ENEC)
 * - US Version: 15.0A (UL/CSA)
 *
 * **Architecture:**
 * - MeterTask calls Overcurrent_Update() after each complete 8-channel measurement cycle
 * - SwitchTask queries Overcurrent_IsSwitchingAllowed() before executing relay commands
 * - All control handlers (HTTP, SNMP, Console) receive rejection when locked out
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef OVERCURRENT_H
#define OVERCURRENT_H

#include <stdbool.h>
#include <stdint.h>

/* ==================== Overcurrent State Enumeration ==================== */

/**
 * @brief Overcurrent protection state levels.
 */
typedef enum {
    OC_STATE_NORMAL = 0, /**< Current within safe limits */
    OC_STATE_WARNING,    /**< Current above warning threshold (Limit - 1.0A) */
    OC_STATE_CRITICAL,   /**< Current above critical threshold (Limit - 0.25A) */
    OC_STATE_LOCKOUT     /**< Switching locked due to overcurrent trip */
} overcurrent_state_t;

/* ==================== Overcurrent Status Structure ==================== */

/**
 * @brief Overcurrent protection status snapshot.
 */
typedef struct {
    overcurrent_state_t state;       /**< Current protection state */
    float total_current_a;           /**< Total measured current [A] */
    float limit_a;                   /**< Configured current limit [A] */
    float warning_threshold_a;       /**< Warning threshold [A] */
    float critical_threshold_a;      /**< Critical threshold [A] */
    float recovery_threshold_a;      /**< Recovery threshold [A] */
    uint8_t last_tripped_channel;    /**< Channel turned off on trip (0xFF if none) */
    uint32_t trip_count;             /**< Total trip events since boot */
    uint32_t last_trip_timestamp_ms; /**< Timestamp of last trip [ms since boot] */
    bool switching_allowed;          /**< True if channel switching is permitted */
} overcurrent_status_t;

/* ==================== Public API ==================== */

/**
 * @brief Initialize the overcurrent protection module.
 *
 * @details
 * Sets up thresholds based on regional configuration (EU/US) and initializes
 * the protection state machine. Must be called during system initialization
 * before MeterTask begins monitoring.
 *
 * @note Thread-safe. Can be called from any task context.
 */
void Overcurrent_Init(void);

/**
 * @brief Update overcurrent protection with new total current measurement.
 *
 * @details
 * Called by MeterTask after completing a full 8-channel measurement cycle.
 * Evaluates current against thresholds and triggers appropriate responses:
 * - WARNING: Logs ERR_SEV_ERROR
 * - CRITICAL: Logs ERR_FATAL_ERROR, turns off last activated channel, locks switching
 * - RECOVERY: Clears lockout when current returns to safe levels
 *
 * @param total_current_a Total current across all channels [Amperes]
 * @return Current overcurrent protection state
 *
 * @note Thread-safe. Typically called from MeterTask context.
 */
overcurrent_state_t Overcurrent_Update(float total_current_a);

/**
 * @brief Check if channel switching is currently allowed.
 *
 * @details
 * Queried by SwitchTask and control handlers before executing relay commands.
 * Returns false when in LOCKOUT state due to overcurrent trip.
 *
 * @return true if switching is permitted, false if locked out
 *
 * @note Thread-safe. Can be called from any task context.
 */
bool Overcurrent_IsSwitchingAllowed(void);

/**
 * @brief Check if turning ON a specific channel is allowed.
 *
 * @details
 * More restrictive check that considers current headroom. Used by control
 * handlers to provide early rejection before queueing commands.
 *
 * @param channel Channel index (0-7) to check
 * @return true if channel can be turned ON, false otherwise
 *
 * @note Thread-safe. Can be called from any task context.
 */
bool Overcurrent_CanTurnOn(uint8_t channel);

/**
 * @brief Record that a channel was just turned ON.
 *
 * @details
 * Updates the "last activated channel" tracking used to determine which
 * channel to trip in an overcurrent event.
 *
 * @param channel Channel index (0-7) that was activated
 *
 * @note Thread-safe. Called by SwitchTask after successful relay activation.
 */
void Overcurrent_RecordChannelOn(uint8_t channel);

/**
 * @brief Get current overcurrent protection status.
 *
 * @details
 * Provides a complete snapshot of the protection system state including
 * thresholds, current measurements, and trip history.
 *
 * @param status Pointer to receive status snapshot
 * @return true on success, false if status pointer is NULL
 *
 * @note Thread-safe. Status is an atomic snapshot.
 */
bool Overcurrent_GetStatus(overcurrent_status_t *status);

/**
 * @brief Get current protection state.
 *
 * @return Current overcurrent_state_t value
 *
 * @note Thread-safe. Can be called from any task context.
 */
overcurrent_state_t Overcurrent_GetState(void);

/**
 * @brief Get the configured current limit.
 *
 * @return Maximum allowed total current [Amperes]
 *
 * @note Returns ENERGIS_CURRENT_LIMIT_A (10A EU / 15A US)
 */
float Overcurrent_GetLimit(void);

/**
 * @brief Manually clear lockout state (for recovery/reset).
 *
 * @details
 * Forces the protection state back to NORMAL and re-enables switching.
 * Should only be used for system recovery after addressing the overcurrent
 * condition. Logs the manual reset event.
 *
 * @return true if lockout was cleared, false if not in lockout state
 *
 * @warning Use with caution. Does not verify current has actually decreased.
 */
bool Overcurrent_ClearLockout(void);

#endif /* OVERCURRENT_H */

/** @} */