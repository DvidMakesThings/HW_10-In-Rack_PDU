/**
 * @file SwitchTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 3.0
 * @date 2025-11-14
 *
 * @details
 * A simple, deterministic implementation for relay control:
 * - Single mutex serializes all MCP23017 access
 * - Direct read/write calls to MCP driver (no queues)
 * - Display LEDs mirror relay states (best-effort with short backoff)
 *
 * Thread-safety:
 * - All public APIs acquire the SwitchTask mutex and are blocking.
 * - APIs return immediately with detailed codes for callers (see header).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static SemaphoreHandle_t s_mutex = NULL;
static volatile bool s_inited = false;
static volatile bool s_manual_active = false;
/* Cached states for status LEDs to avoid redundant writes */
static volatile bool s_led_fault = false;
static volatile bool s_led_pwr = false;
static volatile bool s_led_eth = false;
/* Backoff for display writes after a failure */
static volatile TickType_t s_disp_backoff_until = 0;

static bool lock(TickType_t ticks) {
    if (!s_mutex)
        return false;
    return xSemaphoreTake(s_mutex, ticks) == pdTRUE;
}
static void unlock(void) {
    if (s_mutex)
        xSemaphoreGive(s_mutex);
}

static uint8_t read_relay_mask(mcp23017_t *rel) {
    uint8_t mask = 0;
    for (uint8_t ch = 0; ch < 8u; ch++) {
        if (mcp_read_pin(rel, ch))
            mask |= (uint8_t)(1u << ch);
    }
    return mask;
}

static void mirror_display_from_relay(void) {
    mcp23017_t *rel = mcp_relay();
    mcp23017_t *disp = mcp_display();
    if (!rel || !rel->inited || !disp || !disp->inited)
        return;
    TickType_t now = xTaskGetTickCount();
    if (now < s_disp_backoff_until)
        return;
    uint8_t mask = read_relay_mask(rel);
    if (!mcp_write_mask(disp, 0, 0xFFu, mask)) {
        s_disp_backoff_until = now + pdMS_TO_TICKS(200);
    }
}

BaseType_t SwitchTask_Init(bool enable) {
    s_inited = false;
    if (!enable)
        return pdPASS;

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex)
            return pdFAIL;
    }
    s_inited = true;
    return pdPASS;
}

bool Switch_IsReady(void) { return (s_inited && s_mutex != NULL); }

switch_result_t Switch_GetState(uint8_t channel, bool *out_state) {
    if (!out_state)
        return SWITCH_ERR_NULL_PARAM;
    if (channel >= 8u)
        return SWITCH_ERR_INVALID_CHANNEL;
    if (!s_inited)
        return SWITCH_ERR_NOT_INIT;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return SWITCH_ERR_MUTEX_TIMEOUT;

    mcp23017_t *rel = mcp_relay();
    if (!rel || !rel->inited) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }
    *out_state = (mcp_read_pin(rel, channel) != 0);
    unlock();
    return SWITCH_OK;
}

switch_result_t Switch_GetAllStates(uint8_t *out_mask) {
    if (!out_mask)
        return SWITCH_ERR_NULL_PARAM;
    if (!s_inited)
        return SWITCH_ERR_NOT_INIT;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return SWITCH_ERR_MUTEX_TIMEOUT;
    mcp23017_t *rel = mcp_relay();
    if (!rel || !rel->inited) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }
    *out_mask = read_relay_mask(rel);
    unlock();
    return SWITCH_OK;
}

switch_result_t Switch_SetChannel(uint8_t channel, bool state) {
    if (channel >= 8u)
        return SWITCH_ERR_INVALID_CHANNEL;
    if (!s_inited)
        return SWITCH_ERR_NOT_INIT;
    if (state && !Overcurrent_IsSwitchingAllowed())
        return SWITCH_ERR_OVERCURRENT;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return SWITCH_ERR_MUTEX_TIMEOUT;

    mcp23017_t *rel = mcp_relay();
    mcp23017_t *disp = mcp_display();
    if (!rel || !rel->inited) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }

    if (!mcp_write_pin(rel, channel, state ? 1u : 0u)) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }

    vTaskDelay(pdMS_TO_TICKS(1));

    if (!mcp_write_pin(disp, channel, state ? 1u : 0u)) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }

    unlock();
    return SWITCH_OK;
}

