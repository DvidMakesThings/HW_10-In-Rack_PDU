/**
 * @file MCP23017_driver.c
 * @author DvidMakesThings - David Sipos
 * 
 * @version 2.0.0
 * @date 2025-12-16
 *
 * @details
 * Complete rewrite of MCP23017 driver with simplified, deterministic design.
 *
 * Key Changes from v1.x:
 * - Removed redundant bus mutex (per-device mutex is sufficient)
 * - Simplified I2C operations with clear retry logic
 * - Direct error reporting to caller
 * - Cleaner separation of concerns
 *
 * Thread Safety:
 * - Each device has its own mutex
 * - All public APIs acquire mutex before I2C operations
 * - Shadow registers prevent torn read-modify-write
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define MCP_TAG "[MCP]"

/*
 * CRITICAL: Do NOT use ERROR_PRINT or ERROR_PRINT_CODE in this driver!
 *
 * ERROR_PRINT calls Switch_SetFaultLed() which tries to acquire s_switch_mutex.
 * If we're called from SwitchTask (which already holds s_switch_mutex), this
 * causes a self-deadlock because s_switch_mutex is non-recursive.
 *
 * The task blocks for SWITCH_MUTEX_TIMEOUT_MS (1000ms) waiting on itself,
 * which is the root cause of "OLATA write took 1002 ms" during stress test.
 *
 * Use MCP_ERROR_CODE for driver errors - route through ERROR_PRINT_DEBUG to avoid LED.
 */
#define MCP_ERROR_CODE(code, fmt, ...)                                                             \
    do {                                                                                           \
        ERROR_PRINT_DEBUG("0x%04X " fmt, (unsigned)(code), ##__VA_ARGS__);                         \
    } while (0)

#define MCP_WARNING_CODE(code, fmt, ...)                                                           \
    do {                                                                                           \
        WARNING_PRINT("0x%04X " fmt, (unsigned)(code), ##__VA_ARGS__);                             \
    } while (0)

/* ==================== Device Registry ==================== */

/**
 * @brief Static array of registered devices.
 */
static mcp23017_t g_devices[MCP_MAX_DEVICES];

/**
 * @brief Number of registered devices.
 */
static size_t g_device_count = 0;

/**
 * @brief Global device pointers for board-specific MCPs.
 */
static mcp23017_t *g_mcp_relay = NULL;
static mcp23017_t *g_mcp_display = NULL;
static mcp23017_t *g_mcp_selection = NULL;

/* ==================== Internal I2C Operations ==================== */

/* ==================== Internal Helpers ==================== */

/**
 * @brief Find existing device by I2C bus and address.
 *
 * @param i2c I2C bus instance
 * @param addr 7-bit device address
 * @return Pointer to device context, or NULL if not found.
 */
static mcp23017_t *find_device(i2c_inst_t *i2c, uint8_t addr) {
    for (size_t i = 0; i < g_device_count; i++) {
        if (g_devices[i].i2c == i2c && g_devices[i].addr == addr) {
            return &g_devices[i];
        }
    }
    return NULL;
}

/**
 * @brief Pulse reset GPIO if configured.
 *
 * @param gpio Reset GPIO pin number
 */
static void pulse_reset(int8_t gpio) {
    if (gpio >= 0) {
        gpio_put((uint)gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(MCP_RESET_PULSE_MS));
        gpio_put((uint)gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(MCP_POST_RESET_MS));
    }
}

/* Diagnostic function removed: no-op to keep linkage if referenced */
static inline void mcp_diag_display_ping(mcp23017_t *dev, uint8_t reg_failed) {
    (void)dev;
    (void)reg_failed;
}

/* ==================== Public Device Registration ==================== */

mcp23017_t *mcp_register(i2c_inst_t *i2c, uint8_t addr, int8_t rst_gpio) {
    /* Validate parameters */
    if (!i2c || addr < 0x20 || addr > 0x27) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_MCP23017, 0x03);
        MCP_ERROR_CODE(errorcode, "%s Invalid register params: i2c=%p addr=0x%02X\r\n", MCP_TAG,
                       (void *)i2c, addr);
        // Storage_EnqueueErrorCode(errorcode);
#endif
        return NULL;
    }

    /* Check if already registered */
    mcp23017_t *existing = find_device(i2c, addr);
    if (existing) {
        return existing;
    }

    /* Check for registry space */
    if (g_device_count >= MCP_MAX_DEVICES) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_MCP23017, 0x04);
        MCP_ERROR_CODE(errorcode, "%s Device registry full\r\n", MCP_TAG);
        // Storage_EnqueueErrorCode(errorcode);
