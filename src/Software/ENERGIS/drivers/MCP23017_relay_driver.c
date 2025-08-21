/**
 * @file MCP23017_relay_driver.c
 * @author DvidMakesThings - David Sipos (DvidMakesThings)
 * @brief Driver for the Relay Board's MCP23017 I/O Expander with
 *        race-free, shadow-latched writes to prevent random relay flips.
 * @version 1.1
 * @date 2025-08-21
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 *
 * --------------------------------------------------------------------------
 * WHY THIS REV EXISTS
 * --------------------------------------------------------------------------
 * Previously, mcp_relay_write_pin() performed a read‑modify‑write (RMW) on
 * OLATx for every single bit toggle. When multiple producers (e.g. web UI,
 * SNMP, UART, and HLW8032 MUX control) touched OLATx concurrently—often from
 * different cores/threads—interleaved I²C RMW cycles could drop updates and
 * momentarily write stale bytes. That manifested as "random" relay flips,
 * especially after the system had been running for a while.
 *
 * FIX:
 *  - Maintain software shadow latches (OLAT_A/OLAT_B) for the full 8-bit ports.
 *  - Protect every I²C transaction and shadow update with a mutex.
 *  - Never RMW from the hardware; only update the shadow, then write the
 *    whole byte for the affected port.
 *  - On init, hard-configure IOCON and IODIR, read back OLAT to seed shadow,
 *    then drive a known-safe initial state.
 *  - Provide a fast multi-bit API for batch updates if needed later.
 *
 * RESULT:
 *  - No torn writes. No cross-writer clobber. No random flips.
 * --------------------------------------------------------------------------
 */

#include "MCP23017_relay_driver.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/mutex.h"

/* ------------------------- Local helpers & state ------------------------ */

static mutex_t              s_relay_mutex;
static volatile uint8_t     s_olat_a = 0x00;   // software shadow for OLATA
static volatile uint8_t     s_olat_b = 0x00;   // software shadow for OLATB
static volatile bool        s_inited  = false;

static inline void mcp_lock(void)   { mutex_enter_blocking(&s_relay_mutex); }
static inline void mcp_unlock(void) { mutex_exit(&s_relay_mutex); }

/* Robust I2C write: returns true on success (all bytes written) */
static bool i2c_wr(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    int     n = i2c_write_blocking(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, data, 2, false);
    return (n == 2);
}

/* Robust I2C read: returns true on success (value in *out) */
static bool i2c_rd(uint8_t reg, uint8_t *out) {
    if (!out) return false;
    int n = i2c_write_blocking(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, &reg, 1, true);
    if (n != 1) return false;
    n = i2c_read_blocking(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, out, 1, false);
    return (n == 1);
}

/* Write a full port atomically from shadow. port=0 -> A, 1 -> B */
static bool write_port_from_shadow(uint8_t port_ab) {
    const uint8_t reg = (port_ab == 0) ? MCP23017_OLATA : MCP23017_OLATB;
    const uint8_t val = (port_ab == 0) ? (uint8_t)s_olat_a : (uint8_t)s_olat_b;
    return i2c_wr(reg, val);
}

/* ------------------------------ Public API ----------------------------- */

/**
 * @brief Initializes the relay board's MCP23017.
 *
 * Performs a hardware reset using MCP_REL_RST, configures IOCON/IODIR,
 * seeds software shadows from OLAT, and drives a known state.
 * All I²C access is mutex-guarded to prevent torn RMW updates.
 */
void mcp_relay_init(void) {
    /* One-time mutex init is safe even if called multiple times */
    static bool once = false;
    if (!once) {
        mutex_init(&s_relay_mutex);
        once = true;
    }

    /* Hardware reset pulse */
    gpio_put(MCP_REL_RST, 0);
    sleep_ms(50);
    gpio_put(MCP_REL_RST, 1);
    sleep_ms(50);

    /* Configure device deterministically (protected) */
    mcp_lock();

    /* Put device into a stable IOCON configuration:
       - IOCON.BANK   = 0 (sequential addresses in same bank)
       - IOCON.MIRROR = 0
       - IOCON.SEQOP  = 1 (disable sequential op to avoid internal addr inc)
       - IOCON.DISP   = 0
       - IOCON.HAEN   = 0 (not using hardware address enable)
       - IOCON.ODR    = 0
       - IOCON.INTPOL = 0
       Value: 0b0010_0000 = 0x20
    */
    i2c_wr(MCP23017_IOCON, 0x20);
    i2c_wr(MCP23017_IOCONB, 0x20);

    /* All pins outputs by default for the relay/MUX board */
    i2c_wr(MCP23017_IODIRA, 0x00);
    i2c_wr(MCP23017_IODIRB, 0x00);

    /* No pull-ups on outputs */
    i2c_wr(MCP23017_GPPUA, 0x00);
    i2c_wr(MCP23017_GPPUB, 0x00);

    /* Seed software shadow from silicon OLAT to avoid an initial "jump".
       If read fails, default to 0x00 (all off). */
    uint8_t olata = 0, olatb = 0;
    if (!i2c_rd(MCP23017_OLATA, &olata)) olata = 0x00;
    if (!i2c_rd(MCP23017_OLATB, &olatb)) olatb = 0x00;
    s_olat_a = olata;
    s_olat_b = olatb;

    /* Drive the hardware to match the shadow (known state) */
    write_port_from_shadow(0);
    write_port_from_shadow(1);

    s_inited = true;
    mcp_unlock();
}

