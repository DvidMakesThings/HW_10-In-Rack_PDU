/**
 * @file LoggerTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks03 3. Logger
 * @ingroup tasks
 * @brief Logger task implementation (RTOS version)
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * Implements a FreeRTOS-based logging task that receives log messages
 * via a queue and outputs them to stdio (USB-CDC).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

typedef struct {
    char msg[LOGGER_MSG_MAX];
} LogItem_t;

static QueueHandle_t logQueue;

/**
 * @brief Logger task loop – prints all messages from the queue.
 * @param arg Unused.
 * @details
 * Waits on the file-scope @c logQueue and prints each received log record
 * to stdio (USB-CDC/UART). Performs a short delay after stdio init to let
 * USB enumerate before emitting output.
 */
static void LoggerTask(void *arg) {
    (void)arg;
    LogItem_t item;

    stdio_init_all();
    vTaskDelay(pdMS_TO_TICKS(1000)); /* allow USB-CDC to enumerate */

    for (;;) {
        if (xQueueReceive(logQueue, &item, portMAX_DELAY) == pdPASS) {
            printf("%s", item.msg);
        }
    }
}

/**
 * @brief Query Logger READY state.
 *
 * @details
 * The logger is considered READY once the log queue has been created by
 * LoggerTask_Init() (i.e., @c logQueue != NULL). This is sufficient for
 * deterministic boot gating since the task is spawned immediately after
 * queue creation.
 *
 * @return true if the logger queue exists, false otherwise.
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
    static TaskHandle_t s_logger_task = NULL;

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
                        tskIDLE_PRIORITY + 1, &s_logger_task) != pdPASS) {
            return pdFAIL;
        }
    }

    return pdPASS;
}

/**
 * @brief Printf-style logging into the Logger queue.
 * @param fmt printf-style format string.
 * @param ... Variadic arguments.
 * @details
 * Formats a message and sends it to @c logQueue created by LoggerTask_Init().
 * The message is truncated to @c LOGGER_MSG_MAX bytes if necessary.
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

/**
 * @}
 */