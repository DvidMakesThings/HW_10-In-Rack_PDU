/**
 * @file src/tasks/OCP.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-12-14
 *
 * @details
 * Implementation of the high-priority overcurrent protection system.
 * Monitors total current draw and implements tiered protection responses.
 *
 * Version History:
 * - v1.x: Compile-time regional current limits via macros
 * - v2.0: Runtime regional current limits via DeviceIdentity module
 *
 * **State Machine:**
 * ```
 * NORMAL -> WARNING (current > limit - 1.0A)
 * WARNING -> CRITICAL (current > limit - 0.25A)
 * CRITICAL -> LOCKOUT (trip executed)
 * LOCKOUT -> NORMAL (current < limit - 2.0A)
 * ```
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define OVERCURRENT_TAG "[OC_PROT]"

/* ==================== Module State ==================== */

/**
 * @brief Protection thresholds.
 *
 * @note Initialized to very high defaults to prevent false trips if
 *       Overcurrent_Init() hasn't been called yet. Real values are
 *       set during initialization from DeviceIdentity.
 */
static float s_limit_a = 1000.0f;
static float s_warning_threshold_a = 999.0f;
static float s_critical_threshold_a = 999.75f;
static float s_recovery_threshold_a = 998.0f;

/** @brief Current protection state */
static volatile overcurrent_state_t s_state = OC_STATE_NORMAL;

/** @brief Latest total current measurement */
static volatile float s_total_current_a = 0.0f;

/** @brief Switching allowed flag */
static volatile bool s_switching_allowed = true;

/** @brief Last channel that was turned ON (for trip targeting) */
static volatile uint8_t s_last_activated_channel = 0xFF;

/** @brief Trip event counter */
static volatile uint32_t s_trip_count = 0;

/** @brief Timestamp of last trip */
static volatile uint32_t s_last_trip_timestamp_ms = 0;

/** @brief Warning already logged flag (prevent log spam) */
static volatile bool s_warning_logged = false;

/** @brief Module initialized flag */
static volatile bool s_initialized = false;

/* ==================== Internal Helpers ==================== */

/**
 * @brief Sanitize a current limit value obtained from configuration.
 *
 * @details
 * This module must never operate with an invalid or near-zero limit value, because the
 * WARNING state triggers when total_current_a >= (limit - offset). If limit is 0 or very
 * small, the derived thresholds become negative and will produce false WARNING logs even
 * at 0.00A (all outputs OFF).
 *
 * The identity/config layer is expected to provide 10A (EU) or 15A (US). If it does not,
 * we fall back to a safe regional default.
 *
 * @param limit_a  Raw limit value [A] from configuration.
 * @param region   Region selector from DeviceIdentity.
 *
 * @return float
 * A validated limit in the expected operational range.
 */
static float sanitize_limit_a(float limit_a, device_region_t region) {
    const float expected = (region == DEVICE_REGION_US) ? 15.0f : 10.0f;

    /* Hard bounds: reject zero/negative and unrealistic values */
    if (!(limit_a > 1.0f && limit_a < 50.0f)) {
        return expected;
    }

    /* Region consistency: if region is known, keep it within a reasonable band */
    if (region == DEVICE_REGION_EU) {
        if (limit_a < 8.0f || limit_a > 12.0f) {
            return 10.0f;
        }
    } else if (region == DEVICE_REGION_US) {
        if (limit_a < 13.0f || limit_a > 17.0f) {
            return 15.0f;
        }
    } else {
        /* Unknown region: still prefer a conservative safe default */
        return 10.0f;
    }

    return limit_a;
}

/**
 * @brief Clamp and order derived thresholds.
 *
 * @details
 * Ensures the derived thresholds remain sensible even if configuration values or
 * offsets are changed. The ordering enforced is:
 * recovery < warning < critical < limit
 *
 * This prevents false positives and prevents the state machine from behaving
 * unexpectedly due to threshold inversion.
 */
