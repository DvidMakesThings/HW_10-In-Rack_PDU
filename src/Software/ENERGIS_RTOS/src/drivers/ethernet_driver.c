/**
 * @file src/drivers/ethernet_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * Unified RTOS-compatible W5500 driver combining HAL, SPI, and configuration.
 * Thread-safe with FreeRTOS mutex protection for all SPI operations.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/******************************************************************************
 *                          PRIVATE DEFINITIONS                               *
 ******************************************************************************/

/* W5500 SPI command prefixes */
#define W5500_SPI_READ 0x00
#define W5500_SPI_WRITE 0x04
#define W5500_SPI_VDM_OP 0x00

/******************************************************************************
 *                          PRIVATE VARIABLES                                 *
 ******************************************************************************/

/* SPI mutex for thread-safe access */
SemaphoreHandle_t w5500_spi_mutex = NULL;

/* Callback function pointers */
static void (*_cris_enter)(void) = NULL;
static void (*_cris_exit)(void) = NULL;
static void (*_cs_select)(void) = NULL;
static void (*_cs_deselect)(void) = NULL;
static uint8_t (*_spi_read_byte)(void) = NULL;
static void (*_spi_write_byte)(uint8_t) = NULL;

/******************************************************************************
 *                          SPI LOW-LEVEL FUNCTIONS                           *
 ******************************************************************************/

/**
 * @brief CS select (pull CS low)
 */
static inline void _w5500_cs_select(void) { gpio_put(W5500_PIN_CS, 0); }

/**
 * @brief CS deselect (pull CS high)
 */
static inline void _w5500_cs_deselect(void) { gpio_put(W5500_PIN_CS, 1); }

/**
 * @brief SPI read single byte
 */
static uint8_t _w5500_spi_read(void) {
    uint8_t rx_data = 0;
    uint8_t tx_data = 0xFF;
    spi_read_blocking(W5500_SPI, tx_data, &rx_data, 1);
    return rx_data;
}

/**
 * @brief SPI write single byte
 */
static void _w5500_spi_write(uint8_t tx_data) { spi_write_blocking(W5500_SPI, &tx_data, 1); }

/**
 * @brief Critical section enter (acquire mutex)
 */
static void _w5500_cris_enter(void) {
    if (w5500_spi_mutex) {
        xSemaphoreTake(w5500_spi_mutex, portMAX_DELAY);
    }
}

/**
 * @brief Critical section exit (release mutex)
 */
static void _w5500_cris_exit(void) {
    if (w5500_spi_mutex) {
        xSemaphoreGive(w5500_spi_mutex);
    }
}

/******************************************************************************
 *                          W5500 REGISTER ACCESS                             *
 ******************************************************************************/

uint8_t w5500_read_reg(uint32_t addr_sel) {
    uint8_t ret;
    uint8_t spi_data[3];

    _cris_enter();
    _cs_select();

    addr_sel |= (W5500_SPI_READ | W5500_SPI_VDM_OP);

    /* Send address + control phase */
    spi_data[0] = (addr_sel & 0x00FF0000) >> 16;
    spi_data[1] = (addr_sel & 0x0000FF00) >> 8;
    spi_data[2] = (addr_sel & 0x000000FF) >> 0;

    _spi_write_byte(spi_data[0]);
    _spi_write_byte(spi_data[1]);
    _spi_write_byte(spi_data[2]);

    /* Read data phase */
    ret = _spi_read_byte();

    _cs_deselect();
    _cris_exit();

    return ret;
}

void w5500_write_reg(uint32_t addr_sel, uint8_t wb) {
    uint8_t spi_data[4];

    _cris_enter();
    _cs_select();

    addr_sel |= (W5500_SPI_WRITE | W5500_SPI_VDM_OP);

    /* Send address + control phase */
    spi_data[0] = (addr_sel & 0x00FF0000) >> 16;
    spi_data[1] = (addr_sel & 0x0000FF00) >> 8;
    spi_data[2] = (addr_sel & 0x000000FF) >> 0;
    spi_data[3] = wb;

    _spi_write_byte(spi_data[0]);
    _spi_write_byte(spi_data[1]);
    _spi_write_byte(spi_data[2]);
    _spi_write_byte(spi_data[3]);

    _cs_deselect();
    _cris_exit();
}

