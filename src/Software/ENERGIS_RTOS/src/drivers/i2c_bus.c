/**
 * @file i2c_bus.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.1.0
 * @date 2025-12-30
 *
 * @details
 * Provides a single FIFO-serialized execution path for all I2C transactions
 * across both controllers (i2c0 and i2c1). Callers use synchronous wrappers
 * that submit work to a bus task and block until completion, eliminating
 * interleaving and reducing race conditions under load.
 *
 * v2.1.0 Changes:
 * - Added I2C0 transaction tracing to debug display MCP reset issue
 * - Logs all writes to I2C0 with address, register, and value
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"
#include "task.h"

/**
 * @brief Enable I2C0 transaction tracing for debugging.
 */
#ifndef I2C_TRACE_I2C0
#define I2C_TRACE_I2C0 1
#endif

/**
 * @brief Only trace transactions to these addresses (0 = all).
 */
#ifndef I2C_TRACE_ADDR_FILTER
#define I2C_TRACE_ADDR_FILTER 0x21 /* Display MCP only */
#endif

typedef enum { I2C_OP_WRITE = 0, I2C_OP_READ = 1, I2C_OP_WRITE_READ = 2 } i2c_op_t;

typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
    TaskHandle_t caller;
    uint32_t timeout_us;
    int result; /* filled by bus task */
    i2c_op_t op;
    union {
        struct {
            uint8_t *buf;
            size_t len;
            bool nostop;
            bool is_read; /* false=write, true=read */
        } simple;
        struct {
            const uint8_t *wbuf;
            size_t wlen;
            uint8_t *rbuf;
            size_t rlen;
        } wr;
    } u;
} i2c_bus_request_t;

/* One queue serializes all I2C transactions across i2c0/i2c1 */
static QueueHandle_t s_i2cq = NULL; /* queue of pointers to requests */
static TaskHandle_t s_bus_task = NULL;

/* Single global I2C mutex (legacy compatibility for callers using lock/unlock) */
static SemaphoreHandle_t s_i2c_mutex = NULL;
static TickType_t s_i2c_lock_tick = 0;
static TaskHandle_t s_i2c_owner = NULL;

#ifndef DIAG_I2C_LOCK
#define DIAG_I2C_LOCK 0
#endif
#ifndef I2C_LOCK_WARN_MS
#define I2C_LOCK_WARN_MS 5
#endif

static inline SemaphoreHandle_t *select_mutex(i2c_inst_t *i2c) {
    (void)i2c;
    return &s_i2c_mutex;
}

/* Legacy lock/unlock (no longer required by new wrappers) */
void I2C_BusLock(i2c_inst_t *i2c) {
    SemaphoreHandle_t *m = select_mutex(i2c);
    if (*m == NULL) {
        taskENTER_CRITICAL();
        if (*m == NULL) {
            *m = xSemaphoreCreateMutex();
        }
        taskEXIT_CRITICAL();
    }
    if (*m) {
        xSemaphoreTake(*m, portMAX_DELAY);
        if (DIAG_I2C_LOCK) {
            TickType_t now = xTaskGetTickCount();
            s_i2c_lock_tick = now;
            s_i2c_owner = xTaskGetCurrentTaskHandle();
        }
    }
}

void I2C_BusUnlock(i2c_inst_t *i2c) {
    SemaphoreHandle_t m = *(select_mutex(i2c));
    if (m) {
        if (DIAG_I2C_LOCK) {
            TickType_t start = s_i2c_lock_tick;
            TaskHandle_t owner = s_i2c_owner;
            if (start != 0) {
                TickType_t now = xTaskGetTickCount();
                TickType_t delta = (now >= start) ? (now - start) : 0;
                if (delta >= pdMS_TO_TICKS(I2C_LOCK_WARN_MS)) {
                    const char *name = pcTaskGetName(owner);
                    (void)name;
                }
            }
            s_i2c_lock_tick = 0;
            s_i2c_owner = NULL;
        }
        xSemaphoreGive(m);
    }
}

/* -------------------- I2C Tracing ------------------- */

#if I2C_TRACE_I2C0

/**
 * @brief Log an I2C write transaction for debugging.
 */
static void trace_i2c_write(i2c_inst_t *i2c, uint8_t addr, const uint8_t *buf, size_t len,
                            int result) {
    /* Only trace I2C0 */
    if (i2c != i2c0)
        return;

    /* Filter by address if configured */
    if (I2C_TRACE_ADDR_FILTER != 0 && addr != I2C_TRACE_ADDR_FILTER)
        return;
    (void)buf;
    (void)len;
    (void)result;
}

/**
 * @brief Log an I2C read transaction for debugging.
 */
static void trace_i2c_read(i2c_inst_t *i2c, uint8_t addr, const uint8_t *buf, size_t len,
                           int result) {
    /* Only trace I2C0 */
    if (i2c != i2c0)
        return;

    /* Filter by address if configured */
    if (I2C_TRACE_ADDR_FILTER != 0 && addr != I2C_TRACE_ADDR_FILTER)
        return;
    (void)buf;
    (void)len;
    (void)result;
}

