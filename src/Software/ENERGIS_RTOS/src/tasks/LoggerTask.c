/**
 * @file src/tasks/LoggerTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @brief Logger task implementation with queue-based message handling.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define LOGGER_TAG "[LOGGER]"

/**
 * @brief Log message queue item structure.
 *
 * Contains a single formatted log message ready for output. Messages are
 * pre-formatted by log_printf() before being queued, avoiding formatting
 * work in the output task.
 */
typedef struct {
    char msg[LOGGER_MSG_MAX]; /**< Null-terminated formatted log message. */
} LogItem_t;

/** Logger message queue handle. */
static QueueHandle_t logQueue;

/** Logger task handle for single-instance guard. */
static TaskHandle_t s_logger_task = NULL;

/** Mute depth counter for nested mute sections (0 = not muted). */
static volatile uint32_t s_logger_mute_depth = 0u;

/**
 * @brief Main logger task function.
 *
 * Continuously receives log messages from the queue and outputs them to
 * USB-CDC (stdout). Implements heartbeat monitoring and maintains responsiveness
 * to the health watchdog even when no messages are pending.
 *
 * Operation:
 * - Initializes USB-CDC on first run
 * - Waits 1 second for USB enumeration
 * - Polls queue with 50ms timeout to maintain watchdog responsiveness
 * - Outputs received messages immediately to stdout
 * - Sends periodic heartbeats to health monitor
 *
 * @param[in] arg Task parameters (unused).
 */
static void LoggerTask(void *arg) {
    (void)arg;

    /* Initialize USB-CDC for console output */
    stdio_init_all();
    vTaskDelay(pdMS_TO_TICKS(1000));
    ECHO("%s Task started\r\n", LOGGER_TAG);

    static uint32_t hb_log_ms = 0;
    LogItem_t item;

    /* Main message processing loop */
    for (;;) {
        /* Send periodic heartbeat to health monitor */
        uint32_t __now = to_ms_since_boot(get_absolute_time());
        if ((__now - hb_log_ms) >= LOGTASKBEAT_MS) {
            hb_log_ms = __now;
            Health_Heartbeat(HEALTH_ID_LOGGER);
        }

        /* Receive and output log messages with timeout for watchdog responsiveness */
        if (xQueueReceive(logQueue, &item, pdMS_TO_TICKS(50)) == pdPASS) {
            printf("%s", item.msg);
            Health_Heartbeat(HEALTH_ID_LOGGER);
        }
    }
}

/* Public API Implementation */

/** See loggertask.h for detailed documentation. */
bool Logger_IsReady(void) { return (logQueue != NULL); }

/** See loggertask.h for detailed documentation. */
BaseType_t LoggerTask_Init(bool enable) {
    /* Skip initialization if disabled */
    if (!enable) {
        return pdPASS;
    }

    /* Create message queue if not already created */
    if (logQueue == NULL) {
        logQueue = xQueueCreate(LOGGER_QUEUE_LEN, sizeof(LogItem_t));
        if (logQueue == NULL) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_LOGGER, ERR_SEV_ERROR, ERR_FID_LOGGERTASK, 0x0);
            ERROR_PRINT_CODE(errorcode, "%s Failed to create logger queue\r\n", LOGGER_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
            return pdFAIL;
        }
    }

    /* Create task if not already created */
    if (s_logger_task == NULL) {
        if (xTaskCreate(LoggerTask, "Logger", LOGGER_STACK_SIZE, NULL, LOGTASK_PRIORITY,
                        &s_logger_task) != pdPASS) {
            return pdFAIL;
        }
    }

    return pdPASS;
}

/** See loggertask.h for detailed documentation. */
void log_printf(const char *fmt, ...) {
    /* Validate logger initialization and format string */
    if (logQueue == NULL || fmt == NULL) {
        return;
    }

    /* Drop message if logger is muted */
    if (s_logger_mute_depth != 0u) {
        return;
    }

    /* Format message into queue item */
    LogItem_t item;
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(item.msg, sizeof(item.msg), fmt, args);
    va_end(args);

    /* Send to queue (non-blocking, drop if full) */
    (void)xQueueSend(logQueue, &item, 0);
}

/** See loggertask.h for detailed documentation. */
void log_printf_force(const char *fmt, ...) {
    /* Validate logger initialization and format string */
    if (logQueue == NULL || fmt == NULL) {
        return;
    }

    /* Format message into queue item (ignore mute state) */
    LogItem_t item;
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(item.msg, sizeof(item.msg), fmt, args);
    va_end(args);

    /* Send to queue (non-blocking, drop if full) */
    (void)xQueueSend(logQueue, &item, 0);
}

/** See loggertask.h for detailed documentation. */
void Logger_MutePush(void) {
    taskENTER_CRITICAL();
    s_logger_mute_depth++;
    taskEXIT_CRITICAL();
}

/** See loggertask.h for detailed documentation. */
void Logger_MutePop(void) {
    taskENTER_CRITICAL();
    if (s_logger_mute_depth > 0u) {
        s_logger_mute_depth--;
    }
    taskEXIT_CRITICAL();
}