void w5500_read_buf(uint32_t addr_sel, uint8_t *pBuf, uint16_t len) {
    uint8_t spi_data[3];

    if (!pBuf || len == 0)
        return;

    _cris_enter();
    _cs_select();

    addr_sel |= (W5500_SPI_READ | W5500_SPI_VDM_OP);

    /* Send address + control phase */
    spi_data[0] = (addr_sel & 0x00FF0000) >> 16;
    spi_data[1] = (addr_sel & 0x0000FF00) >> 8;
    spi_data[2] = (addr_sel & 0x000000FF) >> 0;

    _spi_write_byte(spi_data[0]);
    _spi_write_byte(spi_data[1]);
    _spi_write_byte(spi_data[2]);

    /* Read data phase */
    for (uint16_t i = 0; i < len; i++) {
        pBuf[i] = _spi_read_byte();
    }

    _cs_deselect();
    _cris_exit();
}

void w5500_write_buf(uint32_t addr_sel, uint8_t *pBuf, uint16_t len) {
    uint8_t spi_data[3];

    if (!pBuf || len == 0)
        return;

    _cris_enter();
    _cs_select();

    addr_sel |= (W5500_SPI_WRITE | W5500_SPI_VDM_OP);

    /* Send address + control phase */
    spi_data[0] = (addr_sel & 0x00FF0000) >> 16;
    spi_data[1] = (addr_sel & 0x0000FF00) >> 8;
    spi_data[2] = (addr_sel & 0x000000FF) >> 0;

    _spi_write_byte(spi_data[0]);
    _spi_write_byte(spi_data[1]);
    _spi_write_byte(spi_data[2]);

    /* Write data phase */
    for (uint16_t i = 0; i < len; i++) {
        _spi_write_byte(pBuf[i]);
    }

    _cs_deselect();
    _cris_exit();
}

/******************************************************************************
 *                          SOCKET DATA TRANSFER                              *
 ******************************************************************************/

void setSn_TX_WR(uint8_t sn, uint16_t ptr) {
    uint8_t be[2];
    be[0] = (uint8_t)(ptr >> 8);
    be[1] = (uint8_t)(ptr & 0xFF);
    w5500_write_buf(Sn_TX_WR(sn), be, 2);
}

void setSn_RX_RD(uint8_t sn, uint16_t ptr) {
    uint8_t be[2];
    be[0] = (uint8_t)(ptr >> 8);
    be[1] = (uint8_t)(ptr & 0xFF);
    w5500_write_buf(Sn_RX_RD(sn), be, 2);
}

void setSn_PORT(uint8_t sn, uint16_t port) {
    uint8_t be[2];
    be[0] = (uint8_t)(port >> 8);
    be[1] = (uint8_t)(port & 0xFF);
    w5500_write_buf(Sn_PORT(sn), be, 2);
}

uint16_t getSn_PORT(uint8_t sn) {
    uint8_t be[2];
    w5500_read_buf(Sn_PORT(sn), be, 2);
    return (uint16_t)((be[0] << 8) | be[1]);
}

void setSn_DPORT(uint8_t sn, uint16_t dport) {
    uint8_t be[2];
    be[0] = (uint8_t)(dport >> 8);
    be[1] = (uint8_t)(dport & 0xFF);
    w5500_write_buf(Sn_DPORT(sn), be, 2);
}

uint16_t getSn_DPORT(uint8_t sn) {
    uint8_t be[2];
    w5500_read_buf(Sn_DPORT(sn), be, 2);
    return (uint16_t)((be[0] << 8) | be[1]);
}

uint16_t w5500_get_tx_fsr(uint8_t sn) {
    uint16_t val = 0, val1 = 0;

    do {
        val1 = w5500_read_reg(Sn_TX_FSR(sn));
        val1 = (val1 << 8) + w5500_read_reg(W5500_OFFSET_INC(Sn_TX_FSR(sn), 1));
        if (val1 != 0) {
            val = w5500_read_reg(Sn_TX_FSR(sn));
            val = (val << 8) + w5500_read_reg(W5500_OFFSET_INC(Sn_TX_FSR(sn), 1));
        }
    } while (val != val1);

    return val;
}

