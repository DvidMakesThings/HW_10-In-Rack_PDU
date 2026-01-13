/**
 * @file src/misc/rtos_hooks.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * Implementation of FreeRTOS application hooks and fault handlers. This module
 * captures fault context into watchdog scratch registers for post-mortem analysis
 * and provides scheduler liveness monitoring via Idle task canary.
 *
 * All fault handlers follow a consistent pattern:
 * 1. Write fault signature (0xBEEF + cause code) to scratch[0]
 * 2. Capture minimal context (LR, task handle, etc.) to remaining scratch registers
 * 3. Initiate controlled reboot via Health module
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define RTOS_HOOKS_TAG "[RTOSHKS]"

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
 * @brief Convert FreeRTOS tick count to milliseconds.
 *
 * Helper function that reads the current tick count and converts it to
 * milliseconds based on the configured tick rate.
 *
 * @return Milliseconds since scheduler start
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
    /* Increment liveness canary */
    g_rtos_idle_canary++;

    /* Record execution timestamp */
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
 * @brief FreeRTOS stack overflow hook with fault context capture.
 *
 * Called by FreeRTOS when stack overflow is detected for a task. Captures
 * diagnostic context into watchdog scratch registers and initiates reboot.
 *
 * Fault signature format in watchdog scratch registers:
 * - scratch[0]: 0xBEEF0000 | 0xF2 (StackOverflow signature)
 * - scratch[1]: Function return address
 * - scratch[2]: Offending task handle
 * - scratch[3]: Task stack high-water mark (bytes remaining before overflow)
 *
 * @param xTask Offending task handle
 * @param pcTaskName Task name string pointer (may be invalid post-crash)
 *
 * @return None (function does not return; system reboots)
 *
 * @note Requires configCHECK_FOR_STACK_OVERFLOW > 0 in FreeRTOSConfig.h
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)pcTaskName;

    /* Write fault signature and context to watchdog scratch registers */
    watchdog_hw->scratch[0] = 0xBEEF0000u | 0xF2u;
    watchdog_hw->scratch[1] = (uint32_t)__builtin_return_address(0);
    watchdog_hw->scratch[2] = (uint32_t)xTask;
    watchdog_hw->scratch[3] = (uint32_t)uxTaskGetStackHighWaterMark(xTask);

    /* Initiate controlled reboot for post-mortem analysis */
    Health_RebootNow("RTOS hook");
    for (;;)
        ;
}

/**
 * @brief FreeRTOS heap allocation failure hook with context capture.
 *
 * Called by FreeRTOS when pvPortMalloc() fails to allocate memory. Captures
 * diagnostic context and initiates reboot.
 *
 * Fault signature format in watchdog scratch registers:
 * - scratch[0]: 0xBEEF0000 | 0xF3 (MallocFail signature)
 * - scratch[1]: Function return address
 * - scratch[2]: Current task handle
 * - scratch[3]: Current task stack high-water mark
 *
 * @return None (function does not return; system reboots)
 *
 * @note Requires configUSE_MALLOC_FAILED_HOOK=1 in FreeRTOSConfig.h
 */
void vApplicationMallocFailedHook(void) {
    /* Write fault signature and context to watchdog scratch registers */
    watchdog_hw->scratch[0] = 0xBEEF0000u | 0xF3u;
    watchdog_hw->scratch[1] = (uint32_t)__builtin_return_address(0);
    watchdog_hw->scratch[2] = (uint32_t)xTaskGetCurrentTaskHandle();
    watchdog_hw->scratch[3] = (uint32_t)uxTaskGetStackHighWaterMark(NULL);

    /* Initiate controlled reboot for post-mortem analysis */
    Health_RebootNow("RTOS hook");
    for (;;)
        ;
}

/**
 * @brief FreeRTOS assertion failure hook with context capture.
 *
 * Called when a FreeRTOS assertion fails (via configASSERT macro). Captures
 * diagnostic context and initiates reboot.
 *
 * Fault signature format in watchdog scratch registers:
 * - scratch[0]: 0xBEEF0000 | 0xF4 (Assert signature)
 * - scratch[1]: Function return address
 * - scratch[2]: Source line number where assertion failed
 *
 * @param file Source file name (unused, may be invalid after crash)
 * @param line Line number where assertion failed
 *
 * @return None (function does not return; system reboots)
 *
 * @note Requires configASSERT() macro defined in FreeRTOSConfig.h
 */
void vAssertCalled(const char *file, int line) {
    (void)file;

    /* Write fault signature and context to watchdog scratch registers */
    watchdog_hw->scratch[0] = 0xBEEF0000u | 0xF4u;
    watchdog_hw->scratch[1] = (uint32_t)__builtin_return_address(0);
    watchdog_hw->scratch[2] = (uint32_t)line;

    /* Initiate controlled reboot for post-mortem analysis */
    Health_RebootNow("RTOS hook");
    for (;;)
        ;
}

/**
 * @brief Cortex-M HardFault exception handler with context capture.
 *
 * Assembly trampoline that determines which stack pointer was active at the
 * time of the fault (MSP or PSP) and passes it to the C handler function.
 *
 * The handler captures CPU context and writes it to watchdog scratch registers
 * before initiating a controlled reboot.
 *
 * @return None (function does not return; system reboots)
 *
 * @note This function is naked and contains only assembly code
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

/**
 * @brief HardFault C handler that extracts and logs fault context.
 *
 * Called from the HardFault_Handler assembly trampoline with a pointer to the
 * stacked register frame. Extracts all stacked registers and attempts to log
 * them before entering an infinite wait loop.
 *
 * Exception stack frame layout (sp points to r0):
 * [0]=r0, [1]=r1, [2]=r2, [3]=r3, [4]=r12, [5]=lr, [6]=pc, [7]=xpsr
 *
 * @param sp Pointer to exception stack frame
 *
 * @return None (function does not return; enters infinite WFI loop)
 */
void hardfault_c(uint32_t *sp) {
    /* Extract all stacked registers from exception frame */
    uint32_t r0 = sp[0];
    uint32_t r1 = sp[1];
    uint32_t r2 = sp[2];
    uint32_t r3 = sp[3];
    uint32_t r12 = sp[4];
    uint32_t lr = sp[5];
    uint32_t pc = sp[6];
    uint32_t xpsr = sp[7];

#if ERRORLOGGER
    /* Log fault context if error logging is enabled */
    uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_HEALTH, ERR_FATAL_ERROR, ERR_FID_RTOS_HOOKS, 0x0);
    ERROR_PRINT_CODE(errorcode,
                     "%s pc = % 08lx lr = % 08lx xpsr = % 08lx r0 = % 08lx r1 = % 08lx r2 = % 08lx "
                     "r3 = % 08lx r12 = % 08lx\r\n",
                     RTOS_HOOKS_TAG, (unsigned long)pc, (unsigned long)lr, (unsigned long)xpsr,
                     (unsigned long)r0, (unsigned long)r1, (unsigned long)r2, (unsigned long)r3,
                     (unsigned long)r12);
    Storage_EnqueueErrorCode(errorcode);
#endif

    /* Enter infinite low-power wait loop */
    for (;;)
        __asm volatile("wfi");
}