#endif
        return NULL;
    }

    /* Allocate new device slot */
    mcp23017_t *dev = &g_devices[g_device_count];
    memset(dev, 0, sizeof(*dev));

    dev->i2c = i2c;
    dev->addr = addr;
    dev->rst_gpio = rst_gpio;
    dev->olat_a = 0x00;
    dev->olat_b = 0x00;
    dev->inited = false;

    /* Create per-device mutex */
    dev->mutex = xSemaphoreCreateMutex();
    if (!dev->mutex) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_MCP23017, 0x05);
        MCP_ERROR_CODE(errorcode, "%s Mutex create failed for addr=0x%02X\r\n", MCP_TAG, addr);
        // Storage_EnqueueErrorCode(errorcode);
#endif
        return NULL;
    }

    /* Initialize reset GPIO if used */
    if (rst_gpio >= 0) {
        gpio_init((uint)rst_gpio);
        gpio_set_dir((uint)rst_gpio, GPIO_OUT);
        gpio_put((uint)rst_gpio, 1); /* Active-low reset, keep high */
    }

    g_device_count++;
    /* Debug print removed */

    return dev;
}

void mcp_init(mcp23017_t *dev) {
    if (!dev) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_MCP23017, 0x06);
        MCP_ERROR_CODE(errorcode, "%s NULL device in init\r\n", MCP_TAG);
        // Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    if (dev->inited) {
        return;
    }

    /* Reset device if GPIO connected */
    if (dev->rst_gpio >= 0) {
        pulse_reset(dev->rst_gpio);
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    /* Configure IOCON: BANK=0, SEQOP=1 (disable sequential addressing) */
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IOCON, 0x20, MCP_I2C_TIMEOUT_US);
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IOCONB, 0x20, MCP_I2C_TIMEOUT_US);

    /* Configure all pins as outputs */
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IODIRA, 0x00, MCP_I2C_TIMEOUT_US);
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IODIRB, 0x00, MCP_I2C_TIMEOUT_US);

    /* Disable pull-ups */
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_GPPUA, 0x00, MCP_I2C_TIMEOUT_US);
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_GPPUB, 0x00, MCP_I2C_TIMEOUT_US);

    /* Sync shadow registers from hardware */
    uint8_t olat_a = 0, olat_b = 0;
    if (!i2c_bus_read_reg8(dev->i2c, dev->addr, MCP23017_OLATA, &olat_a, MCP_I2C_TIMEOUT_US)) {
        olat_a = 0x00;
    }
    if (!i2c_bus_read_reg8(dev->i2c, dev->addr, MCP23017_OLATB, &olat_b, MCP_I2C_TIMEOUT_US)) {
        olat_b = 0x00;
    }

    dev->olat_a = olat_a;
    dev->olat_b = olat_b;

    /* Write shadows back to ensure consistency */
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_OLATA, dev->olat_a, MCP_I2C_TIMEOUT_US);
    i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_OLATB, dev->olat_b, MCP_I2C_TIMEOUT_US);

    dev->inited = true;

    xSemaphoreGive(dev->mutex);

    /* Debug print removed */
}

bool mcp_recover(mcp23017_t *dev) {
    if (!dev) {
        return false;
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    bool ok = true;
    /* Reprogram essential config; keep SEQOP=1, all outputs, no pull-ups */
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IOCON, 0x20, MCP_I2C_TIMEOUT_US);
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IOCONB, 0x20, MCP_I2C_TIMEOUT_US);
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IODIRA, 0x00, MCP_I2C_TIMEOUT_US);
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_IODIRB, 0x00, MCP_I2C_TIMEOUT_US);
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_GPPUA, 0x00, MCP_I2C_TIMEOUT_US);
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_GPPUB, 0x00, MCP_I2C_TIMEOUT_US);

    /* Re-apply current shadow outputs */
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_OLATA, dev->olat_a, MCP_I2C_TIMEOUT_US);
    ok &= i2c_bus_write_reg8(dev->i2c, dev->addr, MCP23017_OLATB, dev->olat_b, MCP_I2C_TIMEOUT_US);

    xSemaphoreGive(dev->mutex);

