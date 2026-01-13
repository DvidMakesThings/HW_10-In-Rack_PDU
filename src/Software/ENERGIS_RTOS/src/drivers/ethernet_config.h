/**
 * @file drivers/ethernet_config.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup config02 2. W5500 Ethernet Configuration
 * @ingroup config
 * @brief Centralized configuration parameters for W5500 Ethernet controller.
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * This file centralizes all configuration for the W5500 Ethernet controller used in
 * the ENERGIS PDU. Configuration includes:
 * - Hardware pin assignments and SPI settings
 * - Socket memory buffer allocation (16KB TX + 16KB RX total)
 * - Network parameters (IP, MAC, gateway, subnet)
 * - Protocol support flags (TCP, UDP, SNMP, ICMP)
 * - RTOS task and synchronization parameters
 * - Advanced TCP/IP tuning parameters
 *
 * Socket Buffer Strategy:
 * The W5500 provides 16KB each for TX and RX buffers, distributed across 8 sockets.
 * Current allocation prioritizes HTTP server (Socket 0) and SNMP (Socket 1).
 *
 * Configuration Philosophy:
 * - All hardware-specific values come from CONFIG.h
 * - Runtime-changeable parameters (IP, MAC) have defaults
 * - Compile-time validation prevents buffer overallocation
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef W5500_CONFIG_H
#define W5500_CONFIG_H

#include "../CONFIG.h"

/**
 * @defgroup W5500_HW Hardware Pin Configuration
 * @brief SPI peripheral and GPIO pin assignments from CONFIG.h
 * @{
 */

/** @name Hardware Pins
 *  Constants for SPI and GPIO assignments.
 *  @{ */

/** SPI peripheral instance used for W5500 communication */
#define W5500_SPI W5500_SPI_INSTANCE

/** SPI clock frequency in Hz (typically 10-33 MHz) */
#define W5500_SPI_CLOCK SPI_SPEED_W5500

/** SPI MOSI (Master Out Slave In) GPIO pin */
#define W5500_PIN_MOSI W5500_MOSI

/** SPI MISO (Master In Slave Out) GPIO pin */
#define W5500_PIN_MISO W5500_MISO

/** SPI SCK (Serial Clock) GPIO pin */
#define W5500_PIN_SCK W5500_SCK

/** SPI CS (Chip Select, active low) GPIO pin */
#define W5500_PIN_CS W5500_CS

/** Hardware reset GPIO pin (active low) */
#define W5500_PIN_RST W5500_RESET

/** Interrupt GPIO pin (active low, optional) */
#define W5500_PIN_INT W5500_INT

/** @} */

/** @} */

/**
 * @defgroup W5500_MEM Socket Memory Allocation
 * @brief TX/RX buffer sizes for each socket (total 16KB per direction)
 * @{
 */

/** @name Socket Memory Sizes
 *  Per-socket TX/RX buffer allocations (KB).
 *  @{ */

/**
 * @brief Number of hardware sockets available
 */
#define W5500_SOCKET_COUNT 8

/** Socket 0 TX buffer size in KB (HTTP server, primary) */
#define W5500_TX_SIZE_S0 8

/** Socket 1 TX buffer size in KB (SNMP) */
#define W5500_TX_SIZE_S1 8

/** Socket 2 TX buffer size in KB (unused) */
#define W5500_TX_SIZE_S2 0

/** Socket 3 TX buffer size in KB (unused) */
#define W5500_TX_SIZE_S3 0

/** Socket 4 TX buffer size in KB (unused) */
#define W5500_TX_SIZE_S4 0

/** Socket 5 TX buffer size in KB (unused) */
#define W5500_TX_SIZE_S5 0

/** Socket 6 TX buffer size in KB (unused) */
#define W5500_TX_SIZE_S6 0

/** Socket 7 TX buffer size in KB (unused) */
#define W5500_TX_SIZE_S7 0

/** Socket 0 RX buffer size in KB (HTTP server, primary) */
#define W5500_RX_SIZE_S0 8

/** Socket 1 RX buffer size in KB (SNMP) */
#define W5500_RX_SIZE_S1 8

/** Socket 2 RX buffer size in KB (unused) */
#define W5500_RX_SIZE_S2 0

/** Socket 3 RX buffer size in KB (unused) */
#define W5500_RX_SIZE_S3 0

/** Socket 4 RX buffer size in KB (unused) */
#define W5500_RX_SIZE_S4 0

/** Socket 5 RX buffer size in KB (unused) */
#define W5500_RX_SIZE_S5 0

/** Socket 6 RX buffer size in KB (unused) */
#define W5500_RX_SIZE_S6 0

/** Socket 7 RX buffer size in KB (unused) */
#define W5500_RX_SIZE_S7 0

/** @} */

/** @} */

/**
 * @defgroup W5500_NET Network Configuration Defaults
 * @brief Default network parameters (overrideable at runtime)
 * @{
 */

/** @name Network Defaults
 *  Default MAC/IP/subnet/gateway/DNS settings.
 *  @{ */

/** Default MAC address (Locally Administered) */
#define W5500_DEFAULT_MAC {0x02, 0x00, 0x00, 0x12, 0x34, 0x56}

/** Default static IP address */
#define W5500_DEFAULT_IP {192, 168, 0, 12}

/** Default subnet mask */
#define W5500_DEFAULT_SUBNET {255, 255, 255, 0}