static void sanitize_thresholds(void) {
    /* Clamp recovery to non-negative */
    if (s_recovery_threshold_a < 0.0f) {
        s_recovery_threshold_a = 0.0f;
    }

    /* Clamp warning to non-negative */
    if (s_warning_threshold_a < 0.0f) {
        s_warning_threshold_a = 0.0f;
    }

    /* Ensure critical is above warning by a small margin */
    if (s_critical_threshold_a <= s_warning_threshold_a) {
        s_critical_threshold_a = s_warning_threshold_a + 0.05f;
    }

    /* Ensure limit is above critical */
    if (s_limit_a <= s_critical_threshold_a) {
        s_limit_a = s_critical_threshold_a + 0.05f;
    }

    /* Ensure recovery provides hysteresis below warning */
    if (s_recovery_threshold_a >= s_warning_threshold_a) {
        if (s_warning_threshold_a > 0.10f) {
            s_recovery_threshold_a = s_warning_threshold_a - 0.10f;
        } else {
            s_recovery_threshold_a = 0.0f;
        }
    }
}

/**
 * @brief Execute overcurrent trip action.
 *
 * @details
 * Attempts to disable the last channel that was successfully enabled and recorded
 * via Overcurrent_RecordChannelOn(). If no valid channel is tracked, all channels
 * are disabled as a safety fallback.
 *
 * Behavior:
 * - If s_last_activated_channel is in range [0..7], that channel is turned OFF.
 * - Otherwise, all channels are turned OFF.
 *
 * Notes:
 * - The actual relay switching is delegated to SwitchTask via non-blocking API calls.
 * - Logging is performed to indicate the applied protective action.
 *
 * @return uint8_t
 * Returns the logical channel index [0..7] that was turned OFF, or 0xFF if the
 * fallback "all OFF" action was used.
 */
static uint8_t execute_trip(void) {
    /* Snapshot current target channel */
    uint8_t tripped_ch = s_last_activated_channel;

    if (tripped_ch < 8) {
        /* Use SwitchTask interface to turn off the targeted channel */
        (void)Switch_SetChannel(tripped_ch, false);

        INFO_PRINT("%s TRIP: Turning OFF channel %u due to overcurrent\r\n", OVERCURRENT_TAG,
                   (unsigned)(tripped_ch + 1));
    } else {
        /* No tracked channel available, perform a safety fallback */
        (void)Switch_AllOff();

        INFO_PRINT("%s TRIP: Turning OFF ALL channels (no last-on tracking)\r\n", OVERCURRENT_TAG);
        tripped_ch = 0xFF;
    }

    return tripped_ch;
}

/* ==================== Public API Implementation ==================== */

/**
 * @brief Initialize the overcurrent protection module.
 *
 * @details
 * Computes state machine thresholds from runtime configuration obtained from
 * the DeviceIdentity module. The region setting in EEPROM determines the
 * appropriate current limit (10A EU / 15A US).
 *
 * Threshold derivation:
 * - s_limit_a is sourced from DeviceIdentity_GetCurrentLimitA()
 * - s_warning_threshold_a = s_limit_a - ENERGIS_CURRENT_WARNING_OFFSET_A
 * - s_critical_threshold_a = s_limit_a - ENERGIS_CURRENT_SAFETY_MARGIN_A
 * - s_recovery_threshold_a = s_limit_a - ENERGIS_CURRENT_RECOVERY_OFFSET_A
 *
 * State initialization:
 * - Sets state to NORMAL
 * - Clears current measurement cache and trip bookkeeping
 * - Enables switching
 * - Marks the module initialized after all values are set
 *
 * @note
 * DeviceIdentity_Init() must be called before this function to ensure the
 * region setting is available. If DeviceIdentity returns UNKNOWN region,
 * the safe default (10A) is used.
 */
