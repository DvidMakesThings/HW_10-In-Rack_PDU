/**
 * @file ethernet_config.h
 * @author DvidMakesThings - David Sipos
 * @brief W5500 Ethernet Controller Configuration
 *
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * Centralized configuration for W5500 Ethernet controller.
 * This file provides flexible, easy-to-use configuration for:
 * - Hardware pins and SPI settings
 * - Socket memory allocation
 * - Network parameters
 * - Protocol support (TCP, UDP, SNMP, etc.)
 * - RTOS integration
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef W5500_CONFIG_H
#define W5500_CONFIG_H

#include "../CONFIG.h"

/******************************************************************************
 *                          HARDWARE CONFIGURATION                            *
 ******************************************************************************/

/**
 * @defgroup W5500_HW Hardware Configuration
 * @brief SPI and GPIO pin assignments
 * @{
 */

/** SPI peripheral instance (from CONFIG.h) */
#define W5500_SPI W5500_SPI_INSTANCE

/** SPI clock speed (Hz) - from CONFIG.h */
#define W5500_SPI_CLOCK SPI_SPEED_W5500

/** GPIO Pin Assignments (from CONFIG.h) */
#define W5500_PIN_MOSI W5500_MOSI
#define W5500_PIN_MISO W5500_MISO
#define W5500_PIN_SCK W5500_SCK
#define W5500_PIN_CS W5500_CS
#define W5500_PIN_RST W5500_RESET
#define W5500_PIN_INT W5500_INT

/** @} */ // End of W5500_HW

/******************************************************************************
 *                      SOCKET MEMORY CONFIGURATION                           *
 ******************************************************************************/

/**
 * @defgroup W5500_MEM Memory Allocation
 * @brief Socket TX/RX buffer sizes (total 16KB per direction)
 * @{
 */

/**
 * Socket Buffer Allocation Strategy:
 * - Socket 0: 8KB TX / 8KB RX (Primary TCP server/client)
 * - Socket 1: 4KB TX / 4KB RX (Secondary TCP or UDP)
 * - Socket 2: 4KB TX / 4KB RX (SNMP or additional service)
 * - Socket 3-7: Unused (0KB)
 *
 * Total: 16KB TX / 16KB RX (W5500 hardware limit)
 */
#define W5500_SOCKET_COUNT 8

/** TX buffer sizes per socket (KB) - must sum to 16KB */
#define W5500_TX_SIZE_S0 8 /**< Socket 0: Primary (TCP server) */
#define W5500_TX_SIZE_S1 4 /**< Socket 1: SNMP/Additional */
#define W5500_TX_SIZE_S2 4 /**< Socket 2: SNMP/Additional */
#define W5500_TX_SIZE_S3 0 /**< Socket 3: Unused */
#define W5500_TX_SIZE_S4 0 /**< Socket 4: Unused */
#define W5500_TX_SIZE_S5 0 /**< Socket 5: Unused */
#define W5500_TX_SIZE_S6 0 /**< Socket 6: Unused */
#define W5500_TX_SIZE_S7 0 /**< Socket 7: Unused */

/** RX buffer sizes per socket (KB) - must sum to 16KB */
#define W5500_RX_SIZE_S0 8 /**< Socket 0: Primary (TCP server) */
#define W5500_RX_SIZE_S1 4 /**< Socket 1: SNMP/Additional */
#define W5500_RX_SIZE_S2 4 /**< Socket 2: SNMP/Additional */
#define W5500_RX_SIZE_S3 0 /**< Socket 3: Unused */
#define W5500_RX_SIZE_S4 0 /**< Socket 4: Unused */
#define W5500_RX_SIZE_S5 0 /**< Socket 5: Unused */
#define W5500_RX_SIZE_S6 0 /**< Socket 6: Unused */
#define W5500_RX_SIZE_S7 0 /**< Socket 7: Unused */

/** @} */ // End of W5500_MEM

/******************************************************************************
 *                      NETWORK CONFIGURATION                                 *
 ******************************************************************************/

/**
 * @defgroup W5500_NET Network Settings
 * @brief Default network parameters (can be changed at runtime)
 * @{
 */

/** Default MAC address (Locally Administered Address) */
#define W5500_DEFAULT_MAC {0x02, 0x00, 0x00, 0x12, 0x34, 0x56}

/** Default IP address (192.168.0.60) */
#define W5500_DEFAULT_IP {192, 168, 0, 12}

/** Default Subnet mask (255.255.255.0) */
#define W5500_DEFAULT_SUBNET {255, 255, 255, 0}

/** Default Gateway (192.168.0.1) */
#define W5500_DEFAULT_GATEWAY {192, 168, 0, 1}

/** Default DNS server (8.8.8.8) */
static const uint8_t W5500_DEFAULT_DNS[4] = {8, 8, 8, 8};

/** Use DHCP to obtain IP address (0=static, 1=DHCP) */
#define W5500_USE_DHCP 0

/** @} */ // End of W5500_NET

/******************************************************************************
 *                      PROTOCOL CONFIGURATION                                *
 ******************************************************************************/

/**
 * @defgroup W5500_PROTO Protocol Support
 * @brief Enable/disable protocol features
 * @{
 */