switch_result_t Switch_Toggle(uint8_t channel) {
    if (channel >= 8u)
        return SWITCH_ERR_INVALID_CHANNEL;
    if (!s_inited)
        return SWITCH_ERR_NOT_INIT;
    bool cur = false;
    switch_result_t r = Switch_GetState(channel, &cur);
    if (r != SWITCH_OK)
        return r;
    return Switch_SetChannel(channel, !cur);
}

switch_result_t Switch_AllOn(void) {
    if (!s_inited)
        return SWITCH_ERR_NOT_INIT;
    if (!Overcurrent_IsSwitchingAllowed())
        return SWITCH_ERR_OVERCURRENT;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return SWITCH_ERR_MUTEX_TIMEOUT;
    mcp23017_t *rel = mcp_relay();
    if (!rel || !rel->inited) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }
    for (uint8_t ch = 0; ch < 8u; ch++) {
        if (!mcp_write_pin(rel, ch, 1u)) {
            unlock();
            return SWITCH_ERR_I2C_FAIL;
        }
    }
    mirror_display_from_relay();
    unlock();
    return SWITCH_OK;
}

switch_result_t Switch_AllOff(void) {
    if (!s_inited)
        return SWITCH_ERR_NOT_INIT;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return SWITCH_ERR_MUTEX_TIMEOUT;
    mcp23017_t *rel = mcp_relay();
    if (!rel || !rel->inited) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }
    for (uint8_t ch = 0; ch < 8u; ch++) {
        if (!mcp_write_pin(rel, ch, 0u)) {
            unlock();
            return SWITCH_ERR_I2C_FAIL;
        }
    }
    mirror_display_from_relay();
    unlock();
    return SWITCH_OK;
}

switch_result_t Switch_SetMask(uint8_t mask) {
    if (!s_inited)
        return SWITCH_ERR_NOT_INIT;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return SWITCH_ERR_MUTEX_TIMEOUT;
    mcp23017_t *rel = mcp_relay();
    if (!rel || !rel->inited) {
        unlock();
        return SWITCH_ERR_I2C_FAIL;
    }
    for (uint8_t ch = 0; ch < 8u; ch++) {
        uint8_t v = (mask & (1u << ch)) ? 1u : 0u;
        if (!mcp_write_pin(rel, ch, v)) {
            unlock();
            return SWITCH_ERR_I2C_FAIL;
        }
    }
    mirror_display_from_relay();
    unlock();
    return SWITCH_OK;
}

bool Switch_GetStateCompat(uint8_t channel, bool *out_state) {
    return (Switch_GetState(channel, out_state) == SWITCH_OK);
}
bool Switch_SetChannelCompat(uint8_t channel, bool state, uint32_t timeout_ms) {
    (void)timeout_ms;
    return (Switch_SetChannel(channel, state) == SWITCH_OK);
}
bool Switch_ToggleCompat(uint8_t channel, uint32_t timeout_ms) {
    (void)timeout_ms;
    return (Switch_Toggle(channel) == SWITCH_OK);
}
bool Switch_AllOnCompat(uint32_t timeout_ms) {
    (void)timeout_ms;
    return (Switch_AllOn() == SWITCH_OK);
}
bool Switch_AllOffCompat(uint32_t timeout_ms) {
    (void)timeout_ms;
    return (Switch_AllOff() == SWITCH_OK);
}
bool Switch_SetMaskCompat(uint8_t mask, uint32_t timeout_ms) {
    (void)timeout_ms;
    return (Switch_SetMask(mask) == SWITCH_OK);
}