uint16_t w5500_get_rx_rsr(uint8_t sn) {
    uint16_t val = 0, val1 = 0;

    do {
        val1 = w5500_read_reg(Sn_RX_RSR(sn));
        val1 = (val1 << 8) + w5500_read_reg(W5500_OFFSET_INC(Sn_RX_RSR(sn), 1));
        if (val1 != 0) {
            val = w5500_read_reg(Sn_RX_RSR(sn));
            val = (val << 8) + w5500_read_reg(W5500_OFFSET_INC(Sn_RX_RSR(sn), 1));
        }
    } while (val != val1);

    return val;
}

void eth_send_data(uint8_t sn, uint8_t *wizdata, uint16_t len) {
    if (!wizdata || len == 0)
        return;

    /* TX ring size in bytes (e.g. 8192 for 8KB) and modulo mask */
    uint16_t tx_max = getSn_TxMAX(sn);
    uint16_t tx_mask = (uint16_t)(tx_max - 1);

    /* Current hardware write pointer */
    uint16_t wr_ptr = getSn_TX_WR(sn);

    /* Offset inside the ring (0..tx_max-1) and bytes until ring end */
    uint16_t offset = (uint16_t)(wr_ptr & tx_mask);
    uint16_t tail_len = (uint16_t)(tx_max - offset);

    if (len <= tail_len) {
        /* Single contiguous burst: [offset .. offset+len) */
        uint32_t addr = ((uint32_t)offset << 8) | ((uint32_t)W5500_TXBUF_BLOCK(sn) << 3);
        w5500_write_buf(addr, wizdata, len);
    } else {
        /* Split across ring end: write tail, then wrap to 0 for head */
        uint16_t first = tail_len;
        uint16_t second = (uint16_t)(len - first);

        uint32_t addr_tail = ((uint32_t)offset << 8) | ((uint32_t)W5500_TXBUF_BLOCK(sn) << 3);
        w5500_write_buf(addr_tail, wizdata, first);

        uint32_t addr_head = ((uint32_t)0 << 8) | ((uint32_t)W5500_TXBUF_BLOCK(sn) << 3);
        w5500_write_buf(addr_head, wizdata + first, second);
    }

    /* Advance WR by full len (hardware wraps modulo tx_max) */
    wr_ptr = (uint16_t)(wr_ptr + len);
    setSn_TX_WR(sn, wr_ptr);
}

void eth_recv_data(uint8_t sn, uint8_t *wizdata, uint16_t len) {
    if (!wizdata || len == 0)
        return;

    uint16_t rx_max = getSn_RxMAX(sn);
    uint16_t rx_mask = (uint16_t)(rx_max - 1);

    uint16_t rd_ptr = getSn_RX_RD(sn);
    uint16_t offset = (uint16_t)(rd_ptr & rx_mask);
    uint16_t tail_len = (uint16_t)(rx_max - offset);

    if (len <= tail_len) {
        uint32_t addr = ((uint32_t)offset << 8) | ((uint32_t)W5500_RXBUF_BLOCK(sn) << 3);
        w5500_read_buf(addr, wizdata, len);
    } else {
        uint16_t first = tail_len;
        uint16_t second = (uint16_t)(len - first);

        uint32_t addr_tail = ((uint32_t)offset << 8) | ((uint32_t)W5500_RXBUF_BLOCK(sn) << 3);
        w5500_read_buf(addr_tail, wizdata, first);

        uint32_t addr_head = ((uint32_t)0 << 8) | ((uint32_t)W5500_RXBUF_BLOCK(sn) << 3);
        w5500_read_buf(addr_head, wizdata + first, second);
    }

    rd_ptr = (uint16_t)(rd_ptr + len);
    setSn_RX_RD(sn, rd_ptr);
}

void eth_recv_ignore(uint8_t sn, uint16_t len) {
    if (len == 0)
        return;
    uint16_t rd_ptr = getSn_RX_RD(sn);
    rd_ptr = (uint16_t)(rd_ptr + len);
    setSn_RX_RD(sn, rd_ptr);
}

/******************************************************************************
 *                          HARDWARE INITIALIZATION                           *
 ******************************************************************************/

