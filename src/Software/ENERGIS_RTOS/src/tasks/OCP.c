/**
 * @file src/tasks/OCP.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.1
 * @date 2025-12-11
 *
 * @details
 * Implementation of the high-priority overcurrent protection system.
 * Monitors total current draw and implements tiered protection responses.
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
 *       set during initialization.
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
 * @brief Execute overcurrent trip: turn off last activated channel.
 *
 * @return Channel that was turned off, or 0xFF if no channel was active
 */
static uint8_t execute_trip(void) {
    uint8_t tripped_ch = s_last_activated_channel;

    if (tripped_ch < 8) {
        /* Turn off the last activated channel via SwitchTask (non-blocking) */
        (void)Switch_SetChannel(tripped_ch, false, 0);

        INFO_PRINT("%s TRIP: Turning OFF channel %u due to overcurrent\r\n", OVERCURRENT_TAG,
                   (unsigned)(tripped_ch + 1));
    } else {
        /* No tracked channel - turn off all as safety fallback */
        (void)Switch_AllOff(0);

        INFO_PRINT("%s TRIP: Turning OFF ALL channels (no last-on tracking)\r\n", OVERCURRENT_TAG);
        tripped_ch = 0xFF;
    }

    return tripped_ch;
}

/* ==================== Public API Implementation ==================== */

void Overcurrent_Init(void) {
    /* Compute thresholds from regional configuration */
    s_limit_a = ENERGIS_CURRENT_LIMIT_A;
    s_warning_threshold_a = s_limit_a - ENERGIS_CURRENT_WARNING_OFFSET_A;
    s_critical_threshold_a = s_limit_a - ENERGIS_CURRENT_SAFETY_MARGIN_A;
    s_recovery_threshold_a = s_limit_a - ENERGIS_CURRENT_RECOVERY_OFFSET_A;

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

#if ENERGIS_EU_VERSION
    INFO_PRINT("%s Initialized (EU: %.1fA limit, warn=%.2fA, crit=%.2fA, recv=%.2fA)\r\n",
               OVERCURRENT_TAG, s_limit_a, s_warning_threshold_a, s_critical_threshold_a,
               s_recovery_threshold_a);
#else
    INFO_PRINT("%s Initialized (US: %.1fA limit, warn=%.2fA, crit=%.2fA, recv=%.2fA)\r\n",
               OVERCURRENT_TAG, s_limit_a, s_warning_threshold_a, s_critical_threshold_a,
               s_recovery_threshold_a);
#endif
}

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

    overcurrent_state_t prev_state = s_state;
    overcurrent_state_t new_state = prev_state;

    /* State machine evaluation */
    switch (prev_state) {
    case OC_STATE_NORMAL:
        if (total_current_a >= s_critical_threshold_a) {
            /* Jump directly to CRITICAL if current is very high */
            new_state = OC_STATE_CRITICAL;
        } else if (total_current_a >= s_warning_threshold_a) {
            new_state = OC_STATE_WARNING;
        }
        break;

    case OC_STATE_WARNING:
        if (total_current_a >= s_critical_threshold_a) {
            new_state = OC_STATE_CRITICAL;
        } else if (total_current_a < s_warning_threshold_a) {
            /* Hysteresis: return to normal only when clearly below warning */
            new_state = OC_STATE_NORMAL;
            s_warning_logged = false;
        }
        break;

    case OC_STATE_CRITICAL:
        /* CRITICAL triggers immediate trip and lockout */
        new_state = OC_STATE_LOCKOUT;
        break;

    case OC_STATE_LOCKOUT:
        if (total_current_a < s_recovery_threshold_a) {
            /* Recovery: current has dropped sufficiently */
            new_state = OC_STATE_NORMAL;
        }
        break;
    }

    /* Process state transitions */
    if (new_state != prev_state) {
        s_state = new_state;

        switch (new_state) {
        case OC_STATE_WARNING:
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
            uint8_t tripped = execute_trip();
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
            if (prev_state == OC_STATE_LOCKOUT) {
                /* Recovery from lockout */
                s_switching_allowed = true;
                s_warning_logged = false;

                INFO_PRINT("%s RECOVERY: Current %.2fA < %.2fA, switching UNLOCKED\r\n",
                           OVERCURRENT_TAG, total_current_a, s_recovery_threshold_a);
            }
            break;

        default:
            break;
        }
    }

    return s_state;
}

bool Overcurrent_IsSwitchingAllowed(void) {
    if (!s_initialized) {
        return true; /* Allow before init completes */
    }
    return s_switching_allowed;
}

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

void Overcurrent_RecordChannelOn(uint8_t channel) {
    if (channel < 8) {
        s_last_activated_channel = channel;
    }
}

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

overcurrent_state_t Overcurrent_GetState(void) { return s_state; }

float Overcurrent_GetLimit(void) { return s_limit_a; }

bool Overcurrent_ClearLockout(void) {
    if (s_state != OC_STATE_LOCKOUT) {
        return false;
    }

    s_state = OC_STATE_NORMAL;
    s_switching_allowed = true;
    s_warning_logged = false;

    INFO_PRINT("%s MANUAL RESET: Lockout cleared by operator\r\n", OVERCURRENT_TAG);

    return true;
}