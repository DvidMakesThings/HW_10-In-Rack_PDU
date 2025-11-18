/**
 * @file MCP23017_dual_driver.c
 * @author
 * @defgroup driver6 6. MCP23017 Dual Driver
 * @ingroup drivers
 * @brief Robust wrapper that keeps the Relay and Display MCP23017 chips
 *        mirrored with shadow-verified writes, readback, retry, and logging.
 * @version 1.2
 * @date 2025-09-12
 *
 * This module forwards operations to the two single-chip drivers
 * (RELAY and DISPLAY MCP23017s) and adds:
 *  - Post-write readback verification on BOTH chips.
 *  - Small retry loop on mismatch or transient I²C burps.
 *  - Optional resync/re-init on persistent mismatch.
 *  - Asymmetry detection + ERROR/WARNING/INFO prints.
 *
 * Public API is unchanged vs. 1.0 to avoid breaking callers.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 * @{
 */

#include "MCP23017_dual_driver.h"
#include "../CONFIG.h"

#include "pico/mutex.h"
#include "pico/stdlib.h"
#include <stdatomic.h>

/* -----------------------------------------------------------------------------
 * Tunables (conservative defaults)
 * ---------------------------------------------------------------------------*/
#ifndef DUAL_VERIFY_RETRIES
#define DUAL_VERIFY_RETRIES 2 /* extra tries after the first attempt */
#endif

#ifndef DUAL_VERIFY_WAIT_US
#define DUAL_VERIFY_WAIT_US 500 /* settle before verifying both chips */
#endif

#ifndef DUAL_REINIT_AFTER_CONSEC
#define DUAL_REINIT_AFTER_CONSEC 3 /* consecutive verify failures to escalate */
#endif

#ifndef DUAL_REINIT_MIN_INTERVAL_MS
#define DUAL_REINIT_MIN_INTERVAL_MS 500u /* rate limit for re-init/resync */
#endif

/* -----------------------------------------------------------------------------
 * Local state
 * ---------------------------------------------------------------------------*/

/* Monotonic write counter for helpful logs */
static atomic_uint_fast32_t g_set_relay_write_count = 0;

/* Track consecutive verify failures (separately for pin and mask ops) */
static uint8_t s_consec_fail_pin = 0;
static uint8_t s_consec_fail_mask = 0;
static uint32_t s_last_reinit_ms = 0;

/* -----------------------------------------------------------------------------
 * Small helpers
 * ---------------------------------------------------------------------------*/

static inline uint32_t _now_ms(void) { return (uint32_t)to_ms_since_boot(get_absolute_time()); }

/* Read both MCPs for a pin; return current logic level of each (0/1). */
static inline void _read_both_pin(uint8_t pin, uint8_t *out_relay, uint8_t *out_disp) {
    if (out_relay)
        *out_relay = mcp_relay_read_pin(pin);
    if (out_disp)
        *out_disp = mcp_display_read_pin(pin);
}

/* Verify both MCPs match the desired value on a single pin. */
static inline bool _verify_pin(uint8_t pin, uint8_t want) {
    busy_wait_us(DUAL_VERIFY_WAIT_US);
    uint8_t r = mcp_relay_read_pin(pin);
    uint8_t d = mcp_display_read_pin(pin);
    return (r == want) && (d == want);
}

/* Verify both MCPs match the desired masked value on a given port. */
static inline bool _verify_mask(uint8_t port_ab, uint8_t mask, uint8_t want_bits) {
    busy_wait_us(DUAL_VERIFY_WAIT_US);

    /* Build the desired final latch state by reading each pin and applying mask */
    for (uint8_t i = 0; i < 8; ++i) {
        const uint8_t pin = (port_ab == 0) ? i : (uint8_t)(8 + i);
        const uint8_t want = (uint8_t)((want_bits >> i) & 0x01u);

        if ((mask >> i) & 0x01u) {
            uint8_t rr = mcp_relay_read_pin(pin);
            uint8_t dd = mcp_display_read_pin(pin);
            if ((rr != want) || (dd != want)) {
                return false;
            }
        }
    }
    return true;
}