bool w5500_hw_init(void) {
    /* Create SPI mutex */
    if (!w5500_spi_mutex) {
        w5500_spi_mutex = xSemaphoreCreateMutex();
        if (!w5500_spi_mutex) {
            ERROR_PRINT("[W5500] Failed to create SPI mutex\r\n");
            return false;
        }
    }

    /* Initialize SPI peripheral (already done in system_startup.c) */
    /* SPI0 @ 40MHz, CPOL=0, CPHA=0, MSB-first */

    /* Initialize CS pin (output, high = deselected) */
    gpio_init(W5500_PIN_CS);
    gpio_set_dir(W5500_PIN_CS, GPIO_OUT);
    gpio_put(W5500_PIN_CS, 1);

    /* Hardware reset sequence */
    gpio_init(W5500_PIN_RST);
    gpio_set_dir(W5500_PIN_RST, GPIO_OUT);
    gpio_put(W5500_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100)); /* Hold reset for 100ms */
    gpio_put(W5500_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); /* Wait for chip to boot */

    /* Register callback functions */
    _cris_enter = _w5500_cris_enter;
    _cris_exit = _w5500_cris_exit;
    _cs_select = _w5500_cs_select;
    _cs_deselect = _w5500_cs_deselect;
    _spi_read_byte = _w5500_spi_read;
    _spi_write_byte = _w5500_spi_write;

    ETH_LOG("[ETH] Hardware initialized (SPI @ %lu MHz)\r\n", W5500_SPI_CLOCK / 1000000);
    return true;
}

bool w5500_chip_init(w5500_NetConfig *net_info) {
    if (!net_info) {
        ERROR_PRINT("[W5500] NULL network info\r\n");
        return false;
    }

    /* Configure socket memory allocation */
    setSn_RXBUF_SIZE(0, W5500_RX_SIZE_S0);
    setSn_TXBUF_SIZE(0, W5500_TX_SIZE_S0);
    setSn_RXBUF_SIZE(1, W5500_RX_SIZE_S1);
    setSn_TXBUF_SIZE(1, W5500_TX_SIZE_S1);
    setSn_RXBUF_SIZE(2, W5500_RX_SIZE_S2);
    setSn_TXBUF_SIZE(2, W5500_TX_SIZE_S2);
    setSn_RXBUF_SIZE(3, W5500_RX_SIZE_S3);
    setSn_TXBUF_SIZE(3, W5500_TX_SIZE_S3);
    setSn_RXBUF_SIZE(4, W5500_RX_SIZE_S4);
    setSn_TXBUF_SIZE(4, W5500_TX_SIZE_S4);
    setSn_RXBUF_SIZE(5, W5500_RX_SIZE_S5);
    setSn_TXBUF_SIZE(5, W5500_TX_SIZE_S5);
    setSn_RXBUF_SIZE(6, W5500_RX_SIZE_S6);
    setSn_TXBUF_SIZE(6, W5500_TX_SIZE_S6);
    setSn_RXBUF_SIZE(7, W5500_RX_SIZE_S7);
    setSn_TXBUF_SIZE(7, W5500_TX_SIZE_S7);

    /* Configure PHY */
    setPHYCFGR(PHYCFGR_OPMD | PHYCFGR_OPMDC_ALLA | PHYCFGR_RST);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Wait for link up */
    uint8_t link_status;
    uint32_t timeout = 0;
    do {
        link_status = getPHYCFGR();
        vTaskDelay(pdMS_TO_TICKS(100));
        timeout++;
        if (timeout > 50) { /* 5 second timeout */
            ERROR_PRINT("[W5500] PHY link timeout\r\n");
            return false;
        }
    } while ((link_status & PHYCFGR_LNK_ON) == 0);

    ETH_LOG("[ETH] PHY link established\r\n");

    /* Configure network parameters */
    w5500_set_network(net_info);

    /* Program TCP retry behavior from configuration */
    w5500_write_reg(_RTR_, (uint8_t)(W5500_RETRY_TIME >> 8));
    w5500_write_reg(W5500_OFFSET_INC(_RTR_, 1), (uint8_t)(W5500_RETRY_TIME & 0xFF));
    w5500_write_reg(_RCR_, (uint8_t)W5500_RETRY_COUNT);

    ETH_LOG("[ETH] Chip initialized\r\n");
    return true;
}

bool w5500_check_version(void) {
    uint8_t version = getVERSIONR();
    if (version != 0x04) {
        ERROR_PRINT("[W5500] Invalid version: 0x%02X (expected 0x04)\r\n", version);
        return false;
    }
    ETH_LOG("[ETH] Version check OK: 0x%02X\r\n", version);
    return true;
}

