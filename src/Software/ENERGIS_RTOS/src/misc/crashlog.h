/**
 * @file src/misc/crashlog.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup misc Miscellaneous Utilities
 * @brief Miscellaneous utility functions and modules.
 * @{
 *
 * @defgroup misc1 1. Crash Log Module
 * @ingroup misc
 * @brief Crash log and fault diagnostics module
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * This module provides crash logging and diagnostics capabilities for the ENERGIS PDU.
 * It captures and retains critical fault information across system resets in a dedicated
 * noinit RAM section with CRC32 integrity checking.
 *
 * The crash log system provides:
 * - HardFault exception context capture (PC, LR, registers)
 * - Reset cause detection and classification
 * - Monotonic boot counter
 * - Watchdog feed telemetry ring buffer
 * - Software reboot tagging for intentional resets
 * - Post-mortem diagnostics output at boot
 *
 * The crash log structure survives soft resets and watchdog reboots, allowing
 * post-mortem analysis of fault conditions. CRC32 validation ensures data integrity.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#pragma once

#include "../CONFIG.h"

/**
 * @name Constants
 * @ingroup misc1
 * @{
 */
/** @brief Retained log ring capacity for watchdog feeds. */
#define CRASH_FEED_RING 16u
/** @} */

/**
 * @brief Reset cause enumeration recorded at early boot.
 * @enum crash_reset_reason_t
 * @ingroup misc1
 * @name Reset Reason Values
 * @{
 */
typedef enum {
    CR_RESET_UNKNOWN = 0u,  /**< Unknown or not captured. */
    CR_RESET_POWERON = 1u,  /**< Power-on or external power cycle. */
    CR_RESET_WATCHDOG = 2u, /**< Hardware watchdog caused reboot. */
    CR_RESET_SOFTWARE = 3u, /**< Software-called reboot/reset. */
    CR_RESET_BROWNOUT = 4u, /**< Brown-out or supply dip. */
    CR_RESET_EXTERNAL = 5u, /**< External reset pin toggled. */
    CR_RESET_DEBUG = 6u     /**< Debug probe reset. */
} crash_reset_reason_t;
/** @} */

/**
 * @brief Crash log structure stored in noinit so it survives resets.
 *
 * This structure contains a validated header with versioning and CRC.
 * Only after a successful header validation are the runtime fields considered valid.
 */
/**
 * @struct crashlog_t
 * @ingroup misc1
 */
typedef struct {
    /* Header with integrity */
    uint32_t hdr_magic;   /**< Magic 'CRLG' to validate retained block. */
    uint16_t hdr_version; /**< Structure version for compatibility. */
    uint16_t hdr_size;    /**< sizeof(crashlog_t) at build time. */
    uint32_t hdr_crc32;   /**< CRC32 of the structure with this field set to 0. */

    /* HardFault capture (valid only if hf_valid is non-zero) */
    uint8_t hf_valid;             /**< Non-zero indicates HardFault registers below are valid. */
    uint8_t rsvd0[3];             /**< Reserved padding. */
    uint32_t pc;                  /**< Program Counter at fault. */
    uint32_t lr;                  /**< Link Register at fault. */
    uint32_t xpsr;                /**< xPSR at fault. */
    uint32_t r0, r1, r2, r3, r12; /**< Stacked registers at fault. */

    /* Reset diagnostics */
    uint32_t reset_raw_bits;           /**< Platform-specific raw reset bits (if captured). */
    crash_reset_reason_t reset_reason; /**< Decoded reset reason. */
    uint32_t boots_counter;            /**< Monotonic boots counter (retained). */

    /* Watchdog feed telemetry as deltas for readability and robustness */
    uint32_t wdt_last_ts_ms;                     /**< Absolute ms of the last recorded feed. */
    uint32_t wdt_feed_delta_ms[CRASH_FEED_RING]; /**< Deltas in ms since previous feed. */
    uint8_t wdt_feed_wr;                         /**< Write index into ring. */
    uint8_t wdt_feed_count; /**< Number of valid entries (<= CRASH_FEED_RING). */
    uint8_t rsvd1[2];       /**< Reserved padding. */

    /* Optional tag for software-initiated reboot */
    char sw_reboot_tag[24]; /**< Short ASCII tag for reason (optional, NUL-terminated). */
} crashlog_t;

/**
 * @name Public API
 * @ingroup misc1
 * @{
 */
/**
 * @brief Print last crash diagnostics and reset reason, then clear HardFault fields.
 *
 * This function should be called once during system initialization after the logger
 * subsystem is ready. It validates the retained crash log header, prints boot diagnostics
 * including reset reason and watchdog feed history, and outputs any captured HardFault
 * context if present.
 *
 * The function waits briefly for logger readiness (300ms timeout with 10ms polling)
 * to ensure diagnostic output is not lost. After printing, HardFault fields are cleared
 * to prevent stale data from being reported on subsequent boots.
 *
 * @return None
 *
 * @note Call this function from the initialization task after logger is initialized.
 * @note The function is non-blocking and uses best-effort output.
 */
void CrashLog_PrintAndClearOnBoot(void);