/* Escalate if verify keeps failing: rate-limited resync of both chips. */
static inline void _dual_escalate_resync(const char *ctx) {
    const uint32_t now = _now_ms();
    if ((now - s_last_reinit_ms) < DUAL_REINIT_MIN_INTERVAL_MS) {
        return;
    }
    s_last_reinit_ms = now;

    ERROR_PRINT("Dual MCP: persistent mismatch (%s) -> resync both MCPs\r\n", ctx);
    mcp_relay_resync_from_hw();
    mcp_display_resync_from_hw();
}

/* -----------------------------------------------------------------------------
 * Basic register/direction passthroughs (unchanged behavior)
 * ---------------------------------------------------------------------------*/

void mcp_dual_write_reg(uint8_t reg, uint8_t value) {
    mcp_relay_write_reg(reg, value);
    mcp_display_write_reg(reg, value);
}

void mcp_dual_set_direction(uint8_t pin, uint8_t direction) {
    mcp_relay_set_direction(pin, direction);
    mcp_display_set_direction(pin, direction);
}

/* -----------------------------------------------------------------------------
 * Verified/mirrored writes
 * ---------------------------------------------------------------------------*/

void mcp_dual_write_pin(uint8_t pin, uint8_t value) {
    value = (value != 0);

    /* Write both first */
    mcp_relay_write_pin(pin, value);
    mcp_display_write_pin(pin, value);

    /* Verify + small retry loop */
    for (int attempt = 0; attempt <= DUAL_VERIFY_RETRIES; ++attempt) {
        if (_verify_pin(pin, value)) {
            if (attempt > 0) {
                INFO_PRINT("Dual MCP: pin%u recovered after %d retries\r\n", (unsigned)pin,
                           attempt);
            }
            s_consec_fail_pin = 0;
            return;
        }
        /* re-assert both in case one latched wrong */
        mcp_relay_write_pin(pin, value);
        mcp_display_write_pin(pin, value);
    }

    /* Still wrong → warn and maybe escalate */
    uint8_t r = 0, d = 0;
    _read_both_pin(pin, &r, &d);
    WARNING_PRINT("Dual MCP: verify fail on pin%u want=%u relay=%u disp=%u\r\n", (unsigned)pin,
                  (unsigned)value, (unsigned)r, (unsigned)d);

    if (++s_consec_fail_pin >= DUAL_REINIT_AFTER_CONSEC) {
        _dual_escalate_resync("pin");
        s_consec_fail_pin = 0;
        /* one final verify after resync */
        (void)_verify_pin(pin, value);
    }
}

void mcp_dual_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits) {
    /* Apply on both chips first */
    mcp_relay_write_mask(port_ab, mask, value_bits);
    mcp_display_write_mask(port_ab, mask, value_bits);

    /* Verify + small retry loop */
    for (int attempt = 0; attempt <= DUAL_VERIFY_RETRIES; ++attempt) {
        if (_verify_mask(port_ab, mask, value_bits)) {
            if (attempt > 0) {
                INFO_PRINT("Dual MCP: port %c mask write recovered after %d retries\r\n",
                           port_ab ? 'B' : 'A', attempt);
            }
            s_consec_fail_mask = 0;
            return;
        }
        /* re-assert both in case one latched wrong */
        mcp_relay_write_mask(port_ab, mask, value_bits);
        mcp_display_write_mask(port_ab, mask, value_bits);
    }

    WARNING_PRINT("Dual MCP: verify fail on port %c (mask=0x%02X, val=0x%02X)\r\n",
                  port_ab ? 'B' : 'A', (unsigned)mask, (unsigned)value_bits);

    if (++s_consec_fail_mask >= DUAL_REINIT_AFTER_CONSEC) {
        _dual_escalate_resync("mask");
        s_consec_fail_mask = 0;
        /* one final verify after resync */
        (void)_verify_mask(port_ab, mask, value_bits);
    }
}

void mcp_dual_resync_from_hw(void) {
    mcp_relay_resync_from_hw();
    mcp_display_resync_from_hw();
}

/* -----------------------------------------------------------------------------
 * High-level helpers with symmetry reporting / logging
 * ---------------------------------------------------------------------------*/

