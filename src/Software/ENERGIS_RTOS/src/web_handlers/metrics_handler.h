/**
 * @file src/web_handlers/metrics_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui6 6. Metrics Handler
 * @ingroup webhandlers
 * @brief Handler for the page /metrics
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-15
 *
 * @details Implements /metrics HTTP endpoint that exposes ENERGIS runtime data
 * in Prometheus/OpenMetrics text format (version 0.0.4). Provides read-only
 * access to system counters, gauges, and per-channel telemetry for scraping
 * by Prometheus or compatible monitoring systems.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef METRICS_HANDLER_H
#define METRICS_HANDLER_H

#include "../CONFIG.h"

/* =====================  Metrics Configuration  ======================== */
/**
 * @brief Static buffer size for metrics rendering (bytes)
 */
#define METRICS_BUFFER_SIZE 4096

/* =====================  Metrics Counters Structure  =================== */
/**
 * @brief Global metrics counters for system-level statistics.
 *
 * @details All counters are monotonically increasing since boot. Access must
 * be atomic or protected by appropriate synchronization when incrementing.
 */
typedef struct {
    volatile uint32_t http_requests_total;   /**< Total HTTP requests served */
    volatile uint32_t snmp_requests_total;   /**< Total SNMP requests received */
    volatile uint32_t relay_toggles_total;   /**< Total relay state changes */
    volatile uint32_t watchdog_resets_total; /**< Total watchdog resets since power-on */
} metrics_counters_t;

/* =====================  Global Counters Instance  ===================== */
/**
 * @brief Global metrics counters instance.
 * @note Accessed by NetTask, MeterTask, and metrics handler
 */
extern metrics_counters_t g_metrics;

/* =====================  Metrics Handler API  ========================== */
/**
 * @brief Initialize metrics subsystem.
 *
 * @details Resets all counters to zero. Must be called during system init
 * before any counters are incremented.
 *
 * @return None
 */
void metrics_init(void);

/**
 * @brief Handle HTTP GET request to /metrics endpoint.
 *
 * @param sock Socket number for the established HTTP connection
 *
 * @return None
 *
 * @details Renders system metrics in OpenMetrics text format and sends HTTP
 * response. Returns 404 if metrics feature is disabled at compile-time or
 * runtime. Uses static buffer to avoid heap allocation. Non-blocking: reads
 * only from cached snapshots maintained by MeterTask and other subsystems.
 *
 * Response format:
 * - Content-Type: text/plain; version=0.0.4; charset=utf-8
 * - Connection: close
 * - Body: OpenMetrics exposition format
 *
 * @http
 * - 200 OK on success with metrics text body
 * - 404 Not Found if feature disabled
 */
void handle_metrics_request(uint8_t sock);

/**
 * @brief Increment HTTP request counter.
 *
 * @details Thread-safe atomic increment. Call once per HTTP request received.
 *
 * @return None
 */
static inline void metrics_inc_http_requests(void) {
    __atomic_fetch_add(&g_metrics.http_requests_total, 1, __ATOMIC_RELAXED);
}

/**
 * @brief Increment SNMP request counter.
 *
 * @details Thread-safe atomic increment. Call once per SNMP request received.
 *
 * @return None
 */
static inline void metrics_inc_snmp_requests(void) {
    __atomic_fetch_add(&g_metrics.snmp_requests_total, 1, __ATOMIC_RELAXED);
}

/**
 * @brief Increment relay toggle counter.
 *
 * @details Thread-safe atomic increment. Call once per relay state change.
 *
 * @return None
 */
static inline void metrics_inc_relay_toggles(void) {
    __atomic_fetch_add(&g_metrics.relay_toggles_total, 1, __ATOMIC_RELAXED);
}

/**
 * @brief Increment watchdog reset counter.
 *
 * @details Thread-safe atomic increment. Call once per watchdog reset event.
 *
 * @return None
 */
static inline void metrics_inc_watchdog_resets(void) {
    __atomic_fetch_add(&g_metrics.watchdog_resets_total, 1, __ATOMIC_RELAXED);
}

#endif /* METRICS_HANDLER_H */

/** @} */