/* File: misc/rtos_hooks.c  (RP2040-safe, non-blocking hooks) */
#include "../CONFIG.h"
#include "hardware/watchdog.h"

/* Non-blocking log macro: prefer your logger, otherwise no-op. */
#ifndef HOOK_LOGE
#ifdef ERROR_PRINT
#define HOOK_LOGE(...) ERROR_PRINT(__VA_ARGS__)
#else
#define HOOK_LOGE(...)                                                                             \
    do {                                                                                           \
    } while (0)
#endif
#endif

/* Reboot helper: RP2040 watchdog only (no NVIC_SystemReset on this platform). */
static inline void hook_reboot_now(void) {
    HOOK_LOGE("[RTOS HOOK]SYSTEM REBOOTS HERE\r\n");
    /* Fast reboot path; avoids CDC/USB dependencies. */
    watchdog_reboot(0, 0, 0);
    /* If something weird prevents immediate reset, force a short watchdog bite. */
    watchdog_enable(1, 1);

    /*
    for (;;) {
        __asm volatile("nop");
    }
    */
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    taskDISABLE_INTERRUPTS();
    HOOK_LOGE("[FATAL] Stack overflow in task: %s\r\n", pcTaskName ? pcTaskName : "?");
    hook_reboot_now();
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    HOOK_LOGE("[FATAL] Malloc failed\r\n");
    hook_reboot_now();
}

/* If configASSERT() is enabled, it maps to vAssertCalled(). Keep it non-blocking. */
void vAssertCalled(const char *file, int line) {
    taskDISABLE_INTERRUPTS();
    HOOK_LOGE("[ASSERT] %s:%d\r\n", file ? file : "?", line);
    hook_reboot_now();
}