uint8_t set_relay_state(uint8_t pin, uint8_t value, uint8_t *asym_before_out,
                        uint8_t *asym_after_out) {
    value = (value != 0);

    uint8_t r = mcp_relay_read_pin(pin);
    uint8_t d = mcp_display_read_pin(pin);
    const uint8_t asym_before = (r != d) ? 1u : 0u;
    if (asym_before_out)
        *asym_before_out = asym_before;

    /* Fast no-op if both already correct */
    if ((r == value) && (d == value)) {
        if (asym_after_out)
            *asym_after_out = 0u;
        return 0u;
    }

    /* Do the robust mirrored write (will verify/retry/escalate internally) */
    mcp_dual_write_pin(pin, value);

    /* Report asymmetry after */
    uint8_t r2 = mcp_relay_read_pin(pin);
    uint8_t d2 = mcp_display_read_pin(pin);
    if (asym_after_out)
        *asym_after_out = (r2 != d2) ? 1u : 0u;

    return 1u;
}

uint8_t set_relay_state_with_tag(const char *caller_tag, uint8_t pin, uint8_t value,
                                 uint8_t *asym_before_out, uint8_t *asym_after_out) {
    if (pin >= 16) {
        if (asym_before_out)
            *asym_before_out = 0u;
        if (asym_after_out)
            *asym_after_out = 0u;
        return 0u;
    }

    value = (value != 0);

    uint8_t r = mcp_relay_read_pin(pin);
    uint8_t d = mcp_display_read_pin(pin);
    const uint8_t asym_before = (r != d) ? 1u : 0u;
    if (asym_before_out)
        *asym_before_out = asym_before;

    if ((r == value) && (d == value)) {
        if (asym_after_out)
            *asym_after_out = 0u;
        return 0u;
    }

    /* Log when we actually intend to change state */
    uint32_t n = (uint32_t)atomic_fetch_add(&g_set_relay_write_count, 1) + 1u;
    uint64_t us = time_us_64();
    INFO_PRINT("SET_RELAY[%lu] t=%llu us tag=%s pin=%u val=%u\r\n", (unsigned long)n,
               (unsigned long long)us, caller_tag ? caller_tag : "?", (unsigned)pin,
               (unsigned)value);

    mcp_dual_write_pin(pin, value);

    uint8_t r2 = mcp_relay_read_pin(pin);
    uint8_t d2 = mcp_display_read_pin(pin);
    if (asym_after_out)
        *asym_after_out = (r2 != d2) ? 1u : 0u;

    return 1u;
}

/* -----------------------------------------------------------------------------
 * Guardian / diagnostics (unchanged public API)
 * ---------------------------------------------------------------------------*/

uint16_t mcp_dual_asymmetry_mask(void) {
    uint16_t diff = 0;
    for (uint8_t pin = 0; pin < 16; ++pin) {
        uint8_t r = mcp_relay_read_pin(pin);
        uint8_t d = mcp_display_read_pin(pin);
        if (r != d) {
            diff |= (uint16_t)1u << pin;
        }
    }
    return diff;
}

void mcp_dual_guardian_poll(void) {
    static uint32_t last_ms = 0;
    const uint32_t now_ms = _now_ms();
    if ((now_ms - last_ms) < DUAL_GUARD_INTERVAL_MS)
        return;
    last_ms = now_ms;

    uint16_t mask = mcp_dual_asymmetry_mask() & DUAL_GUARD_MASK;
    if (!mask)
        return;

    WARNING_PRINT("Guardian: asymmetry detected (mask=0x%04X)\r\n", (unsigned)mask);

#if DUAL_GUARD_AUTOHEAL
    uint32_t changed_total = 0;
    for (uint8_t pin = 0; pin < 16; ++pin) {
        if ((mask & (1u << pin)) == 0u)
            continue;
        uint8_t desired = mcp_relay_read_pin(pin); /* relay is canonical */
        changed_total += set_relay_state_with_tag(GUARDIAN_TAG, pin, desired, NULL, NULL);
    }

    uint16_t after = mcp_dual_asymmetry_mask() & DUAL_GUARD_MASK;
    if (after) {
        setError(true);
        ERROR_PRINT("Guardian: asymmetry persists AFTER heal (mask=0x%04X, changed=%lu)\r\n",
                    (unsigned)after, (unsigned long)changed_total);
    } else {
        INFO_PRINT("Guardian: healed (changed=%lu)\r\n", (unsigned long)changed_total);
    }
#endif
}

/** @} */
