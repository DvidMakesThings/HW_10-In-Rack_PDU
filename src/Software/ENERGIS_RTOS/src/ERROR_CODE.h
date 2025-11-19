/**
 * @file src/ERROR_CODE.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup config Configuration Files
 * @brief Configuration files for the Energis PDU firmware.
 * @{
 *
 * @defgroup config01 1. RTOS Configuration
 * @ingroup config
 * @brief Configuration header for ENERGIS PDU firmware.
 * @{
 *
 * @version 2.0.0
 * @date 2025-11-08
 *
 * @details This file contains global configuration settings,
 * peripheral assignments, and logging macros for the ENERGIS PDU
 * firmware project. It contains user-configurable options as well
 * as system constants. It describes GPIO pin assignments, I2C/SPI
 * peripheral usage, ADC channel assignments, and other peripheral
 * configurations.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#define ERRORLOGGER 1

/********************************************************************************
 *                      ERROR CODE SCHEME (16-bit: 0xMSCC)                     *
 ********************************************************************************/

#ifndef ERROR
#define ERROR 1
#endif
#ifndef WARNING
#define WARNING 1
#endif

/**
 * @brief Error severity levels for 16-bit error codes.
 *
 * Severity occupies bits 11..8 of the error code.
 */
typedef enum {
    ERR_SEV_INFO = 0x1,    /**< Informational event (bits 11..8 = 0x1) */
    ERR_SEV_WARNING = 0x2, /**< Warning condition (bits 11..8 = 0x2) */
    ERR_SEV_ERROR = 0x4,   /**< Error condition (bits 11..8 = 0x4) */
    ERR_FATAL_ERROR = 0xF  /**< Fatal error condition (bits 11..8 = 0xF) */
} error_severity_t;

/**
 * @brief Module IDs for 16-bit error codes.
 *
 * Module ID occupies bits 15..12 of the error code.
 */
#define ERR_MOD_INIT 0x1    /**< InitTask module */
#define ERR_MOD_NET 0x2     /**< NetTask module */
#define ERR_MOD_METER 0x3   /**< MeterTask module */
#define ERR_MOD_STORAGE 0x4 /**< StorageTask module */
#define ERR_MOD_BUTTON 0x5  /**< ButtonTask module */
#define ERR_MOD_HEALTH 0x6  /**< HealthTask module */
#define ERR_MOD_LOGGER 0x7  /**< LoggerTask module */
#define ERR_MOD_CONSOLE 0x8 /**< ConsoleTask module */

/********************************************************************************
 *                       FILE IDs (FID) PER MODULE / FILE                       *
 *                  Used in ERR_MAKE_CODE(module, severity, FID, EID)          *
 ********************************************************************************/

/* =========================== INIT MODULE (0x1) ============================ */
/* Files:
 *   src/ENERGIS_RTOS.c
 *   src/tasks/InitTask.c
 */
#define ERR_FID_INIT_MAIN 0x0 /**< ENERGIS_RTOS.c (main entry) */
#define ERR_FID_INITTASK 0xF  /**< InitTask.c */

/* ============================ NET MODULE (0x2) ============================ */
#define ERR_FID_NET_HTTP_CONTROL 0x0      /**< control_handler.c */
#define ERR_FID_NET_HTTP_SERVER 0x1       /**< http_server.c */
#define ERR_FID_NET_HTTP_METRICS 0x2      /**< metrics_handler.c */
#define ERR_FID_NET_HTTP_PAGE_CONTENT 0x3 /**< page_content.c */
#define ERR_FID_NET_HTTP_SETTINGS 0x4     /**< settings_handler.c */
#define ERR_FID_NET_HTTP_STATUS 0x5       /**< status_handler.c */
#define ERR_FID_NET_SNMP_CUSTOM 0x6       /**< snmp_custom.c */
#define ERR_FID_NET_SNMP_NETCTRL 0x7      /**< snmp_networkCtrl.c */
#define ERR_FID_NET_SNMP_OUTLETCTRL 0x8   /**< snmp_outletCtrl.c */
#define ERR_FID_NET_SNMP_POWERMON 0x9     /**< snmp_powerMon.c */
#define ERR_FID_NET_SNMP_VOLTAGEMON 0xA   /**< snmp_voltageMon.c */
#define ERR_FID_NET_ETHERNET 0xB          /**< ethernet_driver.c */
#define ERR_FID_NET_SOCKET 0xC            /**< socket.c */
#define ERR_FID_NET_SOCKET2 0xD           /**< socket.c */
#define ERR_FID_NET_SNMP 0xE              /**< drivers/snmp.c */
#define ERR_FID_NETTASK 0xF               /**< NetTask.c */

/* ========================== METER MODULE (0x3) ============================ */
#define ERR_FID_HLW8032 0x0   /**< HLW8032_driver.c */
#define ERR_FID_METERTASK 0xF /**< MeterTask.c */

/* ========================= STORAGE MODULE (0x4) =========================== */
#define ERR_FID_ST_CALIBRATION 0x0    /**< calibration.c */
#define ERR_FID_ST_CHANNEL_LBL 0x1    /**< channel_labels.c */
#define ERR_FID_ST_ENERGY_MON 0x2     /**< energy_monitor.c */
#define ERR_FID_ST_EVENT_LOG 0x3      /**< event_log.c */
#define ERR_FID_ST_FACTORY_DEFS 0x4   /**< factory_defaults.c */
#define ERR_FID_ST_CAT24 0x5          /**< CAT24C256_driver.c */
#define ERR_FID_ST_NETWORK_CFG 0x6    /**< network.c (storage_submodule) */
#define ERR_FID_ST_STORAGE_COMMON 0x7 /**< storage_common.c */
#define ERR_FID_ST_USER_OUTPUT 0x8    /**< user_output.c */
#define ERR_FID_ST_USER_PREFS 0x9     /**< user_prefs.c */
#define ERR_FID_ST_STORAGETASK 0xF    /**< StorageTask.c */

