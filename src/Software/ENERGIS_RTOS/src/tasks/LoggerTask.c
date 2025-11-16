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

static volatile uint32_t s_logger_mute_depth = 0u;

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
 *
 * Formats a message into the internal LogItem_t buffer and enqueues it for the
 * Logger task to flush to the chosen backends (UART, USB-CDC, etc.).
 *
 * When the internal mute depth counter is non-zero (see @ref Logger_MutePush
 * and @ref Logger_MutePop), this function silently drops messages. This allows
 * long, structured dumps (like EEPROM hex dumps) to run without being polluted
 * by unrelated log traffic.
 *
 * @param fmt printf-style format string (must not be NULL)
 * @param ... Variable arguments corresponding to @p fmt
 */
void log_printf(const char *fmt, ...) {
    if (logQueue == NULL || fmt == NULL) {
        return; /* Logger not initialized yet */
    }

    /* Drop message if logger is muted */
    if (s_logger_mute_depth != 0u) {
        return;
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
 * @brief Printf-style logging that bypasses the mute gate.
 *
 * This function behaves like @ref log_printf but ignores the internal mute
 * depth counter. It always attempts to enqueue the message as long as the
 * logger queue exists.
 *
 * Intended for highly structured sequences (e.g., EEPROM dumps) that should
 * still appear even while the logger is temporarily muted for all other
 * tasks.
 *
 * @param fmt printf-style format string (must not be NULL)
 * @param ... Variable arguments corresponding to @p fmt
 */
void log_printf_force(const char *fmt, ...) {
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
 * @brief Begin a critical logging section by muting normal log traffic.
 *
 * @details
 * Increments the internal mute depth counter. While the counter is non-zero,
 * calls to @ref log_printf from any task will be silently dropped.
 *
 * This is useful when printing large, structured outputs (e.g. EEPROM dump)
 * and you want to avoid interleaving from other subsystems.
 */
void Logger_MutePush(void) {
    taskENTER_CRITICAL();
    s_logger_mute_depth++;
    taskEXIT_CRITICAL();
}

/**
 * @brief End a critical logging section started with Logger_MutePush().
 *
 * @details
 * Decrements the internal mute depth counter, saturating at zero. When the
 * counter reaches zero, @ref log_printf resumes normal operation.
 */
void Logger_MutePop(void) {
    taskENTER_CRITICAL();
    if (s_logger_mute_depth > 0u) {
        s_logger_mute_depth--;
    }
    taskEXIT_CRITICAL();
}
