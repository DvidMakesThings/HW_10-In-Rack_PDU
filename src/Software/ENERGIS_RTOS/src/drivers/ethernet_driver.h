/**
 * @file ethernet_driver.h
 * @author DvidMakesThings - David Sipos
 * @brief W5500 Ethernet Controller HAL Driver (RTOS-compatible)
 *
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * Unified W5500 driver combining:
 * - Hardware abstraction layer (register access)
 * - SPI interface with FreeRTOS mutex protection
 * - PHY configuration and link monitoring
 * - Network initialization
 *
 * This driver is thread-safe and designed for FreeRTOS.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef W5500_DRIVER_H
#define W5500_DRIVER_H

// #include "ethernet_config.h"
// #include "ethernet_w5500regs.h"
#include "../CONFIG.h"

/******************************************************************************
 *                          TYPE DEFINITIONS                                  *
 ******************************************************************************/

/**
 * @brief Network information structure
 */
typedef struct {
    uint8_t mac[6]; /**< MAC address */
    uint8_t ip[4];  /**< IP address */
    uint8_t sn[4];  /**< Subnet mask */
    uint8_t gw[4];  /**< Gateway address */
    uint8_t dns[4]; /**< DNS server address */
    uint8_t dhcp;   /**< DHCP mode (0=static, 1=DHCP) */
} w5500_NetConfig;

/**
 * @brief PHY configuration structure
 */
typedef struct {
    uint8_t by;     /**< PHY mode selector (PHY_CONFBY_HW/SW) */
    uint8_t mode;   /**< PHY operation mode */
    uint8_t speed;  /**< Link speed (PHY_SPEED_10/100) */
    uint8_t duplex; /**< Duplex mode (PHY_DUPLEX_HALF/FULL) */
} w5500_PhyConfig;

/**
 * @brief W5500 link status
 */
typedef enum {
    PHY_LINK_OFF = 0, /**< Link down */
    PHY_LINK_ON = 1   /**< Link up */
} w5500_PhyLink;

/**
 * @brief W5500 power mode
 */
typedef enum {
    PHY_POWER_NORM = 0, /**< Normal mode */
    PHY_POWER_DOWN = 1  /**< Power down mode */
} w5500_PhyPower;

/******************************************************************************
 *                          GLOBAL VARIABLES                                  *
 ******************************************************************************/

/**
 * @brief SPI mutex for thread-safe W5500 access
 * @note All W5500 register access MUST acquire this mutex
 */
extern SemaphoreHandle_t w5500_spi_mutex;

/******************************************************************************
 *                          REGISTER ACCESS (LOW-LEVEL)                       *
 ******************************************************************************/

/**
 * @brief Read single byte from W5500 register (thread-safe)
 *
 * @param addr_sel 32-bit address selector (includes BSB)
 * @return Register value
 * @note Automatically acquires SPI mutex
 */
uint8_t w5500_read_reg(uint32_t addr_sel);

/**
 * @brief Write single byte to W5500 register (thread-safe)
 *
 * @param addr_sel 32-bit address selector (includes BSB)
 * @param data Byte to write
 * @note Automatically acquires SPI mutex
 */
void w5500_write_reg(uint32_t addr_sel, uint8_t data);

/**
 * @brief Read buffer from W5500 (thread-safe)
 *
 * @param addr_sel 32-bit address selector (includes BSB)
 * @param buf Pointer to destination buffer
 * @param len Number of bytes to read
 * @note Automatically acquires SPI mutex
 */
void w5500_read_buf(uint32_t addr_sel, uint8_t *buf, uint16_t len);

/**
 * @brief Write buffer to W5500 (thread-safe)
 *
 * @param addr_sel 32-bit address selector (includes BSB)
 * @param buf Pointer to source buffer
 * @param len Number of bytes to write
 * @note Automatically acquires SPI mutex
 */
void w5500_write_buf(uint32_t addr_sel, uint8_t *buf, uint16_t len);

/******************************************************************************
 *                          COMMON REGISTER INLINE ACCESS                     *
 ******************************************************************************/

/* Common register getters/setters */
#define setMR(mr) w5500_write_reg(MR, (mr))
#define getMR() w5500_read_reg(MR)
#define setGAR(gar) w5500_write_buf(GAR, (gar), 4)
#define getGAR(gar) w5500_read_buf(GAR, (gar), 4)
#define setSUBR(subr) w5500_write_buf(SUBR, (subr), 4)
#define getSUBR(subr) w5500_read_buf(SUBR, (subr), 4)
#define setSHAR(shar) w5500_write_buf(SHAR, (shar), 6)
#define getSHAR(shar) w5500_read_buf(SHAR, (shar), 6)
#define setSIPR(sipr) w5500_write_buf(SIPR, (sipr), 4)
#define getSIPR(sipr) w5500_read_buf(SIPR, (sipr), 4)
#define setIR(ir) w5500_write_reg(IR, ((ir) & 0xF0))
#define getIR() (w5500_read_reg(IR) & 0xF0)
#define setPHYCFGR(phycfgr) w5500_write_reg(PHYCFGR, (phycfgr))
#define getPHYCFGR() w5500_read_reg(PHYCFGR)
#define getVERSIONR() w5500_read_reg(VERSIONR)