#else
#define trace_i2c_write(i2c, addr, buf, len, result) ((void)0)
#define trace_i2c_read(i2c, addr, buf, len, result) ((void)0)
#endif

/* -------------------- I2C Bus Manager Implementation ------------------- */

static void i2c_bus_task(void *arg) {
    (void)arg;
    for (;;) {
        i2c_bus_request_t *reqp = NULL;
        if (xQueueReceive(s_i2cq, &reqp, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        int rc = -1;
        if (reqp->op == I2C_OP_WRITE_READ) {
            /* Address/register phase: no stop */
            rc = i2c_write_timeout_us(reqp->i2c, reqp->addr, reqp->u.wr.wbuf, reqp->u.wr.wlen, true,
                                      reqp->timeout_us);
            if (rc == (int)reqp->u.wr.wlen) {
                rc = i2c_read_timeout_us(reqp->i2c, reqp->addr, reqp->u.wr.rbuf, reqp->u.wr.rlen,
                                         false, reqp->timeout_us);
                trace_i2c_read(reqp->i2c, reqp->addr, reqp->u.wr.rbuf, reqp->u.wr.rlen, rc);
            }
            reqp->result = rc;
        } else {
            bool is_read = reqp->u.simple.is_read;
            uint8_t *buf = reqp->u.simple.buf;
            size_t len = reqp->u.simple.len;
            bool nostop = reqp->u.simple.nostop;
            rc = is_read ? i2c_read_timeout_us(reqp->i2c, reqp->addr, buf, len, nostop,
                                               reqp->timeout_us)
                         : i2c_write_timeout_us(reqp->i2c, reqp->addr, buf, len, nostop,
                                                reqp->timeout_us);
            reqp->result = rc;

            /* Trace after operation */
            if (is_read) {
                trace_i2c_read(reqp->i2c, reqp->addr, buf, len, rc);
            } else {
                trace_i2c_write(reqp->i2c, reqp->addr, buf, len, rc);
            }
        }
        xTaskNotifyGive(reqp->caller);
    }
}

void I2C_BusInit(void) {
    if (s_i2cq != NULL) {
        return;
    }
    s_i2cq = xQueueCreate(16, sizeof(i2c_bus_request_t *));
    configASSERT(s_i2cq != NULL);
    BaseType_t ok = xTaskCreate(i2c_bus_task, "I2CBus", configMINIMAL_STACK_SIZE + 256, NULL,
                                tskIDLE_PRIORITY + 2, &s_bus_task);
    configASSERT(ok == pdPASS);
}

static inline int i2c_bus_submit_simple(i2c_inst_t *i2c, uint8_t addr, uint8_t *buf, size_t len,
                                        bool nostop, uint32_t timeout_us, bool is_read) {
    if (s_i2cq == NULL) {
        /* Fallback: direct call if not initialized yet */
        return is_read ? i2c_read_timeout_us(i2c, addr, buf, len, nostop, timeout_us)
                       : i2c_write_timeout_us(i2c, addr, buf, len, nostop, timeout_us);
    }
    i2c_bus_request_t req = {
        .i2c = i2c,
        .addr = addr,
        .caller = xTaskGetCurrentTaskHandle(),
        .timeout_us = timeout_us,
        .result = 0,
        .op = is_read ? I2C_OP_READ : I2C_OP_WRITE,
        .u.simple = {.buf = buf, .len = len, .nostop = nostop, .is_read = is_read},
    };
    i2c_bus_request_t *preq = &req;
    BaseType_t sent = xQueueSend(s_i2cq, &preq, portMAX_DELAY);
    if (sent != pdTRUE) {
        return -1;
    }
    /* Wait until the bus task completes this request */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return req.result;
}

int i2c_bus_write_timeout_us(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len,
                             bool nostop, uint32_t timeout_us) {
    /* Cast away const for transport; data is not modified by bus task */
    return i2c_bus_submit_simple(i2c, addr, (uint8_t *)src, len, nostop, timeout_us, false);
}

int i2c_bus_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                            uint32_t timeout_us) {
    return i2c_bus_submit_simple(i2c, addr, dst, len, nostop, timeout_us, true);
}

static inline int i2c_bus_submit_write_read(i2c_inst_t *i2c, uint8_t addr, const uint8_t *wbuf,
                                            size_t wlen, uint8_t *rbuf, size_t rlen,
                                            uint32_t timeout_us) {
    if (s_i2cq == NULL) {
        int rc = i2c_write_timeout_us(i2c, addr, wbuf, wlen, true, timeout_us);
        if (rc != (int)wlen)
            return -1;
        rc = i2c_read_timeout_us(i2c, addr, rbuf, rlen, false, timeout_us);
        return rc;
    }
    i2c_bus_request_t req = {
        .i2c = i2c,
        .addr = addr,
        .caller = xTaskGetCurrentTaskHandle(),
        .timeout_us = timeout_us,
        .result = 0,
        .op = I2C_OP_WRITE_READ,
        .u.wr = {.wbuf = wbuf, .wlen = wlen, .rbuf = rbuf, .rlen = rlen},
    };
    i2c_bus_request_t *preq = &req;
    BaseType_t sent = xQueueSend(s_i2cq, &preq, portMAX_DELAY);
    if (sent != pdTRUE) {
        return -1;
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return req.result;
}

/* ---------------- High-level helpers with retries + diagnostics ------------- */

#ifndef I2C_BUS_MAX_RETRIES
#define I2C_BUS_MAX_RETRIES 3
#endif
#ifndef I2C_BUS_RETRY_DELAY_US
#define I2C_BUS_RETRY_DELAY_US 500u
#endif
#ifndef I2C_BUS_TIMEOUT_US
#define I2C_BUS_TIMEOUT_US 50000u
#endif
#ifndef I2C_BUS_DIAG
#define I2C_BUS_DIAG 0
#endif

static inline void i2c_bus_diag_log(const char *op, i2c_inst_t *i2c, uint8_t addr, uint8_t reg,
                                    int result) {
    (void)op;
    (void)i2c;
    (void)addr;
    (void)reg;
    (void)result;
}

bool i2c_bus_write_reg8(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t value,
                        uint32_t timeout_us) {
    uint8_t buf[2] = {reg, value};
    int result = 0;
    for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
        result = i2c_bus_write_timeout_us(i2c, addr, buf, 2, false,
                                          timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
        if (result == 2)
            return true;
        if (attempt == 0) {
            i2c_bus_diag_log("WRITE_REG8", i2c, addr, reg, result);
        }
        TickType_t t = pdMS_TO_TICKS((I2C_BUS_RETRY_DELAY_US + 999u) / 1000u);
        if (t < 1)
            t = 1;
        vTaskDelay(t);
    }
    return false;
}

bool i2c_bus_read_reg8(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t *out,
                       uint32_t timeout_us) {
    if (!out)
        return false;
    int rres = 0;
    for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
        rres = i2c_bus_submit_write_read(i2c, addr, &reg, 1, out, 1,
                                         timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
        if (rres == 1)
            return true;
        if (attempt == 0)
            i2c_bus_diag_log("READ_REG8[wr]", i2c, addr, reg, rres);
        TickType_t t = pdMS_TO_TICKS((I2C_BUS_RETRY_DELAY_US + 999u) / 1000u);
        if (t < 1)
            t = 1;
        vTaskDelay(t);
    }
    return false;
}

bool i2c_bus_write_mem16(i2c_inst_t *i2c, uint8_t addr, uint16_t mem, const uint8_t *src,
                         size_t len, uint32_t timeout_us) {
    if (!src && len)
        return false;
    uint8_t hdr[2] = {(uint8_t)(mem >> 8), (uint8_t)(mem & 0xFF)};
    /* Always send header+data as one contiguous write to preserve semantics */
    int result = 0;
    for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
        uint8_t *buf = (uint8_t *)pvPortMalloc(2 + len);
        if (!buf) {
            return false;
        }
        buf[0] = hdr[0];
        buf[1] = hdr[1];
        if (len)
            memcpy(&buf[2], src, len);
        result = i2c_bus_write_timeout_us(i2c, addr, buf, (size_t)(2 + len), false,
                                          timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
        vPortFree(buf);
        if (result == (int)(2 + len))
            return true;
        if (attempt == 0)
            i2c_bus_diag_log("WRITE_MEM16", i2c, addr, hdr[1], result);
        TickType_t t = pdMS_TO_TICKS((I2C_BUS_RETRY_DELAY_US + 999u) / 1000u);
        if (t < 1)
            t = 1;
        vTaskDelay(t);
    }
    return false;
}

bool i2c_bus_read_mem16(i2c_inst_t *i2c, uint8_t addr, uint16_t mem, uint8_t *dst, size_t len,
                        uint32_t timeout_us) {
    if (!dst && len)
        return false;
    uint8_t hdr[2] = {(uint8_t)(mem >> 8), (uint8_t)(mem & 0xFF)};
    int rres = 0;
    for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
        rres = i2c_bus_submit_write_read(i2c, addr, hdr, 2, dst, len,
                                         timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
        if (rres == (int)len)
            return true;
        if (attempt == 0)
            i2c_bus_diag_log("READ_MEM16[wr]", i2c, addr, hdr[1], rres);
        TickType_t t = pdMS_TO_TICKS((I2C_BUS_RETRY_DELAY_US + 999u) / 1000u);
        if (t < 1)
            t = 1;
        vTaskDelay(t);
    }
    return false;
}

int i2c_bus_write_read(i2c_inst_t *i2c, uint8_t addr, const uint8_t *wsrc, size_t wlen,
                       uint8_t *rdst, size_t rlen, uint32_t timeout_us) {
    return i2c_bus_submit_write_read(i2c, addr, wsrc, wlen, rdst, rlen, timeout_us);
}