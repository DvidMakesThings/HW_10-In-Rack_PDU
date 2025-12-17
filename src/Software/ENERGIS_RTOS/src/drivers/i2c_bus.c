/**
 * @file i2c_bus.c
 * @author DvidMakesThings - David Sipos
 * 
 * @version 2.0.0
 * @date 2025-12-17
 *
 * @details
 * Provides a single FIFO-serialized execution path for all I2C transactions
 * across both controllers (i2c0 and i2c1). Callers use synchronous wrappers
 * that submit work to a bus task and block until completion, eliminating
 * interleaving and reducing race conditions under load.
 *
 * Key properties:
 * - One queue for all controllers ensures strict serialization
 * - Synchronous API mirrors Pico SDK semantics but routes through queue
 * - Optional per-op retries with small backoff for transient failures
 * - Minimal diagnostics; noisy debug prints have been removed
 *
 * Thread-safety:
 * - A single queue/task encapsulates controller access
 * - Optional legacy lock/unlock retained for compatibility
 *
 * Usage:
 * - Call I2C_BusInit() once during startup
 * - Use i2c_bus_* helpers instead of direct Pico SDK calls
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"
#include "task.h"

typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
    uint8_t *buf;
    size_t len;
    bool nostop;
    uint32_t timeout_us;
    bool is_read; /* false=write, true=read */
    TaskHandle_t caller;
    int result; /* filled by bus task */
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
                    /* Diagnostic warning removed */
                }
            }
            s_i2c_lock_tick = 0;
            s_i2c_owner = NULL;
        }
        xSemaphoreGive(m);
    }
}

/* -------------------- I2C Bus Manager Implementation ------------------- */

static void i2c_bus_task(void *arg) {
    (void)arg;
    for (;;) {
        i2c_bus_request_t *reqp = NULL;
        if (xQueueReceive(s_i2cq, &reqp, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        int rc;
        if (reqp->is_read) {
            rc = i2c_read_timeout_us(reqp->i2c, reqp->addr, reqp->buf, reqp->len, reqp->nostop,
                                     reqp->timeout_us);
        } else {
            rc = i2c_write_timeout_us(reqp->i2c, reqp->addr, reqp->buf, reqp->len, reqp->nostop,
                                      reqp->timeout_us);
        }
        reqp->result = rc;
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

static inline int i2c_bus_submit(i2c_inst_t *i2c, uint8_t addr, uint8_t *buf, size_t len,
                                 bool nostop, uint32_t timeout_us, bool is_read) {
    if (s_i2cq == NULL) {
        /* Fallback: direct call if not initialized yet */
        return is_read ? i2c_read_timeout_us(i2c, addr, buf, len, nostop, timeout_us)
                       : i2c_write_timeout_us(i2c, addr, buf, len, nostop, timeout_us);
    }
    i2c_bus_request_t req = {
        .i2c = i2c,
        .addr = addr,
        .buf = buf,
        .len = len,
        .nostop = nostop,
        .timeout_us = timeout_us,
        .is_read = is_read,
        .caller = xTaskGetCurrentTaskHandle(),
        .result = 0,
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
    return i2c_bus_submit(i2c, addr, (uint8_t *)src, len, nostop, timeout_us, false);
}

int i2c_bus_read_timeout_us(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len, bool nostop,
                            uint32_t timeout_us) {
    return i2c_bus_submit(i2c, addr, dst, len, nostop, timeout_us, true);
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
    int wres = 0;
    int rres = 0;
    for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
        wres = i2c_bus_write_timeout_us(i2c, addr, &reg, 1, true,
                                        timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
        if (wres != 1) {
            if (attempt == 0)
                i2c_bus_diag_log("READ_REG8[addrphase]", i2c, addr, reg, wres);
        } else {
            rres = i2c_bus_read_timeout_us(i2c, addr, out, 1, false,
                                           timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
            if (rres == 1)
                return true;
            if (attempt == 0)
                i2c_bus_diag_log("READ_REG8[data]", i2c, addr, reg, rres);
        }
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
    /* Build a small stack buffer if len is small, otherwise two writes */
    if (len <= 30) {
        uint8_t buf[2 + 30];
        buf[0] = hdr[0];
        buf[1] = hdr[1];
        if (len)
            memcpy(&buf[2], src, len);
        int result = 0;
        for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
            result = i2c_bus_write_timeout_us(i2c, addr, buf, (size_t)(2 + len), false,
                                              timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
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
    } else {
        /* Write header then data (stop at end) */
        int result = 0;
        for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
            result = i2c_bus_write_timeout_us(i2c, addr, hdr, 2, true,
                                              timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
            if (result != 2) {
                if (attempt == 0)
                    i2c_bus_diag_log("WRITE_MEM16[hdr]", i2c, addr, hdr[1], result);
            } else {
                result = i2c_bus_write_timeout_us(i2c, addr, src, len, false,
                                                  timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
                if (result == (int)len)
                    return true;
                if (attempt == 0)
                    i2c_bus_diag_log("WRITE_MEM16[data]", i2c, addr, hdr[1], result);
            }
            TickType_t t = pdMS_TO_TICKS((I2C_BUS_RETRY_DELAY_US + 999u) / 1000u);
            if (t < 1)
                t = 1;
            vTaskDelay(t);
        }
        return false;
    }
}

bool i2c_bus_read_mem16(i2c_inst_t *i2c, uint8_t addr, uint16_t mem, uint8_t *dst, size_t len,
                        uint32_t timeout_us) {
    if (!dst && len)
        return false;
    uint8_t hdr[2] = {(uint8_t)(mem >> 8), (uint8_t)(mem & 0xFF)};
    int wres = 0, rres = 0;
    for (int attempt = 0; attempt < I2C_BUS_MAX_RETRIES; attempt++) {
        wres = i2c_bus_write_timeout_us(i2c, addr, hdr, 2, true,
                                        timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
        if (wres != 2) {
            if (attempt == 0)
                i2c_bus_diag_log("READ_MEM16[addr]", i2c, addr, hdr[1], wres);
        } else {
            rres = i2c_bus_read_timeout_us(i2c, addr, dst, len, false,
                                           timeout_us ? timeout_us : I2C_BUS_TIMEOUT_US);
            if (rres == (int)len)
                return true;
            if (attempt == 0)
                i2c_bus_diag_log("READ_MEM16[data]", i2c, addr, hdr[1], rres);
        }
        TickType_t t = pdMS_TO_TICKS((I2C_BUS_RETRY_DELAY_US + 999u) / 1000u);
        if (t < 1)
            t = 1;
        vTaskDelay(t);
    }
    return false;
}
