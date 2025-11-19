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

#define LOGGER_TAG "[LOGGER]"

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
 */
static void LoggerTask(void *arg) {
    (void)arg;

    stdio_init_all();
    vTaskDelay(pdMS_TO_TICKS(1000)); /* allow USB-CDC to enumerate */
    ECHO("%s Task started\r\n", LOGGER_TAG);

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

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

/**
 * @brief Query Logger READY state.
 */
bool Logger_IsReady(void) { return (logQueue != NULL); }

/**
 * @brief Initialize and start the Logger task with a deterministic enable gate.
 */
BaseType_t LoggerTask_Init(bool enable) {
    if (!enable) {
        return pdPASS; /* deterministically skipped */
    }

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
 * Formats a message into the internal LogItem_t buffer and enqueues it for the
 * Logger task to flush to the chosen backends (UART, USB-CDC, etc.).
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
 */
void Logger_MutePush(void) {
    taskENTER_CRITICAL();
    s_logger_mute_depth++;
    taskEXIT_CRITICAL();
}

/**
 * @brief End a critical logging section started with Logger_MutePush().
 */
void Logger_MutePop(void) {
    taskENTER_CRITICAL();
    if (s_logger_mute_depth > 0u) {
        s_logger_mute_depth--;
    }
    taskEXIT_CRITICAL();
}