bool Switch_SelectAllOff(uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s_inited)
        return false;
    if (!s_manual_active)
        return true;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return false;
    mcp23017_t *sel = mcp_selection();
    if (sel && sel->inited) {
        (void)mcp_write_mask(sel, 0, 0xFFu, 0x00u);
        (void)mcp_write_mask(sel, 1, 0xFFu, 0x00u);
    }
    unlock();
    return true;
}

bool Switch_SelectShow(uint8_t index, bool on, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (index >= 8u)
        return false;
    if (!s_inited)
        return false;
    if (!s_manual_active)
        return true;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return false;
    mcp23017_t *sel = mcp_selection();
    if (sel && sel->inited) {
        (void)mcp_write_mask(sel, 0, 0xFFu, 0x00u);
        (void)mcp_write_mask(sel, 1, 0xFFu, 0x00u);
        if (on)
            (void)mcp_write_pin(sel, (uint8_t)(index & 0x0Fu), 1u);
    }
    unlock();
    return true;
}

bool Switch_SetFaultLed(bool state, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s_inited)
        return false;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return false;
    if (s_led_fault == state) {
        unlock();
        return true;
    }
    mcp23017_t *disp = mcp_display();
    TickType_t now = xTaskGetTickCount();
    if (disp && disp->inited && (now >= s_disp_backoff_until)) {
#ifdef FAULT_LED
        if (!mcp_write_pin(disp, FAULT_LED, state ? 1u : 0u)) {
            s_disp_backoff_until = now + pdMS_TO_TICKS(200);
        }
#endif
    }
    s_led_fault = state;
    unlock();
    return true;
}

bool Switch_SetPwrLed(bool state, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s_inited)
        return false;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return false;
    if (s_led_pwr == state) {
        unlock();
        return true;
    }
    mcp23017_t *disp = mcp_display();
    TickType_t now = xTaskGetTickCount();
    if (disp && disp->inited && (now >= s_disp_backoff_until)) {
#ifdef PWR_LED
        if (!mcp_write_pin(disp, PWR_LED, state ? 1u : 0u)) {
            s_disp_backoff_until = now + pdMS_TO_TICKS(200);
        }
#endif
    }
    s_led_pwr = state;
    unlock();
    return true;
}

bool Switch_SetEthLed(bool state, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s_inited)
        return false;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return false;
    if (s_led_eth == state) {
        unlock();
        return true;
    }
    mcp23017_t *disp = mcp_display();
    TickType_t now = xTaskGetTickCount();
    if (disp && disp->inited && (now >= s_disp_backoff_until)) {
#ifdef ETH_LED
        if (!mcp_write_pin(disp, ETH_LED, state ? 1u : 0u)) {
            s_disp_backoff_until = now + pdMS_TO_TICKS(200);
        }
#endif
    }
    s_led_eth = state;
    unlock();
    return true;
}

bool Switch_SyncFromHardware(uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s_inited)
        return false;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return false;
    mirror_display_from_relay();
    unlock();
    return true;
}

uint8_t Switch_GetCachedMask(void) {
    uint8_t mask = 0;
    if (!s_inited)
        return 0;
    if (!lock(pdMS_TO_TICKS(SWITCH_MUTEX_TIMEOUT_MS)))
        return 0;
    mcp23017_t *rel = mcp_relay();
    if (rel && rel->inited)
        mask = read_relay_mask(rel);
    unlock();
    return mask;
}

void Switch_SetManualPanelActive(bool active) { s_manual_active = active; }

bool Switch_SetRelayPortBMasked(uint8_t mask, uint8_t value, uint32_t timeout_ms) {
    TickType_t wait = (timeout_ms > 0u) ? pdMS_TO_TICKS(timeout_ms) : 0;
    if (!s_inited)
        return true;
    if (!lock(wait))
        return true;
    mcp23017_t *rel = mcp_relay();
    if (rel && rel->inited) {
        (void)mcp_write_mask(rel, 1, mask, value);
    }
    unlock();
    return true;
}
