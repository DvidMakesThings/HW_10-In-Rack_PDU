/*
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _WIZNET_SPI_FUNCS_H_
#define _WIZNET_SPI_FUNCS_H_

#include <stdint.h>

typedef struct wiznet_spi_config {
    uint8_t data_in_pin;
    uint8_t data_out_pin;
    uint8_t cs_pin;
    uint8_t clock_pin;
    uint8_t irq_pin;
    uint8_t reset_pin;
    uint16_t clock_div_major;
    uint8_t clock_div_minor;
    uint8_t spi_hw_instance;
} wiznet_spi_config_t;

typedef struct wiznet_spi_funcs** wiznet_spi_handle_t;

typedef struct wiznet_spi_funcs {
    void (*close)(wiznet_spi_handle_t funcs);
    void (*set_active)(wiznet_spi_handle_t funcs);
    void (*set_inactive)(void);
    void (*frame_start)(void);
    void (*frame_end)(void);
    uint8_t (*read_byte)(void);
    void (*write_byte)(uint8_t tx_data);
    void (*read_buffer)(uint8_t *pBuf, uint16_t len);
    void (*write_buffer)(uint8_t *pBuf, uint16_t len);
    void (*reset)(wiznet_spi_handle_t funcs);
} wiznet_spi_funcs_t;

#endif
