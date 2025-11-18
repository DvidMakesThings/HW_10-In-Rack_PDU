/**
 * @file src/misc/rtos_hooks.c
 * @author DvidMakesThings - David Sipos
 * 
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details FreeRTOS hook implementations, crash breadcrumbs, and 
 * scheduler canaries. 
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/**
 * @file rtos_hooks.c
 * @brief FreeRTOS hook implementations, crash breadcrumbs, and scheduler canaries.
 *
 * This module provides:
 * - Minimal post-mortem breadcrumbs stored in RP2040 watchdog scratch registers on fatal hooks.
 * - An Idle-hook canary that advances whenever the Idle task runs, used to detect scheduler stalls.
 * - Lightweight accessors to read the canary and correlate last Idle timestamp.
 *
 * All routines are non-blocking and safe for RP2040. Reboots use the watchdog path.
 */

#ifndef HOOK_LOGE
#ifdef ERROR_PRINT
#define HOOK_LOGE(...) ERROR_PRINT(__VA_ARGS__)
#else
#define HOOK_LOGE(...)                                                                             \
    do {                                                                                           \
    } while (0)
#endif
#endif

#if (configUSE_IDLE_HOOK != 1)
#warning                                                                                           \
    "configUSE_IDLE_HOOK is not 1 -> vApplicationIdleHook will not run; idle canary will stay 0"
#endif

/**
 * @brief Global scheduler Idle canary incremented from vApplicationIdleHook().
 *
 * Monotonically increases whenever the FreeRTOS Idle task runs. Lack of progress over
 * multiple Health periods indicates a scheduler stall.
 */
volatile uint32_t g_rtos_idle_canary = 0u;

/**
 * @brief Last observed millisecond timestamp from Idle hook.
 *
 * Captures when the Idle task last executed, based on RTOS tick time converted to ms.
 */
volatile uint32_t g_rtos_idle_last_ms = 0u;

/**
 * @brief Return a coarse RTOS time in milliseconds derived from tick count.
 *
 * @return Milliseconds since scheduler start.
 */
static inline uint32_t rtos_now_ms(void) {
    TickType_t t = xTaskGetTickCount();
    return (uint32_t)pdTICKS_TO_MS(t);
}

/**
 * @brief FreeRTOS Idle hook. Increments the scheduler canary and records time.
 *
 * Runs in Idle task context and must never block. Signals scheduler liveness
 * by advancing g_rtos_idle_canary and stamping g_rtos_idle_last_ms.
 */
void vApplicationIdleHook(void) {
    g_rtos_idle_canary++;
    g_rtos_idle_last_ms = rtos_now_ms();
}

/**
 * @brief Read the current Idle canary value.
 *
 * @return Monotonic counter incremented by the Idle hook.
 */
uint32_t RTOS_IdleCanary_Read(void) { return g_rtos_idle_canary; }

/**
 * @brief Read the last millisecond timestamp observed in the Idle hook.
 *
 * @return Milliseconds since scheduler start when Idle last ran.
 */
uint32_t RTOS_IdleCanary_LastMs(void) { return g_rtos_idle_last_ms; }

/**
 * @brief Compute canary delta since a previous sample.
 *
 * @param prev_value Previously sampled canary value.
 * @return The difference (current - prev_value), modulo 32-bit.
 */
uint32_t RTOS_IdleCanary_Delta(uint32_t prev_value) {
    uint32_t cur = g_rtos_idle_canary;
    return cur - prev_value;
}

/**
 * @brief Snapshot the current RTOS time in milliseconds.
 *
 * @return Milliseconds since scheduler start.
 */
uint32_t RTOS_TicksMs(void) { return rtos_now_ms(); }