#if ERRORLOGGER
    if (!ok) {
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_WARNING, ERR_FID_MCP23017, 0x05);
        MCP_WARNING_CODE(errorcode, "%s Recover failed: addr=0x%02X\r\n", MCP_TAG, dev->addr);
        // Storage_EnqueueWarningCode(errorcode);
    }
#endif

    return ok;
}

/* ==================== Register Operations ==================== */

bool mcp_write_reg(mcp23017_t *dev, uint8_t reg, uint8_t value) {
    if (!dev || !dev->inited) {
        return false;
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    /* Update shadow if writing to OLAT registers */
    if (reg == MCP23017_OLATA) {
        dev->olat_a = value;
    } else if (reg == MCP23017_OLATB) {
        dev->olat_b = value;
    }

    bool result = i2c_bus_write_reg8(dev->i2c, dev->addr, reg, value, MCP_I2C_TIMEOUT_US);

    xSemaphoreGive(dev->mutex);

    return result;
}

bool mcp_read_reg(mcp23017_t *dev, uint8_t reg, uint8_t *out) {
    if (!dev || !dev->inited || !out) {
        return false;
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    bool result = i2c_bus_read_reg8(dev->i2c, dev->addr, reg, out, MCP_I2C_TIMEOUT_US);

    xSemaphoreGive(dev->mutex);

    return result;
}

/* ==================== Pin Operations ==================== */

void mcp_set_direction(mcp23017_t *dev, uint8_t pin, uint8_t direction) {
    if (!dev || !dev->inited || pin > 15) {
        return;
    }

    bool is_port_a = (pin < 8);
    uint8_t bit = pin & 0x07;
    uint8_t reg = is_port_a ? MCP23017_IODIRA : MCP23017_IODIRB;

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    uint8_t current = 0;
    if (i2c_bus_read_reg8(dev->i2c, dev->addr, reg, &current, MCP_I2C_TIMEOUT_US)) {
        if (direction) {
            current |= (uint8_t)(1u << bit); /* Input */
        } else {
            current &= (uint8_t)~(1u << bit); /* Output */
        }
        i2c_bus_write_reg8(dev->i2c, dev->addr, reg, current, MCP_I2C_TIMEOUT_US);
    }

    xSemaphoreGive(dev->mutex);
}

bool mcp_write_pin(mcp23017_t *dev, uint8_t pin, uint8_t value) {
    if (!dev || !dev->inited || pin > 15) {
        return false;
    }

    bool is_port_a = (pin < 8);
    uint8_t bit = pin & 0x07;
    uint8_t reg = is_port_a ? MCP23017_OLATA : MCP23017_OLATB;

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    bool success = false;

    /* Update shadow register and write */
    if (is_port_a) {
        if (value) {
            dev->olat_a |= (uint8_t)(1u << bit);
        } else {
            dev->olat_a &= (uint8_t)~(1u << bit);
        }
        success = i2c_bus_write_reg8(dev->i2c, dev->addr, reg, dev->olat_a, MCP_I2C_TIMEOUT_US);
        if (!success && dev == g_mcp_display) {
            mcp_diag_display_ping(dev, reg);
        }
    } else {
        if (value) {
            dev->olat_b |= (uint8_t)(1u << bit);
        } else {
            dev->olat_b &= (uint8_t)~(1u << bit);
        }
        success = i2c_bus_write_reg8(dev->i2c, dev->addr, reg, dev->olat_b, MCP_I2C_TIMEOUT_US);
    }

    xSemaphoreGive(dev->mutex);
    return success;
}

uint8_t mcp_read_pin(mcp23017_t *dev, uint8_t pin) {
    if (!dev || !dev->inited || pin > 15) {
        return 0;
    }

    bool is_port_a = (pin < 8);
    uint8_t bit = pin & 0x07;
    uint8_t reg = is_port_a ? MCP23017_GPIOA : MCP23017_GPIOB;

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    uint8_t value = 0;
    (void)i2c_bus_read_reg8(dev->i2c, dev->addr, reg, &value, MCP_I2C_TIMEOUT_US);

    xSemaphoreGive(dev->mutex);

    return (uint8_t)((value >> bit) & 1u);
}

bool mcp_write_mask(mcp23017_t *dev, uint8_t port_ab, uint8_t mask, uint8_t value_bits) {
    if (!dev || !dev->inited) {
        return false;
    }

    uint8_t reg = (port_ab == 0) ? MCP23017_OLATA : MCP23017_OLATB;

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    bool success = false;
    if (port_ab == 0) {
        uint8_t new_a = (uint8_t)((dev->olat_a & (uint8_t)~mask) | (value_bits & mask));
        if (new_a == dev->olat_a) {
            success = true; /* no bus write needed */
        } else {
            dev->olat_a = new_a;
            success = i2c_bus_write_reg8(dev->i2c, dev->addr, reg, dev->olat_a, MCP_I2C_TIMEOUT_US);
            if (!success && dev == g_mcp_display) {
                mcp_diag_display_ping(dev, reg);
            }
        }
    } else {
        uint8_t new_b = (uint8_t)((dev->olat_b & (uint8_t)~mask) | (value_bits & mask));
        if (new_b == dev->olat_b) {
            success = true; /* no bus write needed */
        } else {
            dev->olat_b = new_b;
            success = i2c_bus_write_reg8(dev->i2c, dev->addr, reg, dev->olat_b, MCP_I2C_TIMEOUT_US);
        }
    }

    /* No auto-recovery or retries here by design */

    xSemaphoreGive(dev->mutex);
    return success;
}

void mcp_resync_from_hw(mcp23017_t *dev) {
    if (!dev || !dev->inited) {
        return;
    }

    xSemaphoreTake(dev->mutex, portMAX_DELAY);

    uint8_t a = 0, b = 0;
    i2c_bus_read_reg8(dev->i2c, dev->addr, MCP23017_OLATA, &a, MCP_I2C_TIMEOUT_US);
    i2c_bus_read_reg8(dev->i2c, dev->addr, MCP23017_OLATB, &b, MCP_I2C_TIMEOUT_US);
    dev->olat_a = a;
    dev->olat_b = b;

    xSemaphoreGive(dev->mutex);
}

/* ==================== Board-Specific Functions ==================== */

#ifndef MCP_MB_RST
#define MCP_MB_RST -1
#endif

#ifndef MCP_DP_RST
#define MCP_DP_RST -1
#endif

void MCP2017_Init(void) {
    /* Pulse shared reset for display panel MCPs first */
    if (MCP_DP_RST >= 0) {
        gpio_init((uint)MCP_DP_RST);
        gpio_set_dir((uint)MCP_DP_RST, GPIO_OUT);
        gpio_put((uint)MCP_DP_RST, 0);
        sleep_ms(MCP_RESET_PULSE_MS);
        gpio_put((uint)MCP_DP_RST, 1);
        sleep_ms(MCP_POST_RESET_MS);
    }

    /* Register devices.
     * Relay has its own reset pin.
     * Display and Selection share MCP_DP_RST (already pulsed), so pass -1. */
    g_mcp_relay = mcp_register(MCP23017_RELAY_I2C, MCP_RELAY_ADDR, MCP_MB_RST);
    g_mcp_display = mcp_register(MCP23017_DISPLAY_I2C, MCP_DISPLAY_ADDR, MCP_DP_RST);
    g_mcp_selection = mcp_register(MCP23017_SELECTION_I2C, MCP_SELECTION_ADDR, -1);

    /* Initialize all devices */
    if (g_mcp_relay) {
        mcp_init(g_mcp_relay);
    }
    if (g_mcp_display) {
        mcp_init(g_mcp_display);
    }
    if (g_mcp_selection) {
        mcp_init(g_mcp_selection);
    }

    /* Debug print removed */

    /* Turn on power good LED */
    Switch_SetPwrLed(true, 10);
}

mcp23017_t *mcp_relay(void) { return g_mcp_relay; }

mcp23017_t *mcp_display(void) { return g_mcp_display; }

mcp23017_t *mcp_selection(void) { return g_mcp_selection; }