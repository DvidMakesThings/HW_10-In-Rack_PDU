/**
 * @file MCP23017_dual_driver.c
 * @author
 * @defgroup driver6 6. MCP23017 Dual Driver
 * @ingroup drivers
 * @brief Thin wrapper driver that controls both Relay and Display MCP23017 chips together.
 * @version 1.0
 * @date 2025-08-25
 *
 * This driver simply forwards calls to both the Relay and Display
 * MCP23017 robust drivers. It ensures both chips are always
 * updated consistently, using the already shadow-latched and
 * mutex-protected single drivers.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 * @{
 */

#include "MCP23017_dual_driver.h"
#include "../CONFIG.h"
#include "hardware/i2c.h"
#include "pico/mutex.h"
#include "pico/stdlib.h"
#include <stdatomic.h>

/* Monotonic write counter */
static atomic_uint_fast32_t g_set_relay_write_count = 0;

/**
 * @brief Write a value to a register on both MCP23017 chips.
 * @param reg Register address
 * @param value Value to write
 */
void mcp_dual_write_reg(uint8_t reg, uint8_t value) {
    mcp_relay_write_reg(reg, value);
    mcp_display_write_reg(reg, value);
}

/**
 * @brief Configure pin direction on both MCP23017 chips.
 * @param pin Pin number (0-15)
 * @param direction 0 = output, 1 = input
 */
void mcp_dual_set_direction(uint8_t pin, uint8_t direction) {
    mcp_relay_set_direction(pin, direction);
    mcp_display_set_direction(pin, direction);
}

/**
 * @brief Write a digital value to a pin on both MCP23017 chips.
 * @param pin Pin number (0-15)
 * @param value 0 = low, 1 = high
 */
void mcp_dual_write_pin(uint8_t pin, uint8_t value) {
    mcp_relay_write_pin(pin, value);
    mcp_display_write_pin(pin, value);
}

/**
 * @brief Atomically write a masked group of bits on both MCP23017 chips.
 * @param port_ab 0 = Port A, 1 = Port B
 * @param mask Bit mask to apply
 * @param value_bits Bits to set
 */
void mcp_dual_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits) {
    mcp_relay_write_mask(port_ab, mask, value_bits);
    mcp_display_write_mask(port_ab, mask, value_bits);
}

/**
 * @brief Resynchronize both MCP23017 shadow latches from hardware OLAT registers.
 */
void mcp_dual_resync_from_hw(void) {
    mcp_relay_resync_from_hw();
    mcp_display_resync_from_hw();
}

/**
 * @brief Write both MCPs for one pin only if needed, and report asymmetry.
 * @param pin              Pin number (0–15).
 * @param value            Desired state: 0 = low, 1 = high.
 * @param asym_before_out  Optional pointer; set to 1 if relay!=display BEFORE, else 0.
 * @param asym_after_out   Optional pointer; set to 1 if relay!=display AFTER, else 0.
 * @return uint8_t         1 if a write was performed; 0 if no change was needed.
 */
uint8_t set_relay_state(uint8_t pin, uint8_t value, uint8_t *asym_before_out,
                        uint8_t *asym_after_out) {
    value = (value != 0);

    /* Read current pin state on both chips */
    uint8_t r = mcp_relay_read_pin(pin);
    uint8_t d = mcp_display_read_pin(pin);
    uint8_t asym_before = (r != d) ? 1u : 0u;

    if (asym_before_out)
        *asym_before_out = asym_before;

    /* If BOTH already equal the desired value, no-op */
    if ((r == value) && (d == value)) {
        if (asym_after_out)
            *asym_after_out = (uint8_t)0;
        return 0u;
    }

    /* Write desired state to BOTH to keep them mirrored */
    mcp_dual_write_pin(pin, value);

    /* Read-back to confirm and to report asymmetry-after */
    uint8_t r2 = mcp_relay_read_pin(pin);
    uint8_t d2 = mcp_display_read_pin(pin);
    if (asym_after_out)
        *asym_after_out = (r2 != d2) ? 1u : 0u;

    return 1u;
}

/**
 * @brief Write both MCPs for one pin only if needed, and report asymmetry, with logging.
 * @param caller_tag       Text label of the caller (e.g. "uart", "fsm", __func__).
 * @param pin              Pin number (0–15).
 * @param value            Desired state: 0 = low, 1 = high.
 * @param asym_before_out  Optional pointer; set to 1 if relay!=display BEFORE, else 0.
 * @param asym_after_out   Optional pointer; set to 1 if relay!=display AFTER, else 0.
 * @return uint8_t         1 if a write was performed; 0 if no change was needed.
 */
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

    /* Read current pin state on both chips */
    uint8_t r = mcp_relay_read_pin(pin);
    uint8_t d = mcp_display_read_pin(pin);
    uint8_t asym_before = (r != d) ? 1u : 0u;
    if (asym_before_out)
        *asym_before_out = asym_before;

    /* If BOTH already equal the desired value, no-op */
    if ((r == value) && (d == value)) {
        if (asym_after_out)
            *asym_after_out = 0u;
        return 0u;
    }

    /* --- LOG: only when we actually write --- */
    uint32_t n = (uint32_t)atomic_fetch_add(&g_set_relay_write_count, 1) + 1u;
    uint64_t us = time_us_64();
    INFO_PRINT("SET_RELAY[%lu] t=%llu us tag=%s pin=%u val=%u\r\n", (unsigned long)n,
               (unsigned long long)us, caller_tag ? caller_tag : "?", (unsigned)pin,
               (unsigned)value);

    /* Write desired state to BOTH to keep them mirrored */
    mcp_dual_write_pin(pin, value);

    /* Read-back to confirm and to report asymmetry-after */
    uint8_t r2 = mcp_relay_read_pin(pin);
    uint8_t d2 = mcp_display_read_pin(pin);
    if (asym_after_out)
        *asym_after_out = (r2 != d2) ? 1u : 0u;

    return 1u;
}

/**
 * @brief Compute full-chip asymmetry mask (1 bits where relay!=display).
 * @return uint16_t 16-bit mask: bit n = 1 iff relay[n] != display[n].
 */
uint16_t mcp_dual_asymmetry_mask(void) {
    uint16_t diff = 0;
    /* Build mask from individual pin reads to avoid private register deps */
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
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if ((now_ms - last_ms) < DUAL_GUARD_INTERVAL_MS)
        return;
    last_ms = now_ms;

    uint16_t mask = mcp_dual_asymmetry_mask() & DUAL_GUARD_MASK;
    if (!mask)
        return; /* nothing to do */

    WARNING_PRINT("Guardian: asymmetry detected (mask=0x%04X)\r\n", (unsigned)mask);

#if DUAL_GUARD_AUTOHEAL
    uint32_t changed_total = 0;
    for (uint8_t pin = 0; pin < 16; ++pin) {
        if ((mask & (1u << pin)) == 0u)
            continue;

        uint8_t desired = mcp_relay_read_pin(pin); /* relay is canonical */
        /* tag + idempotent write; logs on real changes */
        changed_total += set_relay_state_with_tag(GUARDIAN_TAG, pin, desired, NULL, NULL);
    }

    /* verify */
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