void Overcurrent_Init(void) {
    /* Get current limit from device identity (region-dependent) */
    device_region_t region = DeviceIdentity_GetRegion();
    s_limit_a = sanitize_limit_a(DeviceIdentity_GetCurrentLimitA(), region);

    /* Compute thresholds from fixed offsets */
    s_warning_threshold_a = s_limit_a - ENERGIS_CURRENT_WARNING_OFFSET_A;
    s_critical_threshold_a = s_limit_a - ENERGIS_CURRENT_SAFETY_MARGIN_A;
    s_recovery_threshold_a = s_limit_a - ENERGIS_CURRENT_RECOVERY_OFFSET_A;

    /* Sanity: keep thresholds ordered and non-negative */
    sanitize_thresholds();

    /* Initialize state */
    s_state = OC_STATE_NORMAL;
    s_total_current_a = 0.0f;
    s_switching_allowed = true;
    s_last_activated_channel = 0xFF;
    s_trip_count = 0;
    s_last_trip_timestamp_ms = 0;
    s_warning_logged = false;

    /* Mark as initialized LAST to ensure all values are set */
    s_initialized = true;

    /* Log initialization with region info */
    const char *region_str = "UNKNOWN";
    if (region == DEVICE_REGION_EU) {
        region_str = "EU";
    } else if (region == DEVICE_REGION_US) {
        region_str = "US";
    }

    INFO_PRINT("%s Initialized (%s: %.1fA limit, warn=%.2fA, crit=%.2fA, recv=%.2fA)\r\n",
               OVERCURRENT_TAG, region_str, s_limit_a, s_warning_threshold_a,
               s_critical_threshold_a, s_recovery_threshold_a);
}

/**
 * @brief Update the overcurrent state machine using a new total current sample.
 *
 * @details
 * This function is the evaluation core of the overcurrent protection module.
 * It is intended to be called periodically (typically once per full measurement
 * cycle) with the aggregated total current in amperes.
 *
 * Processing steps:
 * 1. Reject processing if the module is not initialized.
 * 2. Apply a sanity filter to reject invalid measurements.
 * 3. Store the latest accepted measurement for status reporting.
 * 4. Evaluate the state machine based on thresholds and hysteresis.
 * 5. Execute transition side effects:
 *    - WARNING: log once to avoid repeated messages
 *    - LOCKOUT: trip action, lock switching, update trip bookkeeping, log error
 *    - RECOVERY: unlock switching and clear warning latch
 *
 * State behavior:
 * - NORMAL:
 *   - Enters WARNING when total_current_a >= warning threshold
 *   - Enters CRITICAL when total_current_a >= critical threshold
 * - WARNING:
 *   - Enters CRITICAL when total_current_a >= critical threshold
 *   - Returns to NORMAL when total_current_a < warning threshold
 * - CRITICAL:
 *   - Enters LOCKOUT only if total_current_a is still >= critical threshold
 *   - Returns to NORMAL if the measurement is below the critical threshold
 * - LOCKOUT:
 *   - Returns to NORMAL (recovery) when total_current_a < recovery threshold
 *
 * @param total_current_a
 * The total measured current across all channels in amperes.
 *
 * @return overcurrent_state_t
 * The current state after processing the new sample.
 *
 * @note
 * The sanity range check uses an upper bound of 200A to avoid acting on corrupt
 * or uninitialized data. This value is selected to be well above any expected
 * operating regime while still rejecting extreme invalid values.
 */
