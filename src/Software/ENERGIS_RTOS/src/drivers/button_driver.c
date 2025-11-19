/**
 * @file src/drivers/button_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details Low-level driver implementation for the front-panel buttons and
 * selection/relay indicators using FreeRTOS.
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
    mcp23017_t *sel = mcp_selection();
    if (!sel)
        return;
    mcp_write_mask(sel, 0, 0xFFu, 0x00u);
    mcp_write_mask(sel, 1, 0xFFu, 0x00u);
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
    mcp23017_t *sel = mcp_selection();
    if (!sel) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTON_DRV, 0x1);
        ERROR_PRINT_CODE(errorcode,
                         "%s ButtonDrv_SelectShow: MCP23017 selection device not found\r\n",
                         BTNDRV_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    /* show current only */
    drv_sel_all_off();
    if (on) {
        mcp_write_pin(sel, index & 0x0Fu, 1u);
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
    mcp23017_t *rel = mcp_relay();
    if (!rel) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_BUTTON_DRV, 0x4);
        ERROR_PRINT_CODE(errorcode, "%s ButtonDrv_DoSetShort: MCP23017 relay device not found\r\n",
                         BTNDRV_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    uint8_t cur = mcp_read_pin(rel, index & 0x0Fu);
    uint8_t want = cur ? 0u : 1u;
    (void)mcp_set_channel_state(index & 0x0Fu, want);
}

void ButtonDrv_DoSetLong(void) {
#ifdef FAULT_LED
    mcp23017_t *disp = mcp_display();
    if (disp) {
        mcp_write_pin(disp, FAULT_LED, 0u);
    }
#endif
}

uint32_t ButtonDrv_NowMs(void) { return (uint32_t)to_ms_since_boot(get_absolute_time()); }
