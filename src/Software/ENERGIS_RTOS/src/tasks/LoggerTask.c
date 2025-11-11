/**
 * @file src/tasks/LoggerTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details Implements a FreeRTOS-based logging task that receives log messages
 * via a queue and outputs them to stdio (USB-CDC).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/**
 * @brief Single log record placed on the logger queue.
 */
typedef struct {
    char msg[LOGGER_MSG_MAX]; /**< Null-terminated log line payload. */
} LogItem_t;

/** @brief Logger queue handle (created once in LoggerTask_Init). */
static QueueHandle_t logQueue;

/** @brief The logger task handle (for single-instance guard). */
static TaskHandle_t s_logger_task = NULL;

/**
 * @brief Logger task loop – prints all messages from the queue.
 * @param arg Unused.
 * @details
 * - Initializes stdio and waits ~1 s for USB-CDC enumeration.
 * - Periodically heartbeats @ref HEALTH_ID_LOGGER every 500 ms to the HealthTask,
 *   even if the queue is idle (prevents false "NEVER" or stale detections).
 * - Receives @ref LogItem_t items from @ref logQueue (50 ms timeout) and
 *   writes them to stdio with @c printf().
 * - Sends an additional heartbeat after each printed message.
 */
static void LoggerTask(void *arg) {
    (void)arg;

    stdio_init_all();
    vTaskDelay(pdMS_TO_TICKS(1000)); /* allow USB-CDC to enumerate */

    static uint32_t hb_log_ms = 0;
    LogItem_t item;

    for (;;) {
        /* periodic beat irrespective of queue traffic */
        uint32_t __now = to_ms_since_boot(get_absolute_time());
        if ((__now - hb_log_ms) >= LOGTASKBEAT_MS) {
            hb_log_ms = __now;
            Health_Heartbeat(HEALTH_ID_LOGGER);
        }

        /* wait briefly for a log item; stay responsive to HealthTask */
        if (xQueueReceive(logQueue, &item, pdMS_TO_TICKS(50)) == pdPASS) {
            printf("%s", item.msg);
            Health_Heartbeat(HEALTH_ID_LOGGER);
        }
    }
}

/**
 * @brief Query Logger READY state.
 * @return true if the logger queue exists, false otherwise.
 * @details
 * The logger is considered READY once the log queue has been created by
 * LoggerTask_Init() (i.e., @c logQueue != NULL). This is sufficient for
 * deterministic boot gating since the task is spawned immediately after
 * queue creation.
 */
bool Logger_IsReady(void) { return (logQueue != NULL); }

/**
 * @brief Initialize and start the Logger task with a deterministic enable gate.
 * @param enable Gate to allow or skip starting this subsystem.
 * @return pdPASS on success (or when skipped), pdFAIL on creation error.
 * @details
 * Boot order step 1. If @p enable is false, the function skips creation and
 * leaves the module NOT ready. When true, it creates @c logQueue (once) and
 * spawns the @c LoggerTask (once). On success the module becomes READY.
 */
BaseType_t LoggerTask_Init(bool enable) {
    if (!enable) {
        return pdPASS; /* deterministically skipped */
    }

    if (logQueue == NULL) {
        logQueue = xQueueCreate(LOGGER_QUEUE_LEN, sizeof(LogItem_t));
        if (logQueue == NULL) {
            return pdFAIL;
        }
    }

    if (s_logger_task == NULL) {
        if (xTaskCreate(LoggerTask, "Logger", LOGGER_STACK_SIZE,
                        NULL, /* task reads file-scope logQueue */
                        LOGTASK_PRIORITY, &s_logger_task) != pdPASS) {
            return pdFAIL;
        }
    }

    return pdPASS;
}

/**
 * @brief Printf-style logging into the Logger queue.
 * @param fmt printf-style format string.
 * @param ... Variadic arguments for @p fmt.
 * @details
 * Formats a message and sends it to @c logQueue created by LoggerTask_Init().
 * The message is truncated to @c LOGGER_MSG_MAX bytes if necessary.
 * This call is non-blocking; if the queue is full, the message is dropped.
 */
void log_printf(const char *fmt, ...) {
    if (logQueue == NULL || fmt == NULL) {
        return; /* Logger not initialized yet */
    }

    LogItem_t item;
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(item.msg, sizeof(item.msg), fmt, args);
    va_end(args);

    /* Non-blocking send; drop if full */
    (void)xQueueSend(logQueue, &item, 0);
}
