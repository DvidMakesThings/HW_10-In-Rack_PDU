/**
 * @file src/tasks/NetTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks07 7. Network Task
 * @ingroup tasks
 * @brief Network stack manager for W5500 Ethernet, HTTP server, and SNMP agent.
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-07
 *
 * @details
 * This module implements a FreeRTOS task for managing all network operations
 * on the PDU. It is the exclusive owner of the W5500 Ethernet controller and
 * coordinates HTTP and SNMP services with robust link supervision and standby mode support.
 *
 * Architecture:
 * - Single-owner model: NetTask has exclusive access to W5500 hardware
 * - Manages three W5500 hardware sockets: HTTP (0), SNMP agent (1), SNMP trap (2)
 * - Supervises PHY link status with automatic reinitialization on link-up events
 * - Integrates with PowerMgr for standby mode support
 * - Coordinates with StorageTask for network configuration loading
 * - Provides visual feedback via ETH LED (solid when linked, blinking when down)
 *
 * Key Features:
 * - W5500 Ethernet controller management (SPI, GPIO, reset sequencing)
 * - HTTP web server for PDU control interface and metrics endpoint
 * - SNMP agent for network management protocol support
 * - SNMP trap sender for proactive event notifications
 * - Robust link detection and automatic recovery from cable disconnection
 * - Full reinitialization on each link-up event to prevent stale connections
 * - Standby mode support: suspends all operations when system enters standby
 * - ETH LED control: solid ON when linked, blinking at 1 Hz when unlinked
 * - Graceful degradation: system operates normally even without Ethernet
 *
 * Network Configuration:
 * - Loaded from EEPROM via StorageTask at boot
 * - Supports both static IP and DHCP modes
 * - Configurable IP address, subnet mask, gateway, DNS server
 * - Fixed MAC address (device-specific or factory default)
 * - Configuration cached for rapid reinitialization on link events
 *
 * Link Supervision:
 * - Polls W5500 PHY status continuously during RUN mode
 * - Detects link-up transitions: triggers full W5500 reset and service restart
 * - Detects link-down transitions: enables ETH LED blinking indicator
 * - Prevents ERR_CONNECTION_REFUSED after cable reconnection via full reinit
 * - Short timeout guards prevent indefinite blocking during reinitialization
 *
 * Service Management:
 * - HTTP server: processes incoming requests with cooperative scheduling
 * - SNMP agent: polls for incoming requests, rate-limited to prevent starvation
 * - Health monitoring: tracks long-blocking operations and reports to watchdog
 * - Cooperative pacing: yields between HTTP and SNMP operations to maintain responsiveness
 *
 * Standby Mode Behavior:
 * - When system enters STANDBY via Power_EnterStandby():
 *   - W5500 held in hardware reset by power manager
 *   - All network processing suspended
 *   - Maintains reduced heartbeat (500ms) for watchdog
 * - On exit from STANDBY via Power_ExitStandby():
 *   - Detects state transition and calls net_reinit_from_cache()
 *   - Restores W5500 configuration and restarts services
 *   - Re-evaluates link status and updates ETH LED accordingly
 *
 * Error Handling:
 * - Graceful boot without Ethernet: rest of system operates normally
 * - Hardware init failure: enters safe loop with heartbeat only
 * - Configuration load failure: applies factory defaults automatically
 * - Link supervision: handles intermittent connections robustly
 * - All failures logged via structured error codes to EEPROM
 *
 * Usage Pattern:
 * 1. Call NetTask_Init(true) during system initialization (after StorageTask)
 * 2. Query readiness via Net_IsReady() before accessing network functions
 * 3. Task autonomously manages W5500, HTTP, SNMP, and link supervision
 * 4. No explicit shutdown needed; standby mode handled automatically
 *
 * Integration Points:
 * - StorageTask: loads network configuration from EEPROM
 * - PowerMgr: respects standby mode and coordinates W5500 reset control
 * - SwitchTask: controls ETH LED for visual link status feedback
 * - Health monitor: sends heartbeat and reports long-blocking operations
 * - HTTP handlers: serve web interface and metrics endpoint
 * - SNMP handlers: respond to management queries and send traps
 *
 * @note NetTask must be initialized after StorageTask due to configuration dependency.
 * @note No other task should access W5500 hardware or SPI interface directly.
 * @note System continues operating normally even if Ethernet hardware fails.
 * @note HTTP server runs on TCP port 80; SNMP agent on UDP port 161.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef NETTASK_H
#define NETTASK_H

#include "../CONFIG.h"

/**
 * @name Task Constants
 * @{
 */