/**
 * @brief Writes a value to a specified MCP23017 register (guarded).
 *
 * If OLATA/OLATB is written, the shadow is kept in sync.
 */
void mcp_relay_write_reg(uint8_t reg, uint8_t value) {
    if (!s_inited) mcp_relay_init();
    mcp_lock();

    /* If the caller touches OLAT, keep shadow consistent */
    if (reg == MCP23017_OLATA) s_olat_a = value;
    if (reg == MCP23017_OLATB) s_olat_b = value;

    i2c_wr(reg, value);
    mcp_unlock();
}

/**
 * @brief Reads a value from a specified MCP23017 register (guarded).
 */
uint8_t mcp_relay_read_reg(uint8_t reg) {
    if (!s_inited) mcp_relay_init();
    mcp_lock();
    uint8_t v = 0;
    (void)i2c_rd(reg, &v);
    mcp_unlock();
    return v;
}

/**
 * @brief Sets the direction of a specific MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @param direction 0 = output, 1 = input.
 */
void mcp_relay_set_direction(uint8_t pin, uint8_t direction) {
    if (!s_inited) mcp_relay_init();

    const uint8_t is_port_a = (pin < 8) ? 1 : 0;
    const uint8_t reg       = is_port_a ? MCP23017_IODIRA : MCP23017_IODIRB;
    const uint8_t bit       = pin & 0x07;

    mcp_lock();
    uint8_t cur = 0;
    (void)i2c_rd(reg, &cur);
    if (direction) cur |=  (1u << bit); else cur &= ~(1u << bit);
    i2c_wr(reg, cur);
    mcp_unlock();
}

/**
 * @brief Writes a digital value to a specific MCP23017 pin.
 *        Uses shadowed whole-port writes under a mutex to avoid races.
 *
 * @param pin The pin number (0-15).
 * @param value 0 = low, 1 = high.
 */
void mcp_relay_write_pin(uint8_t pin, uint8_t value) {
    if (!s_inited) mcp_relay_init();

    const uint8_t is_port_a = (pin < 8) ? 1 : 0;
    const uint8_t bit       = pin & 0x07;

    mcp_lock();
    if (is_port_a) {
        if (value) s_olat_a |=  (1u << bit);
        else       s_olat_a &= ~(1u << bit);
        write_port_from_shadow(0);
    } else {
        if (value) s_olat_b |=  (1u << bit);
        else       s_olat_b &= ~(1u << bit);
        write_port_from_shadow(1);
    }
    mcp_unlock();
}

/**
 * @brief Reads the digital value from a specific MCP23017 pin.
 *
 * @param pin The pin number (0-15).
 * @return 0 = low, 1 = high.
 */
uint8_t mcp_relay_read_pin(uint8_t pin) {
    if (!s_inited) mcp_relay_init();

    const uint8_t reg = (pin < 8) ? MCP23017_GPIOA : MCP23017_GPIOB;
    const uint8_t bit = pin & 0x07;

    mcp_lock();
    uint8_t gpio = 0;
    (void)i2c_rd(reg, &gpio);
    mcp_unlock();

    return (gpio >> bit) & 0x01;
}

/**
 * @brief Atomically set/clear a group of bits on a given port from a mask.
 *
 * @param port_ab 0 for Port A, 1 for Port B
 * @param mask    Bits to affect
 * @param value_bits Values for those bits (only masked bits are applied)
 */
void mcp_relay_write_mask(uint8_t port_ab, uint8_t mask, uint8_t value_bits) {
    if (!s_inited) mcp_relay_init();

    mcp_lock();
    if (port_ab == 0) {
        s_olat_a = (s_olat_a & ~mask) | (value_bits & mask);
        write_port_from_shadow(0);
    } else {
        s_olat_b = (s_olat_b & ~mask) | (value_bits & mask);
        write_port_from_shadow(1);
    }
    mcp_unlock();
}

/**
 * @brief Re-sync software shadows from hardware OLAT (optional utility).
 */
void mcp_relay_resync_from_hw(void) {
    if (!s_inited) mcp_relay_init();

    mcp_lock();
    uint8_t a = 0, b = 0;
    (void)i2c_rd(MCP23017_OLATA, &a);
    (void)i2c_rd(MCP23017_OLATB, &b);
    s_olat_a = a;
    s_olat_b = b;
    mcp_unlock();
}