overcurrent_state_t Overcurrent_Update(float total_current_a) {
    /* CRITICAL: Do not process if not initialized - thresholds would be wrong */
    if (!s_initialized) {
        return OC_STATE_NORMAL;
    }

    /* Sanity check: reject obviously invalid measurements */
    if (total_current_a < 0.0f || total_current_a > 200.0f) {
        return s_state;
    }

    /* Store latest measurement */
    s_total_current_a = total_current_a;

    /* Snapshot previous state for transition processing */
    overcurrent_state_t prev_state = s_state;
    overcurrent_state_t new_state = prev_state;

    /* State machine evaluation */
    switch (prev_state) {
    case OC_STATE_NORMAL:
        /* Detect transitions into WARNING or CRITICAL based on thresholds */
        if (total_current_a >= s_critical_threshold_a) {
            /* Jump directly to CRITICAL if current is very high */
            new_state = OC_STATE_CRITICAL;
        } else if (total_current_a >= s_warning_threshold_a) {
            new_state = OC_STATE_WARNING;
        }
        break;

    case OC_STATE_WARNING:
        /* Escalate to CRITICAL when the critical threshold is reached */
        if (total_current_a >= s_critical_threshold_a) {
            new_state = OC_STATE_CRITICAL;
        } else if (total_current_a < s_warning_threshold_a) {
            /* Hysteresis: return to normal only when clearly below warning */
            new_state = OC_STATE_NORMAL;
            s_warning_logged = false;
        }
        break;

    case OC_STATE_CRITICAL:
        /* CRITICAL triggers immediate trip only if still above threshold */
        if (total_current_a >= s_critical_threshold_a) {
            new_state = OC_STATE_LOCKOUT;
        } else {
            /* Transient excursion cleared prior to executing trip action */
            new_state = OC_STATE_NORMAL;
        }
        break;

    case OC_STATE_LOCKOUT:
        /* Recovery hysteresis: remain locked until current is clearly below recovery */
        if (total_current_a < s_recovery_threshold_a) {
            /* Recovery: current has dropped sufficiently */
            new_state = OC_STATE_NORMAL;
        }
        break;

    default:
        new_state = OC_STATE_NORMAL;
        break;
    }

    /* Update state and execute side effects */
    s_state = new_state;

    if (new_state != prev_state) {
        switch (new_state) {
        case OC_STATE_WARNING:
            /* Log the warning once per warning episode */
            if (!s_warning_logged) {
#if ERRORLOGGER
                uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_OCP, ERR_SEV_ERROR, ERR_FID_OVPTASK, 0xA);
                ERROR_PRINT_CODE(err_code,
                                 "%s WARNING: Total current %.2fA approaching limit %.1fA\r\n",
                                 OVERCURRENT_TAG, total_current_a, s_limit_a);
                Storage_EnqueueErrorCode(err_code);
#endif
                s_warning_logged = true;
            }
            break;

        case OC_STATE_LOCKOUT: {
            /* Execute trip and enter lockout */
            s_switching_allowed = false;

            /* Apply protective switching action */
            uint8_t tripped = execute_trip();

            /* Update trip bookkeeping */
            s_trip_count++;
            s_last_trip_timestamp_ms = to_ms_since_boot(get_absolute_time());

#if ERRORLOGGER
            uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_OCP, ERR_FATAL_ERROR, ERR_FID_OVPTASK, 0xB);
            ERROR_PRINT_CODE(err_code,
                             "%s CRITICAL: Overcurrent trip! %.2fA >= %.2fA. "
                             "CH%u OFF, switching LOCKED\r\n",
                             OVERCURRENT_TAG, total_current_a, s_critical_threshold_a,
                             (unsigned)(tripped < 8 ? tripped + 1 : 0));
            Storage_EnqueueErrorCode(err_code);
#endif
            (void)tripped; /* Silence unused warning if ERRORLOGGER=0 */
        } break;

        case OC_STATE_NORMAL:
            /* Recovery side effects only apply when leaving LOCKOUT */
            if (prev_state == OC_STATE_LOCKOUT) {
                /* Recovery from lockout */
                s_switching_allowed = true;
                s_warning_logged = false;

                INFO_PRINT("%s RECOVERY: Current %.2fA < %.2fA, switching UNLOCKED\r\n",
                           OVERCURRENT_TAG, total_current_a, s_recovery_threshold_a);
            }
            break;

        default:
            /* No additional side effects required for other transitions */
            break;
        }
    }

    return s_state;
}

/**
 * @brief Query whether switching actions are currently allowed.
 *
 * @details
 * This function provides the fast-path gate for relay switching requests.
 * While the module is uninitialized, switching is allowed to avoid blocking
 * early boot operations. After initialization, switching is allowed only when
 * the module is not in lockout.
 *
 * @return bool
 * true if switching is allowed, false otherwise.
 *
 * @note
 * The returned state reflects the internal lockout flag rather than the raw
 * overcurrent state enum to make the decision explicit.
 */
bool Overcurrent_IsSwitchingAllowed(void) {
    if (!s_initialized) {
        return true; /* Allow before init completes */
    }
    return s_switching_allowed;
}

