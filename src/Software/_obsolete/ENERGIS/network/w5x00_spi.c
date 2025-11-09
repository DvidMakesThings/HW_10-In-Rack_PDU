/**
 * Copyright (c) 2022 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file w5x00_spi.c
 * @defgroup network Network
 * @brief Network drivers and utilities for WIZnet Ethernet chips.
 * @{
 *
 * @defgroup net5 5. W5x00 SPI
 * @ingroup network
 * @brief SPI driver for W5x00 Ethernet chips.
 * @{
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>

#include "port_common.h"

#include "../CONFIG.h"
#include "board_list.h"
#include "w5x00_spi.h"
#include "wizchip_conf.h"

#if (DEVICE_BOARD_NAME == W55RP20_EVB_PICO)
#include "wiznet_spi_pio.h"
#endif

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
static critical_section_t g_wizchip_cri_sec;

#ifdef USE_SPI_DMA
static uint dma_tx;
static uint dma_rx;
static dma_channel_config dma_channel_config_tx;
static dma_channel_config dma_channel_config_rx;
#endif

#ifdef USE_SPI_PIO
wiznet_spi_config_t g_spi_config = {
    .data_in_pin = PIN_MISO,
    .data_out_pin = PIN_MOSI,
    .cs_pin = PIN_CS,
    .clock_pin = PIN_SCK,
    .irq_pin = PIN_IRQ,
    .reset_pin = PIN_RST,
    .clock_div_major = 2,
    .clock_div_minor = 0,
};

wiznet_spi_handle_t spi_handle;
#endif

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
static inline void wizchip_select(void) { gpio_put(PIN_CS, 0); }

static inline void wizchip_deselect(void) { gpio_put(PIN_CS, 1); }

void wizchip_reset() {
    gpio_init(PIN_RST);

#ifdef USE_SPI_PIO
    gpio_pull_up(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    sleep_ms(5);
#else
    gpio_set_dir(PIN_RST, GPIO_OUT);
#endif

    gpio_put(PIN_RST, 0);
    sleep_ms(100);

    gpio_put(PIN_RST, 1);
    sleep_ms(100);

    bi_decl(bi_1pin_with_name(PIN_RST, "W5x00 RESET"));
}

#ifndef USE_SPI_PIO
static uint8_t wizchip_read(void) {
    uint8_t rx_data = 0;
    uint8_t tx_data = 0xFF;

    spi_read_blocking(SPI_PORT, tx_data, &rx_data, 1);

    return rx_data;
}

static void wizchip_write(uint8_t tx_data) { spi_write_blocking(SPI_PORT, &tx_data, 1); }

#ifdef USE_SPI_DMA
static void wizchip_read_burst(uint8_t *pBuf, uint16_t len) {
    uint8_t dummy_data = 0xFF;

    channel_config_set_read_increment(&dma_channel_config_tx, false);
    channel_config_set_write_increment(&dma_channel_config_tx, false);
    dma_channel_configure(dma_tx, &dma_channel_config_tx,
                          &spi_get_hw(SPI_PORT)->dr, // write address
                          &dummy_data,               // read address
                          len,    // element count (each element is of size transfer_data_size)
                          false); // don't start yet

    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, true);
    dma_channel_configure(dma_rx, &dma_channel_config_rx,
                          pBuf,                      // write address
                          &spi_get_hw(SPI_PORT)->dr, // read address
                          len,    // element count (each element is of size transfer_data_size)
                          false); // don't start yet

    dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
    dma_channel_wait_for_finish_blocking(dma_rx);
}

static void wizchip_write_burst(uint8_t *pBuf, uint16_t len) {
    uint8_t dummy_data;

    channel_config_set_read_increment(&dma_channel_config_tx, true);
    channel_config_set_write_increment(&dma_channel_config_tx, false);
    dma_channel_configure(dma_tx, &dma_channel_config_tx,
                          &spi_get_hw(SPI_PORT)->dr, // write address
                          pBuf,                      // read address
                          len,    // element count (each element is of size transfer_data_size)
                          false); // don't start yet

    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, false);
    dma_channel_configure(dma_rx, &dma_channel_config_rx,
                          &dummy_data,               // write address
                          &spi_get_hw(SPI_PORT)->dr, // read address
                          len,    // element count (each element is of size transfer_data_size)
                          false); // don't start yet

    dma_start_channel_mask((1u << dma_tx) | (1u << dma_rx));
    dma_channel_wait_for_finish_blocking(dma_rx);
}
#endif
#endif

static void wizchip_critical_section_lock(void) {
    critical_section_enter_blocking(&g_wizchip_cri_sec);
}

static void wizchip_critical_section_unlock(void) { critical_section_exit(&g_wizchip_cri_sec); }

void wizchip_spi_initialize(void) {
#ifdef USE_SPI_PIO
    spi_handle = wiznet_spi_pio_open(&g_spi_config);
    (*spi_handle)->set_active(spi_handle);

#else
    // this example will use SPI0 at 5MHz
    spi_init(W5500_SPI_INSTANCE, SPI_SPEED_W5500);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    // make the SPI pins available to picotool
    bi_decl(bi_3pins_with_func(PIN_MISO, PIN_MOSI, PIN_SCK, GPIO_FUNC_SPI));

    // chip select is active-low, so we'll initialise it to a driven-high state
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);

    // make the SPI pins available to picotool
    bi_decl(bi_1pin_with_name(PIN_CS, "W5x00 CHIP SELECT"));

#ifdef USE_SPI_DMA
    dma_tx = dma_claim_unused_channel(true);
    dma_rx = dma_claim_unused_channel(true);

    dma_channel_config_tx = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&dma_channel_config_tx, DMA_SIZE_8);
    channel_config_set_dreq(&dma_channel_config_tx, DREQ_SPI0_TX);

    // We set the inbound DMA to transfer from the SPI receive FIFO to a memory buffer paced by the
    // SPI RX FIFO DREQ We coinfigure the read address to remain unchanged for each element, but the
    // write address to increment (so data is written throughout the buffer)
    dma_channel_config_rx = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&dma_channel_config_rx, DMA_SIZE_8);
    channel_config_set_dreq(&dma_channel_config_rx, DREQ_SPI0_RX);
    channel_config_set_read_increment(&dma_channel_config_rx, false);
    channel_config_set_write_increment(&dma_channel_config_rx, true);
#endif
#endif
}

void wizchip_cris_initialize(void) {
    critical_section_init(&g_wizchip_cri_sec);
    reg_wizchip_cris_cbfunc(wizchip_critical_section_lock, wizchip_critical_section_unlock);
}

void wizchip_initialize(void) {

#ifdef USE_SPI_PIO
    (*spi_handle)->frame_end();

    reg_wizchip_spi_cbfunc((*spi_handle)->read_byte, (*spi_handle)->write_byte);
    reg_wizchip_spiburst_cbfunc((*spi_handle)->read_buffer, (*spi_handle)->write_buffer);
    reg_wizchip_cs_cbfunc((*spi_handle)->frame_start, (*spi_handle)->frame_end);
#else

    /* Deselect the FLASH : chip select high */
    wizchip_deselect();

    /* CS function register */
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);

    /* SPI function register */
    reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);