/**
 * @brief Record a hardware watchdog feed timestamp for telemetry tracking.
 *
 * This function should be called immediately after feeding the hardware watchdog.
 * It maintains a ring buffer of feed deltas (time between consecutive feeds) to
 * help diagnose watchdog-related resets. The ring buffer size is defined by
 * CRASH_FEED_RING.
 *
 * Feed deltas are stored as relative time differences to maximize diagnostic
 * value and handle timestamp wraps gracefully.
 *
 * @param now_ms Monotonic milliseconds timestamp of the feed event.
 *
 * @return None
 *
 * @note This function validates and initializes the crash log header if needed.
 * @note Thread-safe when called from a single context (typically HealthTask).
 */
void CrashLog_RecordWdtFeed(uint32_t now_ms);

/**
 * @brief Capture reset cause at early boot before RTOS initialization.
 *
 * This function must be called very early during system startup, before clocks,
 * RTOS, or any dynamic initialization. It performs the following operations:
 * - Validates the retained crash log header using CRC32
 * - Initializes the structure on first power-on or after corruption
 * - Increments the monotonic boot counter
 * - Reads and decodes platform-specific reset cause bits
 * - Updates the crash log with current reset information
 *
 * The boot counter persists across resets and provides correlation for crash analysis.
 *
 * @return None
 *
 * @note Call this as the first statement in main() before any other initialization.
 * @note This function uses no heap allocation and performs minimal hardware access.
 */
void CrashLog_CaptureResetReasonEarly(void);

/**
 * @brief Tag an intentional software reboot with a descriptive reason string.
 *
 * This function allows software to record a short descriptive tag before initiating
 * a controlled reboot via watchdog. The tag helps distinguish intentional reboots
 * from fault conditions during post-mortem analysis.
 *
 * The tag string is copied into the retained crash log and will be visible in the
 * boot diagnostics output. String is truncated to fit the 24-byte buffer and
 * automatically null-terminated.
 *
 * @param tag Null-terminated ASCII string describing reboot reason.
 *            Pass NULL to clear the tag. Maximum 23 characters (24th byte is reserved
 *            for null terminator).
 *
 * @return None
 *
 * @note Call this immediately before initiating a watchdog reboot.
 * @note Common tags: "User request", "Firmware update", "Network reconfiguration"
 */
void CrashLog_RecordSoftwareRebootTag(const char *tag);

/**
 * @brief HardFault handler C bridge that captures exception context.
 *
 * This function is called from the HardFault exception vector with a pointer to
 * the stacked register frame. It captures the complete CPU context at the time
 * of the fault into the retained crash log structure.
 *
 * Captured registers: r0, r1, r2, r3, r12, lr, pc, xpsr
 *
 * After capturing context, the function marks the HardFault as valid, updates
 * the CRC32, and initiates a controlled reboot. In debug builds, it triggers
 * a breakpoint before rebooting.
 *
 * @param stacked_regs Pointer to exception stack frame containing:
 *                     [0]=r0, [1]=r1, [2]=r2, [3]=r3, [4]=r12, [5]=lr, [6]=pc, [7]=xpsr
 *
 * @return None (function does not return)
 *
 * @note This function is typically called from assembly HardFault_Handler.
 * @note The function never returns; system is rebooted after context capture.
 */
void CrashLog_OnHardFault(uint32_t *stacked_regs);
/** @} */

/* --- Platform hooks (weak) --- */

/**
 * @name Platform Hooks
 * @ingroup misc1
 * @{
 */
/**
 * @brief Platform hook to read raw reset-cause bits from hardware.
 *
 * This weak function reads platform-specific reset cause registers and returns
 * them as a raw bit field. The default implementation supports RP2040/RP2350
 * platforms and reads the watchdog status plus scratch registers to distinguish
 * intentional vs fault-driven watchdog resets.
 *
 * Platforms can override this function to provide custom reset detection logic.
 *
 * @return Platform-specific raw reset bits. For RP2040:
 *         - bit 0: watchdog caused reboot (fault condition)
 *         - bit 1: intentional software reboot
 *
 * @note Default implementation is weak and can be overridden per platform.
 * @note RP2040 implementation checks watchdog scratch registers to distinguish
 *       Health-owned intentional reboots from true watchdog faults.
 */
uint32_t CrashLog_Platform_ReadResetBits(void);

/**
 * @brief Platform hook to decode raw reset bits into standardized reason enum.
 *
 * This weak function interprets the raw reset bits returned by
 * CrashLog_Platform_ReadResetBits() and maps them to the standard
 * crash_reset_reason_t enumeration.
 *
 * The default implementation provides heuristic decoding suitable for RP2040/RP2350:
 * - Watchdog bit set -> CR_RESET_WATCHDOG
 * - Software bit set -> CR_RESET_SOFTWARE
 * - Neither set -> CR_RESET_POWERON
 *
 * Platforms can override this function for custom reset classification.
 *
 * @param raw Raw bits value from CrashLog_Platform_ReadResetBits()
 *
 * @return Decoded reset reason from crash_reset_reason_t enumeration
 *
 * @note Default implementation is weak and can be overridden per platform.
 */
crash_reset_reason_t CrashLog_Platform_DecodeReset(uint32_t raw);
/** @} */

/** @} */
/** @} */