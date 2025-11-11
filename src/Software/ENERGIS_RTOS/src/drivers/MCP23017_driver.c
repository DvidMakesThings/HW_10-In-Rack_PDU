/**
 * @file src/drivers/MCP23017_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details
 * All functions accept the I2C address via the registered device context.
 * Uses FreeRTOS mutex for thread-safety, shadowed OLAT writes to avoid
 * torn RMW, conservative I2C retries with small delays, and optional HW reset.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ===================== Tunables (safe, conservative) ===================== */
#ifndef MCP_I2C_MAX_RETRIES
#define MCP_I2C_MAX_RETRIES 3
#endif

#ifndef MCP_I2C_RETRY_DELAY_US
#define MCP_I2C_RETRY_DELAY_US 300
#endif

#ifndef MCP_RESET_PULSE_MS
#define MCP_RESET_PULSE_MS 5
#endif

#ifndef MCP_POST_RESET_MS
#define MCP_POST_RESET_MS 10
#endif

/* Max distinct devices we track (addresses 0x20…0x27 fits well under this) */
#ifndef MCP_MAX_DEVICES
#define MCP_MAX_DEVICES 8
#endif

/* ============================ Local registry ============================= */

static mcp23017_t g_devs[MCP_MAX_DEVICES];
static size_t g_dev_count = 0;

/** 
 * @brief Find existing device by address on same bus. 
 * 
 * @param i2c I2C bus instance
 * @param addr 7-bit I2C address
 * @return Pointer to existing device context, or NULL if not found
*/
static mcp23017_t *_find_dev(i2c_inst_t *i2c, uint8_t addr) {
    for (size_t i = 0; i < g_dev_count; ++i) {
        if (g_devs[i].i2c == i2c && g_devs[i].addr == addr) {
            return &g_devs[i];
        }
    }
    return NULL;
}

/** 
 * @brief I2C robust write: reg+1byte, with retries. 
 * 
 * @param i2c I2C bus instance
 * @param addr 7-bit I2C address
 * @param reg Register address
 * @param val Value to write
 * @return true on success, false on failure
*/
static bool _i2c_wr(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    for (int attempt = 0; attempt < MCP_I2C_MAX_RETRIES; ++attempt) {
        int n = i2c_write_blocking(i2c, addr, buf, 2, false);
        if (n == 2)
            return true;
        busy_wait_us(MCP_I2C_RETRY_DELAY_US);
    }
    ERROR_PRINT("[MCP] I2C write fail addr=0x%02X reg=0x%02X\r\n", addr, reg);
    return false;
}

/** 
 * @brief I2C robust read: write reg, repeated-start, read 1 byte, with retries. 
 *
 * @param i2c I2C bus instance
 * @param addr 7-bit I2C address
 * @param reg Register address
 * @param out Pointer to output byte
 * @return true on success, false on failure 
 */
static bool _i2c_rd(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t *out) {
    if (!out)
        return false;
    for (int attempt = 0; attempt < MCP_I2C_MAX_RETRIES; ++attempt) {
        int n = i2c_write_blocking(i2c, addr, &reg, 1, true);
        if (n == 1) {
            n = i2c_read_blocking(i2c, addr, out, 1, false);
            if (n == 1)
                return true;
        }
        busy_wait_us(MCP_I2C_RETRY_DELAY_US);
    }
    ERROR_PRINT("[MCP] I2C read fail addr=0x%02X reg=0x%02X\r\n", addr, reg);
    return false;
}

/* ============================ Public API ============================ */

mcp23017_t *mcp_register(i2c_inst_t *i2c, uint8_t addr, int8_t rst_gpio) {
    if (!i2c || addr < 0x20 || addr > 0x27) {
        ERROR_PRINT("[MCP] register bad args i2c=%p addr=0x%02X\r\n", i2c, addr);
        return NULL;
    }
    mcp23017_t *dev = _find_dev(i2c, addr);
    if (dev)
        return dev;

    if (g_dev_count >= MCP_MAX_DEVICES) {
        ERROR_PRINT("[MCP] registry full\r\n");
        return NULL;
    }

    dev = &g_devs[g_dev_count++];
    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->addr = addr;
    dev->rst_gpio = rst_gpio;
    dev->olat_a = 0x00;
    dev->olat_b = 0x00;
    dev->inited = false;

    dev->mutex = xSemaphoreCreateMutex();
    if (!dev->mutex) {
        ERROR_PRINT("[MCP] mutex create failed\r\n");
        g_dev_count--;
        return NULL;
    }

    if (rst_gpio >= 0) {
        gpio_init((uint)rst_gpio);
        gpio_set_dir((uint)rst_gpio, true);
        gpio_put((uint)rst_gpio, 1);
    }
    return dev;
}
void mcp_init(mcp23017_t *dev) {
    if (!dev)
        return;
    if (dev->inited)
        return;

    if (dev->rst_gpio >= 0) {
        gpio_put((uint)dev->rst_gpio, 0);
        sleep_ms(MCP_RESET_PULSE_MS);
        gpio_put((uint)dev->rst_gpio, 1);
        sleep_ms(MCP_POST_RESET_MS);
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_IOCON, 0x20); /* SEQOP=1 */
    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_IOCONB, 0x20);
    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_IODIRA, 0x00); /* all out */
    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_IODIRB, 0x00);
    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_GPPUA, 0x00);
    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_GPPUB, 0x00);

    uint8_t a = 0, b = 0;
    if (!_i2c_rd(dev->i2c, dev->addr, MCP23017_OLATA, &a))
        a = 0x00;
    if (!_i2c_rd(dev->i2c, dev->addr, MCP23017_OLATB, &b))
        b = 0x00;
    dev->olat_a = a;
    dev->olat_b = b;
    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_OLATA, dev->olat_a);
    (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_OLATB, dev->olat_b);

    dev->inited = true;
    xSemaphoreGive(dev->mutex);

    ECHO("[MCP] init 0x%02X OK\n", dev->addr);
}