#endif
#ifdef USE_SPI_DMA
    reg_wizchip_spiburst_cbfunc(wizchip_read_burst, wizchip_write_burst);
#endif

    /* W5x00 initialize */
    uint8_t temp;
#if (_WIZCHIP_ == W5100S)
    uint8_t memsize[2][4] = {{2, 2, 2, 2}, {2, 2, 2, 2}};
#elif (_WIZCHIP_ == W5500)
    // uint8_t memsize[2][8] = {{2, 2, 2, 2, 2, 2, 2, 2}, {2, 2, 2, 2, 2, 2, 2, 2}};

    uint8_t memsize[2][8] = {
        {8, 4, 4, 0, 0, 0, 0, 0}, // 8KB TX for socket 0
        {8, 4, 4, 0, 0, 0, 0, 0}  // 8KB RX for socket 0
    };

#endif

    if (ctlwizchip(CW_INIT_WIZCHIP, (void *)memsize) == -1) {
        printf(" W5x00 initialized fail\n");

        return;
    }

    /* Check PHY link status */
    do {
        if (ctlwizchip(CW_GET_PHYLINK, (void *)&temp) == -1) {
            printf(" Unknown PHY link status\n");

            return;
        }
    } while (temp == PHY_LINK_OFF);
}

void wizchip_check(void) {
#if (_WIZCHIP_ == W5100S)
    /* Read version register */
    if (getVER() != 0x51) {
        printf(" ACCESS ERR : VERSION != 0x51, read value = 0x%02x\n", getVER());

        while (1)
            ;
    }
#elif (_WIZCHIP_ == W5500)
    /* Read version register */
    if (getVERSIONR() != 0x04) {
        printf(" ACCESS ERR : VERSION != 0x04, read value = 0x%02x\n", getVERSIONR());

        while (1)
            ;
    } else {
        printf("[INFO] W5500 version: 0x%02x\n", getVERSIONR());
    }
#endif
}

/* Network */
void network_initialize(wiz_NetInfo net_info) { ctlnetwork(CN_SET_NETINFO, (void *)&net_info); }

void print_network_information(wiz_NetInfo net_info) {
    uint8_t tmp_str[8] = {
        0,
    };

    ctlnetwork(CN_GET_NETINFO, (void *)&net_info);
    ctlwizchip(CW_GET_ID, (void *)tmp_str);

    if (net_info.dhcp == NETINFO_DHCP) {
        ECHO("=============================================\n");
        ECHO(" %s network configuration : DHCP\n", (char *)tmp_str);
    } else {
        ECHO("=============================================\n");
        ECHO(" %s network configuration : static\n", (char *)tmp_str);
    }

    ECHO(" MAC         : %02X:%02X:%02X:%02X:%02X:%02X\n", net_info.mac[0], net_info.mac[1],
         net_info.mac[2], net_info.mac[3], net_info.mac[4], net_info.mac[5]);
    ECHO(" IP          : %d.%d.%d.%d\n", net_info.ip[0], net_info.ip[1], net_info.ip[2],
         net_info.ip[3]);
    ECHO(" Subnet Mask : %d.%d.%d.%d\n", net_info.sn[0], net_info.sn[1], net_info.sn[2],
         net_info.sn[3]);
    ECHO(" Gateway     : %d.%d.%d.%d\n", net_info.gw[0], net_info.gw[1], net_info.gw[2],
         net_info.gw[3]);
    ECHO(" DNS         : %d.%d.%d.%d\n", net_info.dns[0], net_info.dns[1], net_info.dns[2],
         net_info.dns[3]);
    ECHO("=============================================\n\n");
}
