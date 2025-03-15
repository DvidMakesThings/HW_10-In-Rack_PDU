/**
 * Copyright (c) 2022 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "wizchip_conf.h"
#include "socket.h"
#include "w5x00_gpio_irq.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
static void (*callback_ptr)(void);

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* GPIO */
void wizchip_gpio_interrupt_initialize(uint8_t socket, void (*callback)(void))
{
    uint16_t reg_val;
    int ret_val;

    reg_val = (SIK_CONNECTED | SIK_DISCONNECTED | SIK_RECEIVED | SIK_TIMEOUT); // except SendOK
    ret_val = ctlsocket(socket, CS_SET_INTMASK, (void *)&reg_val);

#if (_WIZCHIP_ == W5100S)
    reg_val = (1 << socket);
#elif (_WIZCHIP_ == W5500)
    reg_val = ((1 << socket) << 8);
#endif
    ret_val = ctlwizchip(CW_SET_INTRMASK, (void *)&reg_val);

    callback_ptr = callback;
    gpio_set_irq_enabled_with_callback(PIN_INT, GPIO_IRQ_EDGE_FALL, true, &wizchip_gpio_interrupt_callback);
}

static void wizchip_gpio_interrupt_callback(uint gpio, uint32_t events)
{
    if (callback_ptr != NULL)
    {
        callback_ptr();
    }
}