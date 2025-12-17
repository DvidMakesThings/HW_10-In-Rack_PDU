/**
 * @file src/drivers/button_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.1
 * @date 2025-12-10
 *
 * @details Low-level driver implementation for the front-panel buttons and
 * selection/relay indicators using FreeRTOS.
 *
 * v1.1.1 Changes:
 * - Kept Switch_Toggle for RTOS-cooperative
 *   relay control via SwitchTask
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define BTNDRV_TAG "[BTNDRV]"

/* -------------------- Local helpers ---------------------------------------- */
/**
 * @brief Turn off all selection LEDs.
 * @param None
 * @return None
 *
 */
static inline void drv_sel_all_off(void) {
    /* Selection MCP I2C operations are owned by SwitchTask.
     * If SwitchTask is not ready yet (early bring-up), leave LEDs as-is. */
    if (Switch_IsReady()) {
        (void)Switch_SelectAllOff(0);
    }
}

/* -------------------- Public driver API ------------------------------------ */

void ButtonDrv_InitGPIO(void) {
    gpio_init(BUT_PLUS);
    gpio_pull_up(BUT_PLUS);
    gpio_set_dir(BUT_PLUS, false);
    gpio_init(BUT_MINUS);
    gpio_pull_up(BUT_MINUS);
    gpio_set_dir(BUT_MINUS, false);
    gpio_init(BUT_SET);
    gpio_pull_up(BUT_SET);
    gpio_set_dir(BUT_SET, false);

    drv_sel_all_off();
}

bool ButtonDrv_ReadPlus(void) { return gpio_get(BUT_PLUS) ? true : false; }

bool ButtonDrv_ReadMinus(void) { return gpio_get(BUT_MINUS) ? true : false; }

bool ButtonDrv_ReadSet(void) { return gpio_get(BUT_SET) ? true : false; }

void ButtonDrv_SelectAllOff(void) { drv_sel_all_off(); }

void ButtonDrv_SelectShow(uint8_t index, bool on) {
    /* Selection LEDs are driven via SwitchTask to ensure ButtonTask never blocks on I2C. */
    if (index >= 8u) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTON_DRV, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s ButtonDrv_SelectShow: bad index %u\r\n", BTNDRV_TAG,
                         (unsigned)index);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    if (!Switch_IsReady()) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTON_DRV, 0x2);
        ERROR_PRINT_CODE(errorcode, "%s ButtonDrv_SelectShow: SwitchTask not ready\r\n",
                         BTNDRV_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    if (!Switch_SelectShow(index, on, 0)) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTON_DRV, 0x3);
        ERROR_PRINT_CODE(errorcode, "%s ButtonDrv_SelectShow: enqueue failed\r\n", BTNDRV_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }
}

void ButtonDrv_SelectLeft(uint8_t *io_index, bool led_on) {
    if (!io_index) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTON_DRV, 0x2);
        ERROR_PRINT_CODE(errorcode, "%s ButtonDrv_SelectLeft: NULL io_index pointer\r\n",
                         BTNDRV_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }
    uint8_t idx = *io_index;
    idx = (idx == 0u) ? 7u : (uint8_t)(idx - 1u);
    *io_index = idx;
    ButtonDrv_SelectShow(idx, led_on);
}

void ButtonDrv_SelectRight(uint8_t *io_index, bool led_on) {
    if (!io_index) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTON_DRV, 0x3);
        ERROR_PRINT_CODE(errorcode, "%s ButtonDrv_SelectRight: NULL io_index pointer\r\n",
                         BTNDRV_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }
    uint8_t idx = *io_index;
    idx = (idx == 7u) ? 0u : (uint8_t)(idx + 1u);
    *io_index = idx;
    ButtonDrv_SelectShow(idx, led_on);
}

void ButtonDrv_DoSetShort(uint8_t index) {
    /* Use non-blocking Switch_Toggle via SwitchTask
     * This prevents I2C bus contention and watchdog starvation */
    if (index < 8) {
        (void)Switch_Toggle(index & 0x07u);
    }
}

void ButtonDrv_DoSetLong(void) {
#ifdef FAULT_LED
    /* Route through SwitchTask to prevent i2c0 bus contention */
    if (Switch_IsReady()) {
        Switch_SetFaultLed(false, 0);
    } else {
        /* Fallback for early init */
        mcp23017_t *disp = mcp_display();
        if (disp) {
            mcp_write_pin(disp, FAULT_LED, 0u);
        }
    }
#endif
}

uint32_t ButtonDrv_NowMs(void) { return (uint32_t)to_ms_since_boot(get_absolute_time()); }