bool mcp_write_reg(mcp23017_t *dev, uint8_t reg, uint8_t value) {
    if (!dev)
        return false;
    if (!dev->inited)
        mcp_init(dev);

    xSemaphoreTake(dev->mutex, portMAX_DELAY);
    if (reg == MCP23017_OLATA)
        dev->olat_a = value;
    if (reg == MCP23017_OLATB)
        dev->olat_b = value;
    bool ok = _i2c_wr(dev->i2c, dev->addr, reg, value);
    xSemaphoreGive(dev->mutex);
    return ok;
}

bool mcp_read_reg(mcp23017_t *dev, uint8_t reg, uint8_t *out) {
    if (!dev || !out)
        return false;
    if (!dev->inited)
        mcp_init(dev);
    xSemaphoreTake(dev->mutex, portMAX_DELAY);
    bool ok = _i2c_rd(dev->i2c, dev->addr, reg, out);
    xSemaphoreGive(dev->mutex);
    return ok;
}

void mcp_set_direction(mcp23017_t *dev, uint8_t pin, uint8_t direction) {
    if (!dev || pin > 15)
        return;
    if (!dev->inited)
        mcp_init(dev);
    const bool is_a = (pin < 8);
    const uint8_t bit = pin & 7;
    const uint8_t reg = is_a ? MCP23017_IODIRA : MCP23017_IODIRB;

    xSemaphoreTake(dev->mutex, portMAX_DELAY);
    uint8_t cur = 0;
    (void)_i2c_rd(dev->i2c, dev->addr, reg, &cur);
    if (direction)
        cur |= (uint8_t)(1u << bit); /* input */
    else
        cur &= ~(uint8_t)(1u << bit); /* output */
    (void)_i2c_wr(dev->i2c, dev->addr, reg, cur);
    xSemaphoreGive(dev->mutex);
}

void mcp_write_pin(mcp23017_t *dev, uint8_t pin, uint8_t value) {
    if (!dev || pin > 15)
        return;
    if (!dev->inited)
        mcp_init(dev);

    const bool is_a = (pin < 8);
    const uint8_t bit = pin & 7;
    const uint8_t reg = is_a ? MCP23017_OLATA : MCP23017_OLATB;

    xSemaphoreTake(dev->mutex, portMAX_DELAY);
    if (is_a) {
        if (value)
            dev->olat_a |= (uint8_t)(1u << bit);
        else
            dev->olat_a &= ~(uint8_t)(1u << bit);
        (void)_i2c_wr(dev->i2c, dev->addr, reg, dev->olat_a);
    } else {
        if (value)
            dev->olat_b |= (uint8_t)(1u << bit);
        else
            dev->olat_b &= ~(uint8_t)(1u << bit);
        (void)_i2c_wr(dev->i2c, dev->addr, reg, dev->olat_b);
    }
    xSemaphoreGive(dev->mutex);
}

uint8_t mcp_read_pin(mcp23017_t *dev, uint8_t pin) {
    if (!dev || pin > 15)
        return 0;
    if (!dev->inited)
        mcp_init(dev);
    const bool is_a = (pin < 8);
    const uint8_t bit = pin & 7;
    const uint8_t reg = is_a ? MCP23017_GPIOA : MCP23017_GPIOB;

    xSemaphoreTake(dev->mutex, portMAX_DELAY);
    uint8_t v = 0;
    (void)_i2c_rd(dev->i2c, dev->addr, reg, &v);
    xSemaphoreGive(dev->mutex);
    return (uint8_t)((v >> bit) & 1u);
}

void mcp_write_mask(mcp23017_t *dev, uint8_t port_ab, uint8_t mask, uint8_t value_bits) {
    if (!dev)
        return;
    if (!dev->inited)
        mcp_init(dev);
    xSemaphoreTake(dev->mutex, portMAX_DELAY);
    if (port_ab == 0) {
        dev->olat_a = (uint8_t)((dev->olat_a & ~mask) | (value_bits & mask));
        (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_OLATA, dev->olat_a);
    } else {
        dev->olat_b = (uint8_t)((dev->olat_b & ~mask) | (value_bits & mask));
        (void)_i2c_wr(dev->i2c, dev->addr, MCP23017_OLATB, dev->olat_b);
    }
    xSemaphoreGive(dev->mutex);
}