/**
 * @brief FreeRTOS stack-overflow hook: capture task context and reboot.
 *
 * Stores a StackOverflow signature and minimal context in watchdog scratch registers:
 *  - scratch[0] = 0xBEEF0000 | 0xF2
 *  - scratch[1] = call site LR
 *  - scratch[2] = offending task handle
 *  - scratch[3] = task stack high-water mark
 *
 * @param xTask       Offending task handle.
 * @param pcTaskName  Task name pointer (may be invalid after crash).
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)pcTaskName;
    watchdog_hw->scratch[0] = 0xBEEF0000u | 0xF2u;
    watchdog_hw->scratch[1] = (uint32_t)__builtin_return_address(0);
    watchdog_hw->scratch[2] = (uint32_t)xTask;
    watchdog_hw->scratch[3] = (uint32_t)uxTaskGetStackHighWaterMark(xTask);
    Health_RebootNow("RTOS hook");
    for (;;)
        ;
}

/**
 * @brief FreeRTOS malloc-fail hook: capture cause and reboot.
 *
 * Stores a MallocFail signature and minimal context in watchdog scratch registers:
 *  - scratch[0] = 0xBEEF0000 | 0xF3
 *  - scratch[1] = call site LR
 *  - scratch[2] = current task handle
 *  - scratch[3] = current task stack high-water mark
 */
void vApplicationMallocFailedHook(void) {
    watchdog_hw->scratch[0] = 0xBEEF0000u | 0xF3u;
    watchdog_hw->scratch[1] = (uint32_t)__builtin_return_address(0);
    watchdog_hw->scratch[2] = (uint32_t)xTaskGetCurrentTaskHandle();
    watchdog_hw->scratch[3] = (uint32_t)uxTaskGetStackHighWaterMark(NULL);
    Health_RebootNow("RTOS hook");
    for (;;)
        ;
}

/**
 * @brief Assertion failure hook: capture line marker and reboot.
 *
 * Stores an Assert signature and minimal context in watchdog scratch registers:
 *  - scratch[0] = 0xBEEF0000 | 0xF4
 *  - scratch[1] = call site LR
 *  - scratch[2] = __LINE__ value
 *
 * @param file File name (unused).
 * @param line Line number where the assertion failed.
 */
void vAssertCalled(const char *file, int line) {
    (void)file;
    watchdog_hw->scratch[0] = 0xBEEF0000u | 0xF4u;
    watchdog_hw->scratch[1] = (uint32_t)__builtin_return_address(0);
    watchdog_hw->scratch[2] = (uint32_t)line;
    Health_RebootNow("RTOS hook");
    for (;;)
        ;
}

/**
 * @brief HardFault handler: capture link register and reboot.
 *
 * Stores a HardFault signature and minimal context in watchdog scratch registers:
 *  - scratch[0] = 0xBEEF0000 | 0xF1
 *  - scratch[1] = call site LR
 *  - scratch[2] = current task handle
 */
__attribute__((naked)) void HardFault_Handler(void) {
    __asm volatile("movs r0, #4        \n" /* r0 = 4 */
                   "mov  r1, lr        \n" /* r1 = EXC_RETURN */
                   "tst  r1, r0        \n" /* test bit 2 -> which stack */
                   "beq  1f            \n" /* if 0 -> MSP */
                   "mrs  r0, psp       \n" /* r0 = PSP */
                   "b    hardfault_c   \n"
                   "1:                 \n"
                   "mrs  r0, msp       \n" /* r0 = MSP */
                   "b    hardfault_c   \n");
}

/* sp points to stacked regs: r0 r1 r2 r3 r12 lr pc xPSR */
void hardfault_c(uint32_t *sp) {
    uint32_t r0 = sp[0];
    uint32_t r1 = sp[1];
    uint32_t r2 = sp[2];
    uint32_t r3 = sp[3];
    uint32_t r12 = sp[4];
    uint32_t lr = sp[5];
    uint32_t pc = sp[6];
    uint32_t xpsr = sp[7];

    ERROR_PRINT(
        "[HF] pc=%08lx lr=%08lx xpsr=%08lx r0=%08lx r1=%08lx r2=%08lx r3=%08lx r12=%08lx\r\n",
        (unsigned long)pc, (unsigned long)lr, (unsigned long)xpsr, (unsigned long)r0,
        (unsigned long)r1, (unsigned long)r2, (unsigned long)r3, (unsigned long)r12);

    for (;;)
        __asm volatile("wfi");
}