/* Socket register getters/setters */
#define setSn_MR(sn, mr) w5500_write_reg(Sn_MR(sn), (mr))
#define getSn_MR(sn) w5500_read_reg(Sn_MR(sn))
#define setSn_CR(sn, cr) w5500_write_reg(Sn_CR(sn), (cr))
#define getSn_CR(sn) w5500_read_reg(Sn_CR(sn))
#define setSn_IR(sn, ir) w5500_write_reg(Sn_IR(sn), (ir))
#define getSn_IR(sn) w5500_read_reg(Sn_IR(sn))
#define setSn_IMR(sn, imr) w5500_write_reg(Sn_IMR(sn), (imr))
#define getSn_IMR(sn) w5500_read_reg(Sn_IMR(sn))
#define getSn_SR(sn) w5500_read_reg(Sn_SR(sn))

/**
 * @brief Set the local TCP/UDP port (big-endian)
 * @param sn   Socket number [0..7]
 * @param port Host-endian port number (e.g., 80)
 */
void setSn_PORT(uint8_t sn, uint16_t port);

/**
 * @brief Get the local TCP/UDP port (big-endian)
 * @param sn   Socket number [0..7]
 * @return     Host-endian port number
 */
uint16_t getSn_PORT(uint8_t sn);

#define setSn_DIPR(sn, dipr) w5500_write_buf(Sn_DIPR(sn), (dipr), 4)
#define getSn_DIPR(sn, dipr) w5500_read_buf(Sn_DIPR(sn), (dipr), 4)

/**
 * @brief Set the destination TCP/UDP port (big-endian)
 * @param sn   Socket number [0..7]
 * @param dport Host-endian destination port
 */
void setSn_DPORT(uint8_t sn, uint16_t dport);

/**
 * @brief Get the destination TCP/UDP port (big-endian)
 * @param sn   Socket number [0..7]
 * @return     Host-endian destination port
 */
uint16_t getSn_DPORT(uint8_t sn);

#define setSn_RXBUF_SIZE(sn, size) w5500_write_reg(Sn_RXBUF_SIZE(sn), (size))
#define getSn_RXBUF_SIZE(sn) w5500_read_reg(Sn_RXBUF_SIZE(sn))
#define setSn_TXBUF_SIZE(sn, size) w5500_write_reg(Sn_TXBUF_SIZE(sn), (size))
#define getSn_TXBUF_SIZE(sn) w5500_read_reg(Sn_TXBUF_SIZE(sn))
#define getSn_TX_FSR(sn) w5500_get_tx_fsr(sn)
#define getSn_RX_RSR(sn) w5500_get_rx_rsr(sn)

/**
 * @brief Set socket TX write pointer (Sn_TX_WR) in big-endian.
 *
 * @param sn   Socket number [0..7]
 * @param ptr  New TX write pointer
 */
void setSn_TX_WR(uint8_t sn, uint16_t ptr);

#define getSn_TX_WR(sn)                                                                            \
    ((w5500_read_reg(Sn_TX_WR(sn)) << 8) + w5500_read_reg(W5500_OFFSET_INC(Sn_TX_WR(sn), 1)))

/**
 * @brief Set socket RX read pointer (Sn_RX_RD) in big-endian.
 *
 * @param sn   Socket number [0..7]
 * @param ptr  New RX read pointer
 */
void setSn_RX_RD(uint8_t sn, uint16_t ptr);

#define getSn_RX_RD(sn)                                                                            \
    ((w5500_read_reg(Sn_RX_RD(sn)) << 8) + w5500_read_reg(W5500_OFFSET_INC(Sn_RX_RD(sn), 1)))
#define setSn_TTL(sn, ttl) w5500_write_reg(Sn_TTL(sn), (ttl))
#define getSn_TTL(sn) w5500_read_reg(Sn_TTL(sn))
#define setSn_TOS(sn, tos) w5500_write_reg(Sn_TOS(sn), (tos))
#define getSn_TOS(sn) w5500_read_reg(Sn_TOS(sn))
#define setSn_MSSR(sn, mss)                                                                        \
    {                                                                                              \
        w5500_write_reg(Sn_MSSR(sn), (uint8_t)((mss) >> 8));                                       \
        w5500_write_reg(W5500_OFFSET_INC(Sn_MSSR(sn), 1), (uint8_t)(mss));                         \
    }
#define getSn_MSSR(sn)                                                                             \
    (((uint16_t)w5500_read_reg(Sn_MSSR(sn)) << 8) +                                                \
     w5500_read_reg(W5500_OFFSET_INC(Sn_MSSR(sn), 1)))
