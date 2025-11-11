/**
 * @file src/tasks/ConsoleTask.h
 * @author DvidMakesThings - David Sipos
 * 
 * @defgroup tasks04 3. Console Task
 * @ingroup tasks
 * @brief UART console task implementation (RTOS version, polling USB-CDC)
 * @{
 *
 * @version 2.0.0
 * @date 2025-11-08
 * 
 * @details
 * Architecture:
 * 1. ConsoleTask polls USB-CDC at 10ms intervals (no ISR)
 * 2. Accumulates characters into line buffer
 * 3. On complete line: parses and dispatches to handlers
 * 4. Handlers execute directly or query other tasks
 *
 * Note: Console input comes from USB-CDC (stdio), not a hardware UART.
 * This matches the CMakeLists.txt config: pico_enable_stdio_usb(... 1)
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */


#ifndef CONSOLE_TASK_H
#define CONSOLE_TASK_H

#include "../CONFIG.h"

/* ==================== Queue Handles ==================== */

/** Power/relay control queue: ConsoleTask -> PowerTask (future) */
extern QueueHandle_t q_power;

/** Config storage queue: ConsoleTask -> StorageTask (future) */
extern QueueHandle_t q_cfg;

/** Meter reading queue: ConsoleTask -> MeterTask (future) */
extern QueueHandle_t q_meter;

/** Network operation queue: ConsoleTask -> NetTask (future) */
extern QueueHandle_t q_net;

/* ==================== Message Structures ==================== */

/** Power/relay control message kinds */
typedef enum {
    PWR_CMD_SET_RELAY,     /**< Set relay on/off */
    PWR_CMD_GET_RELAY,     /**< Query relay state */
    PWR_CMD_RELAY_ALL_OFF, /**< Emergency all-off */
} power_cmd_kind_t;

/** Power/relay control message */
typedef struct {
    power_cmd_kind_t kind; /**< Command type */
    uint8_t channel;       /**< Channel 0-7 (for SET/GET) */
    uint8_t value;         /**< 0=off, 1=on (for SET) */
} power_msg_t;

/* Config message (placeholder for StorageTask) */
typedef struct {
    uint8_t action; /**< 0=read, 1=write, 2=commit, 3=defaults */
    char key[32];   /**< Config key name */
    char val[64];   /**< Config value */
} cfg_msg_t;

/* Meter message (placeholder for MeterTask) */
typedef struct {
    uint8_t action;  /**< 0=read_now, 1=set_channel */
    uint8_t channel; /**< Channel 0-7 */
} meter_msg_t;

/* Network message (placeholder for NetTask) */
typedef struct {
    uint8_t action; /**< 0=set_ip, 1=set_subnet, 2=set_gw, 3=set_dns */
    uint8_t ip[4];  /**< IP address bytes */
} net_msg_t;

/* ==================== Public API ==================== */

/**
 * @brief Initialize and start the Console task with a deterministic enable gate.
 *
 * @details
 * Deterministic boot order step 2/6.
 * - Waits up to 5 s for Logger_IsReady() to report ready.
 * - Creates Console queues (q_power, q_cfg, q_meter, q_net).
 * - Spawns the ConsoleTask.
 * - Returns pdPASS on success, pdFAIL on any creation error.
 *
 * @instructions
 * Call after LoggerTask_Init(true):
 *   BaseType_t rc = ConsoleTask_Init(true);
 * Gate subsequent steps with Console_IsReady().
 *
 * @param enable Gate that allows or skips starting this subsystem.
 * @return pdPASS on success (or when skipped), pdFAIL on creation error.
 */
BaseType_t ConsoleTask_Init(bool enable);


/**
 * @brief Get Console READY state.
 *
 * @details
 * Console is considered READY once its core config queue has been created by
 * ConsoleTask_Init(). This avoids any extra latches or extern variables.
 *
 * @return true if the Console config queue exists, false otherwise.
 */
bool Console_IsReady(void);


#endif /* CONSOLE_TASK_H */

/** @} */