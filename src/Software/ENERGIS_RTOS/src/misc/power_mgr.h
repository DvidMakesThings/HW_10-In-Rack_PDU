/**
 * @file src/misc/power_mgr.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup misc3 3. Power Manager Module
 * @ingroup misc
 * @brief Centralized power state management and standby mode control
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-17
 *
 * @details
 * This module provides centralized power state management for the ENERGIS PDU,
 * implementing a software-based standby mode that reduces power consumption and
 * network activity while maintaining system responsiveness for wake events.
 *
 * The power manager maintains a global state that all tasks can query to adapt
 * their behavior between normal operation (RUN) and low-power (STANDBY) modes.
 * State transitions are atomic and coordinated across hardware peripherals.
 *
 * Standby Mode Characteristics:
 * - W5500 Ethernet controller held in hardware reset (PHY disabled, no traffic)
 * - All relay outputs disabled
 * - Display LEDs off except PWR_LED (breathing pattern)
 * - Selection LEDs disabled
 * - Tasks continue executing but skip non-critical operations
 * - Watchdog remains serviced to prevent unexpected resets
 * - System remains responsive to button press wake events
 *
 * Wake from standby is typically triggered by a short press of the power button,
 * which calls Power_ExitStandby() to restore normal operation.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef POWER_MGR_H
#define POWER_MGR_H

#include "CONFIG.h"

/**
 * @brief System power states
 * @enum power_state_t
 * @ingroup misc3
 * @name Power States
 * @{
 */
typedef enum {
    PWR_STATE_RUN = 0,    /**< Normal operation mode */
    PWR_STATE_STANDBY = 1 /**< Low-power standby mode */
} power_state_t;
/** @} */

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

/**
 * @name Public API
 * @ingroup misc3
 * @{
 */
/**
 * @brief Initialize the power manager subsystem.
 *
 * @details Must be called once during system initialization before any other
 * power manager functions are used. Sets initial state to PWR_STATE_RUN.
 *
 * @return None
 */
void Power_Init(void);

/**
 * @brief Query the current system power state.
 *
 * @details Thread-safe query that can be called from any task context.
 * Tasks use this to determine whether to perform normal operations or
 * enter a minimal parked mode.
 *
 * @return Current power state (PWR_STATE_RUN or PWR_STATE_STANDBY)
 */
power_state_t Power_GetState(void);

/**
 * @brief Enter standby mode.
 *
 * @details Transitions the system from RUN to STANDBY state. Performs:
 * - Turn off all relay outputs (all channels OFF)
 * - Set all MCP23017 display outputs to 0 except PWR_LED
 * - Hold W5500 RESET low (chip held in reset, network down)
 * - All LEDs off except PWR_LED (which pulses)
 * - Set internal power state to PWR_STATE_STANDBY
 *
 * Tasks are NOT suspended; they must detect the new state via Power_GetState()
 * and switch to minimal operation (heartbeat + delay only).
 *
 * @note This function is typically called by ButtonTask on long press of BUT_PWR
 *
 * @return None
 */
void Power_EnterStandby(void);

/**
 * @brief Exit standby mode and return to normal operation.
 *
 * @details Transitions the system from STANDBY to RUN state. Performs:
 * - Release W5500 RESET (drive high)
 * - Restore normal LED state machine
 * - Set internal power state to PWR_STATE_RUN
 * - Relays remain OFF; user must turn them on explicitly
 *
 * After calling this function, NetTask will detect the state change and
 * reinitialize the W5500 using its existing link-detection and reinit logic.
 *
 * @note This function is typically called by ButtonTask on short press of BUT_PWR
 * while in STANDBY
 *
 * @return None
 */
void Power_ExitStandby(void);

/**
 * @brief Service the PWR_LED pulse animation in standby mode.
 *
 * @details Should be called periodically (e.g., every 10-50ms) when in STANDBY
 * to update the PWR_LED fade in/out pattern. Has no effect in RUN mode.
 * Implements a simple software PWM by toggling the LED at varying rates to
 * create a breathing effect.
 *
 * @note Call this from a task that runs frequently (e.g., ButtonTask scan loop)
 *
 * @return None
 */
void Power_ServiceStandbyLED(void);
/** @} */

#endif /* POWER_MGR_H */

/** @} */