void mcp_resync_from_hw(mcp23017_t *dev) {
    if (!dev)
        return;
    if (!dev->inited)
        mcp_init(dev);
    xSemaphoreTake(dev->mutex, portMAX_DELAY);
    uint8_t a = 0, b = 0;
    (void)_i2c_rd(dev->i2c, dev->addr, MCP23017_OLATA, &a);
    (void)_i2c_rd(dev->i2c, dev->addr, MCP23017_OLATB, &b);
    dev->olat_a = a;
    dev->olat_b = b;
    xSemaphoreGive(dev->mutex);
}

#ifndef MCP_MB_RST
#define MCP_MB_RST -1
#endif
#ifndef MCP_DP_RST
#define MCP_DP_RST -1
#endif

static mcp23017_t *g_mcp_relay = NULL;     /* I2C1 @ MCP_RELAY_ADDR   */
static mcp23017_t *g_mcp_display = NULL;   /* I2C0 @ MCP_DISPLAY_ADDR */
static mcp23017_t *g_mcp_selection = NULL; /* I2C0 @ MCP_SELECTION_ADDR */

void MCP2017_Init(void) {
    static bool buses_inited = false;

    /* 1) If DISPLAY and SELECTION share a reset pin, pulse it ONCE here. */
    if (MCP_DP_RST >= 0) {
        gpio_init((uint)MCP_DP_RST);
        gpio_set_dir((uint)MCP_DP_RST, GPIO_OUT);
        gpio_put((uint)MCP_DP_RST, 0);
        sleep_ms(MCP_RESET_PULSE_MS);
        gpio_put((uint)MCP_DP_RST, 1);
        sleep_ms(MCP_POST_RESET_MS);
    }

    /* 2) Register devices. Important:
          - Relay keeps its own reset (MCP_MB_RST).
          - Display & Selection use -1 so the driver will NOT toggle the shared reset again. */
    g_mcp_relay = mcp_register(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, MCP_MB_RST);
    g_mcp_display = mcp_register(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, -1);
    g_mcp_selection = mcp_register(MCP23017_SELECTION_I2C, MCP_SELECTION_ADDR, -1);

    /* 3) Init all (no further reset pulses for display/selection). */
    if (g_mcp_relay)
        mcp_init(g_mcp_relay);
    if (g_mcp_display)
        mcp_init(g_mcp_display);
    if (g_mcp_selection)
        mcp_init(g_mcp_selection);

    DEBUG_PRINT("\t[MCP] Found devices:\r\n");
    DEBUG_PRINT("\t[MCP] relay=%p \n", (void *)g_mcp_relay);
    DEBUG_PRINT("\t[MCP] display=%p\n", (void *)g_mcp_display);
    DEBUG_PRINT("\t[MCP] selection=%p\n", (void *)g_mcp_selection);

    setPowerGood(true);
}

mcp23017_t *mcp_relay(void) { return g_mcp_relay; }
mcp23017_t *mcp_display(void) { return g_mcp_display; }
mcp23017_t *mcp_selection(void) { return g_mcp_selection; }

bool mcp_set_channel_state(uint8_t ch, uint8_t value) {
    if (ch > 7)
        return false;
    mcp23017_t *rel = mcp_relay();
    if (!rel)
        return false;

    uint8_t v = value ? 1u : 0u;
    mcp_write_pin(rel, ch, v); /* switch relay */

    mcp23017_t *disp = mcp_display();
    if (disp)
        mcp_write_pin(disp, ch, v); /* mirror 1=LED ON, 0=LED OFF */

    return true;
}

bool mcp_get_channel_state(uint8_t ch) {
    if (ch > 7)
        return false;
    mcp23017_t *rel = mcp_relay();
    if (!rel)
        return false;
    uint8_t v = mcp_read_pin(rel, ch);
    return (v != 0);
}

bool setError(bool state) {
    mcp23017_t *disp = mcp_display();
    if (!disp)
        return false;
    /* Error LED on pin 8 (port B, bit 0), active HIGH */
    mcp_write_pin(disp, FAULT_LED, state ? 1u : 0u);
    return true;
}

bool setPowerGood(bool state) {
    mcp23017_t *disp = mcp_display();
    if (!disp)
        return false;
    /* Power Good LED on pin 9 (port B, bit 1), active HIGH */
    mcp_write_pin(disp, PWR_LED, state ? 1u : 0u);
    return true;
}

bool setNetworkLink(bool state) {
    mcp23017_t *disp = mcp_display();
    if (!disp)
        return false;
    /* Network Link LED on pin 10 (port B, bit 2), active HIGH */
    mcp_write_pin(disp, ETH_LED, state ? 1u : 0u);
    return true;
}