/** Enable TCP protocol support */
#define W5500_ENABLE_TCP 1

/** Enable UDP protocol support */
#define W5500_ENABLE_UDP 1

/** Enable SNMP protocol support */
#define W5500_ENABLE_SNMP 1

/** Enable ICMP (ping) support */
#define W5500_ENABLE_ICMP 1

/** Enable IPv6 support (future) */
#define W5500_ENABLE_IPV6 0

/** @} */ // End of W5500_PROTO

/******************************************************************************
 *                      RTOS CONFIGURATION                                    *
 ******************************************************************************/

/**
 * @defgroup W5500_RTOS RTOS Integration
 * @brief FreeRTOS task and synchronization settings
 * @{
 */

/** Network task priority (higher = more priority) */
#define W5500_TASK_PRIORITY 3

/** Network task stack size (bytes) */
#define W5500_TASK_STACK 2048

/** Network event queue length */
#define W5500_QUEUE_LENGTH 16

/** SPI access timeout (ticks) - for mutex acquisition */
#define W5500_SPI_TIMEOUT pdMS_TO_TICKS(100)

/** Socket operation timeout (ticks) */
#define W5500_SOCKET_TIMEOUT pdMS_TO_TICKS(5000)

/** PHY link check interval (ms) */
#define W5500_PHY_CHECK_MS 1000

/** Use interrupts for socket events (0=polling, 1=interrupt) */
#define W5500_USE_INTERRUPTS 1

/** Interrupt poll interval when using interrupts (ms) */
#define W5500_IRQ_POLL_MS 10

/** @} */ // End of W5500_RTOS

/******************************************************************************
 *                      ADVANCED SETTINGS                                     *
 ******************************************************************************/

/**
 * @defgroup W5500_ADV Advanced Settings
 * @brief Expert-level configuration
 * @{
 */

/** Retry time value (RTR) - timeout for TCP retransmission (ms) */
#define W5500_RETRY_TIME 2000

/** Retry count (RCR) - number of retransmission attempts */
#define W5500_RETRY_COUNT 8

/** TCP Maximum Segment Size (MSS) */
#define W5500_TCP_MSS 1460

/** TCP Keep-Alive Time (seconds, 0=disabled) */
#define W5500_KEEPALIVE_TIME 120

/** Enable TCP_NODELAY (disable Nagle's algorithm) */
#define W5500_TCP_NODELAY 0

/** PHY configuration: Auto-negotiation */
#define W5500_PHY_AUTONEG 1

/** PHY configuration: Force speed (0=auto, 10=10Mbps, 100=100Mbps) */
#define W5500_PHY_SPEED 0

/** PHY configuration: Force duplex (0=auto, 1=half, 2=full) */
#define W5500_PHY_DUPLEX 0

/** @} */ // End of W5500_ADV

/******************************************************************************
 *                      DEBUG AND LOGGING                                     *
 ******************************************************************************/

/**
 * @defgroup W5500_DBG Debug Configuration
 * @brief Debugging and logging settings
 * @{
 */

/** Enable W5500 driver debug output */
#define W5500_DEBUG DEBUG

/** Enable socket operation debug */
#define W5500_DEBUG_SOCKET 0

/** Enable SPI transaction debug */
#define W5500_DEBUG_SPI 1

/** Enable network event logging */
#define W5500_LOG_EVENTS 1

/** Debug macros (use CONFIG.h logging) */
#if W5500_DEBUG
#define W5500_DBG(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define W5500_DBG(...) ((void)0)
#endif

#if W5500_DEBUG_SOCKET
#define W5500_SOCK_DBG(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define W5500_SOCK_DBG(...) ((void)0)
#endif

#if W5500_DEBUG_SPI
#define W5500_SPI_DBG(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define W5500_SPI_DBG(...) ((void)0)
#endif

#if W5500_LOG_EVENTS
#define ETH_LOG(...) INFO_PRINT(__VA_ARGS__)
#else
#define ETH_LOG(...) ((void)0)
#endif

/** @} */ // End of W5500_DBG

/******************************************************************************
 *                      COMPILE-TIME VALIDATION                               *
 ******************************************************************************/

/* Validate socket buffer allocation */
#define _W5500_TX_TOTAL                                                                            \
    (W5500_TX_SIZE_S0 + W5500_TX_SIZE_S1 + W5500_TX_SIZE_S2 + W5500_TX_SIZE_S3 +                   \
     W5500_TX_SIZE_S4 + W5500_TX_SIZE_S5 + W5500_TX_SIZE_S6 + W5500_TX_SIZE_S7)

#define _W5500_RX_TOTAL                                                                            \
    (W5500_RX_SIZE_S0 + W5500_RX_SIZE_S1 + W5500_RX_SIZE_S2 + W5500_RX_SIZE_S3 +                   \
     W5500_RX_SIZE_S4 + W5500_RX_SIZE_S5 + W5500_RX_SIZE_S6 + W5500_RX_SIZE_S7)

#if _W5500_TX_TOTAL > 16
#error "W5500: Total TX buffer size exceeds 16KB limit!"
#endif

#if _W5500_RX_TOTAL > 16
#error "W5500: Total RX buffer size exceeds 16KB limit!"
#endif

#endif /* W5500_CONFIG_H */
