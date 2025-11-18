#include "CONFIG.h"

/* -------- safe print -------- */
static void log_err(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
#ifdef ERROR_PRINT
    /* Need a fixed-size buffer to avoid re-entrancy surprises inside printf */
    char buf[256];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    /* Trim */
    if ((size_t)n >= sizeof(buf))
        buf[sizeof(buf) - 1] = 0;
    ERROR_PRINT("%s", buf);
#else
    vprintf(fmt, ap);
    va_end(ap);
#endif
}

/* -------- helpers -------- */
static inline uint32_t rd_lr(void) {
    register uint32_t lr __asm("lr");
    return lr;
}
static inline uint32_t rd_pc(void) {
    uint32_t pc;
    __asm volatile("adr %0, .+0" : "=r"(pc));
    return pc;
}
static inline void stall_forever(void) {
    for (;;)
        __asm volatile("wfi");
}

/* -------- wrapped symbols (linker --wrap=...) -------- */
void __real_watchdog_reboot(unsigned, unsigned, unsigned);
void __wrap_watchdog_reboot(unsigned a, unsigned b, unsigned c) {
    log_err("[WRAP] watchdog_reboot called lr=%08lx a=%u b=%u c=%u\r\n", (unsigned long)rd_lr(), a,
            b, c);
    /* Do NOT reboot here; let you see the caller. */
    stall_forever();
}

void __real_abort(void);
void __wrap_abort(void) {
    log_err("[WRAP] abort() called lr=%08lx\r\n", (unsigned long)rd_lr());
    stall_forever();
}

/* newlib/assert */
void __real___assert_func(const char *file, int line, const char *func, const char *expr);
void __wrap____assert_func(const char *file, int line, const char *func, const char *expr);
void __wrap___assert_func(const char *file, int line, const char *func, const char *expr) {
    log_err("[WRAP] __assert %s:%d %s: %s\r\n", file ? file : "?", line, func ? func : "?",
            expr ? expr : "?");
    stall_forever();
}

/* Pico SDK panic family */
void __real_panic(const char *fmt, ...);
void __wrap_panic(const char *fmt, ...) {
    log_err("[WRAP] panic() lr=%08lx\r\n", (unsigned long)rd_lr());
    /* Print original format safely (optional) */
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        char buf[256];
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        ERROR_PRINT("[WRAP] panic msg: %s\r\n", buf);
    }
    stall_forever();
}

/* hard_assertion_failure(const char *file, int line, const char *func, const char *msg) */
void __real_hard_assertion_failure(const char *, int, const char *, const char *);
void __wrap_hard_assertion_failure(const char *file, int line, const char *func, const char *msg) {
    log_err("[WRAP] hard_assert %s:%d %s: %s\r\n", file ? file : "?", line, func ? func : "?",
            msg ? msg : "?");
    stall_forever();
}

/* invalid_params(const char *func, const char *msg) */
void __real_invalid_params(const char *, const char *);
void __wrap_invalid_params(const char *func, const char *msg) {
    log_err("[WRAP] invalid_params %s: %s (lr=%08lx)\r\n", func ? func : "?", msg ? msg : "?",
            (unsigned long)rd_lr());
    stall_forever();
}

/* reset_usb_boot() – if anything sneaks this in, we’ll see it */
void __real_reset_usb_boot(uint32_t, uint32_t);
void __wrap_reset_usb_boot(uint32_t a, uint32_t b) {
    log_err("[WRAP] reset_usb_boot a=%lu b=%lu lr=%08lx\r\n", (unsigned long)a, (unsigned long)b,
            (unsigned long)rd_lr());
    stall_forever();
}

/* pico_runtime exposes internal reboot helpers in newer SDKs; harmless to wrap even if unused */
void __real_runtime_unreset_core(void);
void __wrap_runtime_unreset_core(void) {
    register unsigned lr __asm("lr");
    ERROR_PRINT("[WRAP] runtime_unreset_core lr=%08x\r\n", lr);
    for (;;)
        __asm volatile("wfi");
}

void __real_runtime_reboot(void);
void __wrap_runtime_reboot(void) {
    register unsigned lr __asm("lr");
    ERROR_PRINT("[WRAP] runtime_reboot lr=%08x\r\n", lr);
    for (;;)
        __asm volatile("wfi");
}

void __real___NVIC_SystemReset(void);
void __wrap___NVIC_SystemReset(void) {
    register unsigned lr __asm("lr");
    ERROR_PRINT("[WRAP] __NVIC_SystemReset lr=%08x\r\n", lr);
    for (;;)
        __asm volatile("wfi");
}

void __real_scb_reboot(void);
void __wrap_scb_reboot(void) {
    register unsigned lr __asm("lr");
    ERROR_PRINT("[WRAP] scb_reboot lr=%08x\r\n", lr);
    for (;;)
        __asm volatile("wfi");
}