/******************************************************************************
 *                          NETWORK CONFIGURATION                             *
 ******************************************************************************/

void w5500_set_network(w5500_NetConfig *net_info) {
    if (!net_info)
        return;

    /* Set MAC address */
    setSHAR(net_info->mac);

    /* Set IP address */
    setSIPR(net_info->ip);

    /* Set subnet mask */
    setSUBR(net_info->sn);

    /* Set gateway */
    setGAR(net_info->gw);
}

void w5500_get_network(w5500_NetConfig *net_info) {
    if (!net_info)
        return;

    /* Get MAC address */
    getSHAR(net_info->mac);

    /* Get IP address */
    getSIPR(net_info->ip);

    /* Get subnet mask */
    getSUBR(net_info->sn);

    /* Get gateway */
    getGAR(net_info->gw);

    /* Fill in other fields */
    memcpy(net_info->dns, W5500_DEFAULT_DNS, 4);
    net_info->dhcp = W5500_USE_DHCP;
}

void w5500_print_network(w5500_NetConfig *net_info) {
    if (!net_info)
        return;

    ETH_LOG("[ETH] Network Configuration:\r\n");
    ETH_LOG("[ETH]  MAC  : %02X:%02X:%02X:%02X:%02X:%02X\r\n", net_info->mac[0], net_info->mac[1],
            net_info->mac[2], net_info->mac[3], net_info->mac[4], net_info->mac[5]);
    ETH_LOG("[ETH]  IP   : %d.%d.%d.%d\r\n", net_info->ip[0], net_info->ip[1], net_info->ip[2],
            net_info->ip[3]);
    ETH_LOG("[ETH]  Mask : %d.%d.%d.%d\r\n", net_info->sn[0], net_info->sn[1], net_info->sn[2],
            net_info->sn[3]);
    ETH_LOG("[ETH]  GW   : %d.%d.%d.%d\r\n", net_info->gw[0], net_info->gw[1], net_info->gw[2],
            net_info->gw[3]);
    ETH_LOG("[ETH]  DNS  : %d.%d.%d.%d\r\n", net_info->dns[0], net_info->dns[1], net_info->dns[2],
            net_info->dns[3]);
    ETH_LOG("[ETH]  DHCP : %s\r\n", net_info->dhcp ? "Enabled" : "Disabled");
}

/******************************************************************************
 *                          PHY MANAGEMENT                                    *
 ******************************************************************************/

w5500_PhyLink w5500_get_link_status(void) {
    uint8_t phycfgr = getPHYCFGR();
    return (phycfgr & PHYCFGR_LNK_ON) ? PHY_LINK_ON : PHY_LINK_OFF;
}

int8_t w5500_set_phy_conf(w5500_PhyConfig *phyconf) {
    if (!phyconf)
        return -1;

    uint8_t tmp = 0;

    if (phyconf->by == PHY_CONFBY_SW) {
        tmp = (phyconf->mode & 0x07) << 3;
        setPHYCFGR(tmp);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    return 0;
}

void w5500_get_phy_conf(w5500_PhyConfig *phyconf) {
    if (!phyconf)
        return;

    uint8_t tmp = getPHYCFGR();

    phyconf->by = PHY_CONFBY_SW;
    phyconf->mode = (tmp >> 3) & 0x07;
    phyconf->speed = (tmp & PHYCFGR_SPD_100) ? PHY_SPEED_100 : PHY_SPEED_10;
    phyconf->duplex = (tmp & PHYCFGR_DPX_FULL) ? PHY_DUPLEX_FULL : PHY_DUPLEX_HALF;
}

void w5500_set_phy_power(w5500_PhyPower mode) {
    uint8_t tmp = getPHYCFGR();

    if (mode == PHY_POWER_DOWN) {
        tmp |= PHYCFGR_OPMDC_PDOWN;
    } else {
        tmp &= ~PHYCFGR_OPMDC_PDOWN;
    }

    setPHYCFGR(tmp);
}

/******************************************************************************
 *                          UTILITY FUNCTIONS                                 *
 ******************************************************************************/

void w5500_sw_reset(void) {
    setMR(MR_RST);
    vTaskDelay(pdMS_TO_TICKS(10));
    while (getMR() & MR_RST) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

const char *w5500_get_chip_id(void) { return "W5500"; }