/* ========================= BUTTON MODULE (0x5) ============================ */
#define ERR_FID_BUTTON_DRV 0x0 /**< button_driver.c */
#define ERR_FID_MCP23017 0x1   /**< MCP23017_driver.c */
#define ERR_FID_MCP23017_2 0x2 /**< MCP23017_driver.c */
#define ERR_FID_BUTTONTASK 0xF /**< ButtonTask.c */

/* ========================= HEALTH MODULE (0x6) ============================ */
#define ERR_FID_CRASHLOG 0x0      /**< crashlog.c */
#define ERR_FID_POWER_MGR 0x1     /**< power_mgr.c */
#define ERR_FID_RTOS_HOOKS 0x2    /**< rtos_hooks.c */
#define ERR_FID_WATCHDOG_WRAP 0x3 /**< wrap_watchdog.c */
#define ERR_FID_HEALTHTASK 0xF    /**< HealthTask.c */

/* ========================= LOGGER MODULE (0x7) ============================ */
#define ERR_FID_HELPERS 0x0    /**< helpers.c */
#define ERR_FID_LOGGERTASK 0xF /**< LoggerTask.c */

/* ======================== CONSOLE MODULE (0x8) ============================ */
#define ERR_FID_CONSOLETASK 0x0  /**< ConsoleTask.c */
#define ERR_FID_CONSOLETASK2 0x1 /**< ConsoleTask.c */
#define ERR_FID_CONSOLETASK3 0x2 /**< ConsoleTask.c */
/* Reserve 0x1..0xF for future console_submodules .c files */

/********************************************************************************
 *                                FUNCTION MACROS                               *
 *                                                                              *
 ********************************************************************************/

/**
 * @brief File ID and Error ID helpers for the CC byte.
 *
 * The CC byte (bits 7..0) is split into:
 * - FID (bits 7..4): File ID within module (0x0-0xF)
 * - EID (bits 3..0): Error ID within that file (0x0-0xF)
 */
#define ERR_FID(fid) ((uint8_t)((fid) & 0x0F))
#define ERR_EID(eid) ((uint8_t)((eid) & 0x0F))
#define ERR_CC(fid, eid) ((uint8_t)((ERR_FID(fid) << 4) | ERR_EID(eid)))

/**
 * @brief Construct a 16-bit error code from module, severity, file ID, and error ID.
 *
 * Format: 0xMSCC where CC = (FID << 4 | EID)
 * - M (bits 15..12): Module ID (0x1-0x8)
 * - S (bits 11..8):  Severity (0x1=INFO, 0x2=WARNING, 0xF=ERROR)
 * - FID (bits 7..4): File ID within module (0x0-0xF)
 * - EID (bits 3..0): Error ID within that file (0x0-0xF)
 *
 * @param module Module ID (4 bits)
 * @param severity Severity level (4 bits)
 * @param fid File ID within module (4 bits)
 * @param eid Error ID within file (4 bits)
 * @return 16-bit error code
 *
 * Example: ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, 0x1, 0x3) = 0x2F13
 */
#define ERR_MAKE_CODE(module, severity, fid, eid)                                                  \
    ((uint16_t)((((module) & 0x0F) << 12) | (((severity) & 0x0F) << 8) | ERR_CC(fid, eid)))

/**
 * @brief Extract File ID from error code.
 * @param code 16-bit error code
 * @return File ID (bits 7..4)
 */
#define ERR_GET_FID(code) ((uint8_t)(((code) >> 4) & 0x0F))

/**
 * @brief Extract Error ID from error code.
 * @param code 16-bit error code
 * @return Error ID (bits 3..0)
 */
#define ERR_GET_EID(code) ((uint8_t)((code) & 0x0F))

/**
 * @brief Extract Module ID from error code.
 * @param code 16-bit error code
 * @return Module ID (bits 15..12)
 */
#define ERR_GET_MODULE(code) ((uint8_t)(((code) >> 12) & 0x0F))

/**
 * @brief Extract Severity from error code.
 * @param code 16-bit error code
 * @return Severity (bits 11..8)
 */
#define ERR_GET_SEVERITY(code) ((uint8_t)(((code) >> 8) & 0x0F))

/**
 * @brief Error-aware logging macros with 16-bit error codes.
 *
 * These macros format and print log messages with error codes but do NOT
 * perform EEPROM access. EEPROM writes must be done separately by calling
 * EEPROM_AppendEventLog() with eepromMtx held.
 */
#if ERROR
#define ERROR_PRINT_CODE(code, fmt, ...)                                                           \
    do {                                                                                           \
        setError(true);                                                                            \
        log_printf_force("[ERROR 0x%04X] " fmt, (uint16_t)(code), ##__VA_ARGS__);                  \
    } while (0)
#else
#define ERROR_PRINT_CODE(code, fmt, ...) ((void)0)
#endif

#if WARNING
#define WARNING_PRINT_CODE(code, fmt, ...)                                                         \
    log_printf_force("[WARNING 0x%04X] " fmt, (uint16_t)(code), ##__VA_ARGS__)
#else
#define WARNING_PRINT_CODE(code, fmt, ...) ((void)0)
#endif