/**
 * @brief Network task stack size
 */
#define NET_TASK_STACK_SIZE 1024

/**
 * @brief Network task cycle time in milliseconds
 */
#define NET_TASK_CYCLE_MS 10
/** @} */

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

/**
 * @name Public API
 * @{
 */

/**
 * @brief Apply network configuration from StorageTask and initialize the W5500.
 *
 * Converts persistent network configuration into W5500 register settings and
 * performs low-level chip initialization. Used during boot and on link-up events
 * to configure IP, subnet, gateway, DNS, MAC, and mode (DHCP/static).
 *
 * Configuration Process:
 * 1. Map networkInfo structure to w5500_NetConfig format
 * 2. Call w5500_chip_init() to program chip registers
 * 3. Verify configuration and log results
 * 4. Return success/failure status
 *
 * @param[in] ni Pointer to networkInfo structure loaded from EEPROM.
 *
 * @return true if W5500 initialized successfully with provided configuration.
 * @return false if hardware init failed or PHY link is not present.
 *
 * @note Link absence is not treated as fatal; system continues without Ethernet.
 * @note Safe to call multiple times; used for reinitialization on link events.
 * @note Logs configuration to console via w5500_print_network().
 */
/** @ingroup tasks07 */
bool ethernet_apply_network_from_storage(const networkInfo *ni);

/**
 * @brief Create and start the Network Task with a deterministic enable gate.
 *
 * Creates the NetTask FreeRTOS task and waits for StorageTask readiness before
 * proceeding. Implements deterministic boot sequencing to ensure configuration
 * is available before network initialization.
 *
 * Initialization Sequence:
 * 1. Wait for StorageTask readiness with 5-second timeout
 * 2. Spawn NetTask with 4KB stack at NETTASK_PRIORITY
 * 3. Task internally waits for storage_wait_ready() before loading config
 * 4. Task applies network config and starts HTTP/SNMP services
 *
 * @param[in] enable Set true to initialize and start task, false to skip
 *                   initialization deterministically without side effects.
 *
 * @return pdPASS on successful initialization or when skipped (enable=false).
 * @return pdFAIL if task creation fails.
 *
 * @note Call after SwitchTask_Init(true) in boot sequence (step 6/7).
 * @note Logs error codes on failure via error logger if enabled.
 * @note Idempotent: safe to call multiple times, creates resources only once.
 * @note Task begins autonomous operation immediately after creation.
 */
/** @ingroup tasks07 */
BaseType_t NetTask_Init(bool enable);

/**
 * @brief Query network subsystem readiness status.
 *
 * Provides a thread-safe method to check whether NetTask has been created
 * and is operational. Used for deterministic sequencing to ensure network
 * subsystem is available before accessing network-dependent functions.
 *
 * @return true if NetTask has been created successfully and is running.
 * @return false if NetTask_Init() was not called, called with enable=false,
 *         or initialization failed.
 *
 * @note Based on task handle existence, providing immediate readiness indication.
 * @note Safe to call from any task context or early in boot before scheduler starts.
 */
/** @ingroup tasks07 */
bool Net_IsReady(void);

/** @} */

#endif // NETTASK_H

/** @} */