#define setSn_KPALVTR(sn, kpalvtr) w5500_write_reg(Sn_KPALVTR(sn), (kpalvtr))
#define getSn_KPALVTR(sn) w5500_read_reg(Sn_KPALVTR(sn))

/* Helper functions for socket buffer size (max TX/RX) */
#define getSn_TxMAX(sn) (getSn_TXBUF_SIZE(sn) << 10)
#define getSn_RxMAX(sn) (getSn_RXBUF_SIZE(sn) << 10)

/******************************************************************************
 *                          SOCKET DATA TRANSFER                              *
 ******************************************************************************/

/**
 * @brief Send data to socket TX buffer
 * @param sn Socket number
 * @param wizdata Data buffer
 * @param len Data length
 */
void eth_send_data(uint8_t sn, uint8_t *wizdata, uint16_t len);

/**
 * @brief Receive data from socket RX buffer
 * @param sn Socket number
 * @param wizdata Data buffer
 * @param len Data length
 */
void eth_recv_data(uint8_t sn, uint8_t *wizdata, uint16_t len);

/**
 * @brief Ignore (discard) received data
 * @param sn Socket number
 * @param len Length to discard
 */
void eth_recv_ignore(uint8_t sn, uint16_t len);

/**
 * @brief Get socket TX free size (with double-read protection)
 * @param sn Socket number
 * @return Free size in TX buffer
 */
uint16_t w5500_get_tx_fsr(uint8_t sn);

/**
 * @brief Get socket RX received size (with double-read protection)
 * @param sn Socket number
 * @return Received size in RX buffer
 */
uint16_t w5500_get_rx_rsr(uint8_t sn);

/******************************************************************************
 *                          INITIALIZATION                                    *
 ******************************************************************************/

/**
 * @brief Initialize W5500 hardware (SPI, GPIOs, chip reset)
 *
 * Call this ONCE during system initialization, before scheduler starts.
 * Sets up:
 * - SPI peripheral and GPIO pins
 * - Critical section (mutex) for SPI access
 * - Hardware reset sequence
 * - Register callback functions
 *
 * @return true on success, false on failure
 */
bool w5500_hw_init(void);

/**
 * @brief Initialize W5500 chip (memory, PHY, network)
 *
 * Call this AFTER w5500_hw_init() and AFTER scheduler starts.
 * Configures:
 * - Socket memory allocation
 * - PHY auto-negotiation
 * - Network parameters (MAC, IP, etc.)
 *
 * @param net_info Pointer to network configuration structure
 * @return true on success, false on failure
 */
bool w5500_chip_init(w5500_NetConfig *net_info);

/**
 * @brief Check W5500 version register (diagnostic)
 *
 * @return true if version register reads 0x04 (W5500), false otherwise
 */
bool w5500_check_version(void);

/******************************************************************************
 *                          NETWORK CONFIGURATION                             *
 ******************************************************************************/

/**
 * @brief Set network configuration (MAC, IP, subnet, gateway)
 *
 * @param net_info Pointer to network configuration structure
 */
void w5500_set_network(w5500_NetConfig *net_info);

/**
 * @brief Get current network configuration
 *
 * @param net_info Pointer to structure to fill with current config
 */
void w5500_get_network(w5500_NetConfig *net_info);

/**
 * @brief Print network configuration to logger
 *
 * @param net_info Pointer to network configuration structure
 */
void w5500_print_network(w5500_NetConfig *net_info);

/******************************************************************************
 *                          PHY MANAGEMENT                                    *
 ******************************************************************************/

/**
 * @brief Get PHY link status
 *
 * @return PHY_LINK_ON if link is up, PHY_LINK_OFF if link is down
 */
w5500_PhyLink w5500_get_link_status(void);

/**
 * @brief Configure PHY mode (auto-negotiation, speed, duplex)
 *
 * @param phyconf Pointer to PHY configuration structure
 * @return 0 on success, negative on error
 */
int8_t w5500_set_phy_conf(w5500_PhyConfig *phyconf);

/**
 * @brief Get current PHY configuration
 *
 * @param phyconf Pointer to structure to fill with PHY config
 */
void w5500_get_phy_conf(w5500_PhyConfig *phyconf);

/**
 * @brief Set PHY power mode
 *
 * @param mode PHY_POWER_NORM or PHY_POWER_DOWN
 */
void w5500_set_phy_power(w5500_PhyPower mode);

/******************************************************************************
 *                          UTILITY FUNCTIONS                                 *
 ******************************************************************************/

/**
 * @brief Software reset W5500 chip
 */
void w5500_sw_reset(void);

/**
 * @brief Get chip ID string
 *
 * @return Pointer to chip ID string ("W5500")
 */
const char *w5500_get_chip_id(void);

#endif /* W5500_DRIVER_H */