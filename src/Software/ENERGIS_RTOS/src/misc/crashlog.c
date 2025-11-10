/* File: misc/crashlog.c */
#include "../CONFIG.h"

/* Log through your non-blocking logger to avoid CDC blocking. */
#ifndef CRASH_LOG
#ifdef ERROR_PRINT
#define CRASH_LOG(...) ERROR_PRINT(__VA_ARGS__)
#else
#define CRASH_LOG(...)                                                                             \
    do {                                                                                           \
    } while (0)
#endif
#endif

/* Use a clear FourCC magic: 'CRSH' = 0x43525348 */
#define CRASH_MAGIC 0x43525348u

#if defined(__GNUC__)
__attribute__((section(".noinit"))) static crashlog_t s_crash;
#else
static crashlog_t s_crash;
#endif

void CrashLog_OnHardFault(uint32_t *sp) {
    /* Stacked regs: r0,r1,r2,r3,r12,lr,pc,xpsr */
    s_crash.magic = CRASH_MAGIC;
    s_crash.r0 = sp[0];
    s_crash.r1 = sp[1];
    s_crash.r2 = sp[2];
    s_crash.r3 = sp[3];
    s_crash.r12 = sp[4];
    s_crash.lr = sp[5];
    s_crash.pc = sp[6];
    s_crash.xpsr = sp[7];

    __asm volatile("" ::: "memory"); /* ensure writes land */

#if defined(NDEBUG)
    CRASH_LOG("[CRASHLOG]SYSTEM REBOOTS HERE\r\n");
    /* Release: reboot immediately so the unit always comes back. */
    watchdog_reboot(0, 0, 0);
    for (;;) {
        __asm volatile("nop");
    }
#else
    /* Debug: try to break; if no debugger, reboot anyway. */
    __asm volatile("bkpt #0");
    watchdog_reboot(0, 0, 0);
    for (;;) {
        __asm volatile("nop");
    }
#endif
}

void CrashLog_PrintAndClearOnBoot(void) {
    /* Nothing to do if no valid crash captured. */
    if (s_crash.magic != CRASH_MAGIC) {
        return;
    }

    /* Try briefly to wait for the logger so we don't stall early boot. */
    {
        extern bool Logger_IsReady(void);
        const uint32_t timeout_ms = 300u; /* hard cap to avoid blocking boot */
        const uint32_t step_ms = 10u;
        uint32_t waited_ms = 0u;

        while (!Logger_IsReady() && waited_ms < timeout_ms) {
#if defined(pdMS_TO_TICKS)
            vTaskDelay(pdMS_TO_TICKS(step_ms));
#else
            /* crude delay if RTOS not running yet */
            for (volatile uint32_t i = 0; i < 3000u; ++i) {
                __asm volatile("nop");
            }
#endif
            waited_ms += step_ms;
        }
    }

    /* Print (best-effort, non-blocking logger expected behind CRASH_LOG). */
    CRASH_LOG("\r\n[CRASH] HardFault captured\r\n");
    CRASH_LOG("  PC=0x%08lx LR=0x%08lx xPSR=0x%08lx\r\n", (unsigned long)s_crash.pc,
              (unsigned long)s_crash.lr, (unsigned long)s_crash.xpsr);
    CRASH_LOG("  r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx\r\n",
              (unsigned long)s_crash.r0, (unsigned long)s_crash.r1, (unsigned long)s_crash.r2,
              (unsigned long)s_crash.r3, (unsigned long)s_crash.r12);

    /* Clear so it won't re-print on next boot. */
    s_crash.magic = 0u;
    __asm volatile("" ::: "memory");
}