/** Default gateway address */
#define W5500_DEFAULT_GATEWAY {192, 168, 0, 1}

/** Default DNS server address (Google DNS) */
static const uint8_t W5500_DEFAULT_DNS[4] = {8, 8, 8, 8};

/** Enable DHCP (0=static IP, 1=DHCP) */
#define W5500_USE_DHCP 0

/** @} */

/** @} */

/**
 * @defgroup W5500_PROTO Protocol Support Flags
 * @brief Enable/disable protocol features
 * @{
 */

/** @name Protocol Flags
 *  Enable/disable supported protocols.
 *  @{ */

/** Enable TCP protocol support */
#define W5500_ENABLE_TCP 1

/** Enable UDP protocol support */
#define W5500_ENABLE_UDP 1

/** Enable SNMP protocol support */
#define W5500_ENABLE_SNMP 1

/** Enable ICMP (ping) support */
#define W5500_ENABLE_ICMP 1

/** Enable IPv6 support (reserved for future use) */
#define W5500_ENABLE_IPV6 0

/** @} */

/** @} */

/**
 * @defgroup W5500_RTOS RTOS Integration Parameters
 * @brief FreeRTOS task and synchronization settings
 * @{
 */

/** @name RTOS Parameters
 *  Task sizes, timeouts, and polling intervals.
 *  @{ */

/** Network task stack size in bytes */
#define W5500_TASK_STACK 2048

/** Network event queue depth */
#define W5500_QUEUE_LENGTH 16

/** SPI mutex acquisition timeout in ticks */
#define W5500_SPI_TIMEOUT pdMS_TO_TICKS(100)

/** Socket operation timeout in ticks */
#define W5500_SOCKET_TIMEOUT pdMS_TO_TICKS(5000)

/** PHY link status check interval in milliseconds */
#define W5500_PHY_CHECK_MS 1000

/** Enable interrupt-driven socket events (0=polling, 1=interrupt) */
#define W5500_USE_INTERRUPTS 1

/** Interrupt polling interval when using interrupts (milliseconds) */
#define W5500_IRQ_POLL_MS 10

/** @} */

/** @} */

/**
 * @defgroup W5500_ADV Advanced TCP/IP Tuning
 * @brief Expert-level configuration parameters
 * @{
 */

/** @name TCP/IP Tuning
 *  Retry timing, MSS, and PHY settings.
 *  @{ */

/** TCP retransmission timeout in milliseconds */
#define W5500_RETRY_TIME 2000

/** TCP retransmission attempt count */
#define W5500_RETRY_COUNT 8

/** TCP Maximum Segment Size in bytes */
#define W5500_TCP_MSS 1460

/** TCP Keep-Alive interval in seconds (0=disabled) */
#define W5500_KEEPALIVE_TIME 120

/** Enable TCP_NODELAY to disable Nagle's algorithm */
#define W5500_TCP_NODELAY 0

/** PHY auto-negotiation enable */
#define W5500_PHY_AUTONEG 1

/** PHY forced speed (0=auto, 10=10Mbps, 100=100Mbps) */
#define W5500_PHY_SPEED 0

/** PHY forced duplex mode (0=auto, 1=half, 2=full) */
#define W5500_PHY_DUPLEX 0

/** @} */

/** @} */

/**
 * @defgroup W5500_DBG Debug and Logging
 * @brief Debugging output control
 * @{
 */

/** @name Debug Macros
 *  Conditional logging and debug helpers.
 *  @{ */

/** Enable general W5500 driver debug output */
#define W5500_DEBUG DEBUG

/** Enable socket operation debug */
#define W5500_DEBUG_SOCKET 0

/** Enable SPI transaction debug */
#define W5500_DEBUG_SPI 1

/** Enable network event logging */
#define W5500_LOG_EVENTS 1

#if W5500_DEBUG
#define W5500_DBG(...) DEBUG_PRINT(__VA_ARGS__)
#else
#define W5500_DBG(...) ((void)0)
#endif

/** @} */

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

/** @} */

/**
 * @brief Compile-time validation of socket buffer allocation
 *
 * @details
 * Ensures total TX and RX buffer allocations do not exceed W5500 hardware limits.
 * W5500 provides exactly 16KB for TX buffers and 16KB for RX buffers across all sockets.
 */
/** @name Compile-Time Validation
 *  @ingroup config02
 *  @{ */
#define _W5500_TX_TOTAL                                                                            \
    (W5500_TX_SIZE_S0 + W5500_TX_SIZE_S1 + W5500_TX_SIZE_S2 + W5500_TX_SIZE_S3 +                   \
     W5500_TX_SIZE_S4 + W5500_TX_SIZE_S5 + W5500_TX_SIZE_S6 + W5500_TX_SIZE_S7)

#define _W5500_RX_TOTAL                                                                            \
    (W5500_RX_SIZE_S0 + W5500_RX_SIZE_S1 + W5500_RX_SIZE_S2 + W5500_RX_SIZE_S3 +                   \
     W5500_RX_SIZE_S4 + W5500_RX_SIZE_S5 + W5500_RX_SIZE_S6 + W5500_RX_SIZE_S7)

#if _W5500_TX_TOTAL > 16
#error "W5500: Total TX buffer size exceeds 16KB hardware limit"
#endif

#if _W5500_RX_TOTAL > 16
#error "W5500: Total RX buffer size exceeds 16KB hardware limit"
#endif

/** @} */

#endif /* W5500_CONFIG_H */

/** @} */
