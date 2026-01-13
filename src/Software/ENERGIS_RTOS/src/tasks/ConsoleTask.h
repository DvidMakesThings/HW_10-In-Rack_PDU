/**
 * @file src/tasks/ConsoleTask.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks02 2. Console Task
 * @ingroup tasks
 * @brief USB-CDC console command interface for system control and diagnostics.
 * @{
 *
 * @version 2.0.0
 * @date 2025-11-08
 *
 * @details
 * This module implements a command-line interface accessible via USB-CDC for PDU
 * configuration, control, and diagnostics. The console provides comprehensive access
 * to system functions including relay control, power monitoring, calibration, network
 * configuration, and firmware management.
 *
 * Architecture:
 * - Polls USB-CDC interface at configurable intervals without using interrupts
 * - Accumulates characters into line buffer with backspace support
 * - Parses complete lines and dispatches to command handlers
 * - Commands execute directly or interact with other tasks via queues
 * - Supports standby mode with suspended command processing
 *
 * Command Categories:
 * - General: System information, temperature, reboot, bootloader access
 * - Output Control: Channel switching, overcurrent status and reset
 * - Measurement: Power data reading, calibration procedures
 * - Network: IP configuration, network information display
 * - Debug: Advanced diagnostics, EEPROM operations, provisioning
 *
 * Input Handling:
 * - Line-based command input with CR/LF termination
 * - Backspace/delete key support for editing
 * - Case-insensitive command matching
 * - Automatic argument parsing and validation
 *
 * Power Management:
 * - Automatically suspends in standby mode to conserve power
 * - Reduced heartbeat rate during standby
 * - Resumes normal operation on exit from standby
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CONSOLE_TASK_H
#define CONSOLE_TASK_H

#include "../CONFIG.h"

/**
 * @name Inter-Task Communication Queues
 * @brief Queues used by ConsoleTask to communicate with subsystems.
 * @{
 */

/**
 * @brief Power/relay control queue.
 *
 * Message queue for power and relay control commands from ConsoleTask
 * to other subsystems. Reserved for future inter-task communication.
 */
extern QueueHandle_t q_power;

/**
 * @brief Configuration storage queue.
 *
 * Message queue for configuration read/write operations between ConsoleTask
 * and StorageTask. Used for persistent settings management.
 */
extern QueueHandle_t q_cfg;

/**
 * @brief Meter reading queue.
 *
 * Message queue for power measurement requests from ConsoleTask to MeterTask.
 * Reserved for future asynchronous meter queries.
 */
extern QueueHandle_t q_meter;

/**
 * @brief Network operation queue.
 *
 * Message queue for network configuration commands from ConsoleTask to NetTask.
 * Reserved for future network management operations.
 */
extern QueueHandle_t q_net;
/** @} */

/* Message Type Definitions */

/**
 * @enum power_cmd_kind_t
 * @brief Power/relay control command types sent via `q_power`.
 * @ingroup tasks02
 * @details Commands control individual relays or perform global shutdown.
 */
typedef enum {
    PWR_CMD_SET_RELAY,     /**< Set specific relay output state (on/off). */
    PWR_CMD_GET_RELAY,     /**< Query current state of specific relay output. */
    PWR_CMD_RELAY_ALL_OFF, /**< Emergency shutdown: turn off all relay outputs. */
} power_cmd_kind_t;

/**
 * @struct power_msg_t
 * @brief Message for power/relay control operations.
 * @ingroup tasks02
 * @details Sent on `q_power` to set/query relay states, including target channel
 * and desired on/off value.
 */
typedef struct {
    power_cmd_kind_t kind; /**< Type of power command to execute. */
    uint8_t channel;       /**< Target output channel (0-7). */
    uint8_t value;         /**< Desired state: 0=off, 1=on (used for SET commands). */
} power_msg_t;

/**
 * @struct cfg_msg_t
 * @brief Configuration storage message for StorageTask.
 * @ingroup tasks02
 * @details Supports read, write, commit, and factory defaults operations; used
 * on `q_cfg` to route persistent configuration changes.
 */
typedef struct {
    uint8_t action; /**< Operation: 0=read, 1=write, 2=commit, 3=load defaults. */
    char key[32];   /**< Configuration parameter name. */
    char val[64];   /**< Configuration parameter value. */
} cfg_msg_t;

/**
 * @struct meter_msg_t
 * @brief Meter reading message for MeterTask.
 * @ingroup tasks02
 * @details Reserved for future asynchronous measurement queries; sent via `q_meter`.
 */
typedef struct {
    uint8_t action;  /**< Operation: 0=read_now, 1=set_channel. */
    uint8_t channel; /**< Target measurement channel (0-7). */
} meter_msg_t;

/**
 * @struct net_msg_t
 * @brief Network operation message for NetTask.
 * @ingroup tasks02
 * @details Reserved for future network parameter updates; sent via `q_net`.
 */
typedef struct {
    uint8_t action; /**< Operation: 0=set_ip, 1=set_subnet, 2=set_gateway, 3=set_dns. */
    uint8_t ip[4];  /**< IP address octets. */
} net_msg_t;

/* Public API */

/**
 * @brief Initialize and start the console task.
 *
 * Creates the ConsoleTask FreeRTOS task, initializes USB-CDC polling, sets up
 * inter-task message queues, and prepares the command dispatcher. Implements
 * deterministic initialization with logger readiness gate.
 *
 * Initialization Sequence:
 * 1. Waits for LoggerTask readiness with timeout
 * 2. Creates message queues for inter-task communication
 * 3. Spawns ConsoleTask with configured priority
 * 4. Task begins USB-CDC polling and command processing
 *
 * Message Queues Created:
 * - q_power: Power/relay control commands (8 messages deep)
 * - q_cfg: Configuration storage operations (8 messages deep)
 * - q_meter: Meter reading requests (8 messages deep)
 * - q_net: Network configuration commands (8 messages deep)
 *
 * @param[in] enable Set true to initialize and start task, false to skip
 *                   initialization deterministically without side effects.
 *
 * @return pdPASS on successful initialization or when skipped (enable=false).
 * @return pdFAIL if initialization fails (queue creation, task creation).
 *
 * @note Call after LoggerTask_Init() in boot sequence (step 2/6).
 * @note Use Console_IsReady() to verify initialization before dependent tasks.
 * @note Logs error codes to error logger on failure.
 */
BaseType_t ConsoleTask_Init(bool enable);

/**
 * @brief Query console task readiness status.
 *
 * Provides a thread-safe method to check whether ConsoleTask has completed
 * initialization successfully. Used for deterministic boot sequencing to
 * ensure proper task dependency ordering.
 *
 * @return true if ConsoleTask_Init(true) completed successfully and queues are created.
 * @return false if ConsoleTask_Init() was not called, called with enable=false,
 *         or initialization failed.
 *
 * @note Based on q_cfg queue existence, avoiding extra state variables.
 * @note Safe to call from any task context.
 */
bool Console_IsReady(void);

#endif /* CONSOLE_TASK_H */

/** @} */