/**
 * @brief Determine whether a specific channel is permitted to be turned ON.
 *
 * @details
 * This function performs a more conservative gate than Overcurrent_IsSwitchingAllowed().
 * It can be used by control handlers to reject commands early when current headroom is
 * limited or when the system is in a protective state.
 *
 * Current decision logic:
 * - If module is not initialized, allow.
 * - If switching is not allowed due to lockout, reject.
 * - If current state is WARNING or above, reject to preserve headroom.
 * - Otherwise allow.
 *
 * @param channel
 * Channel index [0..7]. The current implementation does not apply per-channel
 * policy, but keeps the parameter for future enhancement.
 *
 * @return bool
 * true if the channel may be turned ON, false otherwise.
 */
bool Overcurrent_CanTurnOn(uint8_t channel) {
    (void)channel; /* Currently not per-channel, but signature allows future enhancement */

    if (!s_initialized) {
        return true;
    }

    /* Reject if in lockout or if we're already at warning level */
    if (!s_switching_allowed) {
        return false;
    }

    /* Additional headroom check: don't allow turning on if already at warning */
    if (s_state >= OC_STATE_WARNING) {
        return false;
    }

    return true;
}

/**
 * @brief Record the last channel that was turned ON.
 *
 * @details
 * Stores the most recently activated channel index, used for targeted trip action.
 * The module trips the last known activated channel under a lockout event, which is
 * intended to localize the fault to the most recently added load.
 *
 * @param channel
 * Channel index [0..7] that has been turned ON successfully.
 */
void Overcurrent_RecordChannelOn(uint8_t channel) {
    /* Only accept valid channel indices */
    if (channel < 8) {
        s_last_activated_channel = channel;
    }
}

/**
 * @brief Obtain a snapshot of the current protection status.
 *
 * @details
 * Fills the provided status structure with thresholds, state, current measurement,
 * trip bookkeeping, and switching permission flag.
 *
 * The snapshot is intended for diagnostics and control interfaces. Values are copied
 * from volatile module state. The snapshot is best-effort; it is expected to be read
 * atomically enough for reporting purposes in this system architecture.
 *
 * @param status
 * Pointer to the destination status structure.
 *
 * @return bool
 * true on success, false if status is NULL.
 */
bool Overcurrent_GetStatus(overcurrent_status_t *status) {
    if (status == NULL) {
        return false;
    }

    /* Atomic snapshot */
    status->state = s_state;
    status->total_current_a = s_total_current_a;
    status->limit_a = s_limit_a;
    status->warning_threshold_a = s_warning_threshold_a;
    status->critical_threshold_a = s_critical_threshold_a;
    status->recovery_threshold_a = s_recovery_threshold_a;
    status->last_tripped_channel = s_last_activated_channel;
    status->trip_count = s_trip_count;
    status->last_trip_timestamp_ms = s_last_trip_timestamp_ms;
    status->switching_allowed = s_switching_allowed;

    return true;
}

/**
 * @brief Get the current overcurrent protection state.
 *
 * @details
 * Returns the internal state enum representing the current protection level.
 *
 * @return overcurrent_state_t
 * Current protection state.
 */
overcurrent_state_t Overcurrent_GetState(void) { return s_state; }

/**
 * @brief Get the configured current limit.
 *
 * @details
 * Returns the configured hard current limit, derived from device identity
 * region setting at initialization.
 *
 * @return float
 * Current limit in amperes.
 */
float Overcurrent_GetLimit(void) { return s_limit_a; }

/**
 * @brief Clear lockout state manually.
 *
 * @details
 * If the module is currently in LOCKOUT, this function resets the state to NORMAL,
 * re-enables switching, clears the warning latch, and logs the manual reset.
 *
 * Safety note:
 * - This does not verify that current has fallen below recovery threshold.
 * - The caller is responsible for verifying that conditions are safe before use.
 *
 * @return bool
 * true if lockout was cleared, false if the module was not in lockout.
 */
bool Overcurrent_ClearLockout(void) {
    if (s_state != OC_STATE_LOCKOUT) {
        return false;
    }

    /* Reset protective state and re-enable switching */
    s_state = OC_STATE_NORMAL;
    s_switching_allowed = true;
    s_warning_logged = false;

    INFO_PRINT("%s MANUAL RESET: Lockout cleared by operator\r\n", OVERCURRENT_TAG);

    return true;
}
