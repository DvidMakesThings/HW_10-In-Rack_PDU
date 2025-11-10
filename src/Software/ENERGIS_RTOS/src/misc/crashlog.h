/* File: misc/crashlog.h */
#pragma once
/**
 * @file crashlog.h
 * @brief Minimal HardFault crash log (PC/LR/xPSR + stacked regs) kept in RAM.
 *
 * Call CrashLog_PrintAndClearOnBoot() at early boot to print last crash.
 */

#include "../CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t magic;
    uint32_t pc;
    uint32_t lr;
    uint32_t xpsr;
    uint32_t r0, r1, r2, r3, r12;
} crashlog_t;

void CrashLog_PrintAndClearOnBoot(void);

/* Handler entry called from vector HardFault_Handler (C version). */
void CrashLog_OnHardFault(uint32_t *stacked_regs);

#ifdef __cplusplus
}
#endif
