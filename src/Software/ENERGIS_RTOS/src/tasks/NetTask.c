/**
 * @file src/tasks/NetTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.3.0
 * @date 2025-11-17
 *
 * @details NetTask owns all W5500 operations. It waits for StorageTask to signal that
 * configuration is ready, reads the network config, brings up the W5500, then
 * runs the web server loop.
 *
 * The task is explicitly robust against missing or interrupted Ethernet link:
 * - If the PDU boots without a cable, the rest of the system still runs.
 * - ETH LED blinks while there is no PHY link.
 * - On each link-up transition, the W5500 is fully reinitialized and all
 *   IP-based services (HTTP + SNMP) are restarted to avoid ERR_CONNECTION_REFUSED.
 *
 * Standby mode support:
 * - When system enters STANDBY mode (via Power_EnterStandby()), NetTask
 *   stops all W5500 access, HTTP, and SNMP processing.
 * - The W5500 is held in hardware reset by Power_EnterStandby().
 * - NetTask continues running but only feeds heartbeat and delays.
 * - On exit from STANDBY (via Power_ExitStandby()), NetTask detects the
 *   state transition and calls net_reinit_from_cache() to restore network.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define NET_TASK_TAG "[NET]"

#ifndef HTTP_SOCKET_NUM
/**
 * @brief Socket index used by the HTTP server.
 */
#define HTTP_SOCKET_NUM (0u)
#endif

#ifndef SNMP_SOCKET_NUM
/**
 * @brief Socket index used by the SNMP agent.
 */
#define SNMP_SOCKET_NUM (1u)
#endif

#ifndef TRAP_SOCKET_NUM
/**
 * @brief Socket index used by the SNMP trap sender.
 */
#define TRAP_SOCKET_NUM (2u)
#endif

/**
 * @brief Handle to the network FreeRTOS task.
 */
static TaskHandle_t netTaskHandle = NULL;

/**
 * @brief Cached network configuration for W5500 reinitialization.
 *
 * @details
 * Once StorageTask provides the persistent network configuration, it is stored
 * in this global so that any later PHY link-up event can trigger a full W5500
 * reconfiguration without re-querying StorageTask.
 */
static networkInfo s_net_cfg;

/**
 * @brief Last observed PHY link status.
 *
 * @details
 * Used to detect link-up and link-down transitions; changes in this value
 * drive ETH LED mode and W5500 reinitialization.
 */
static w5500_PhyLink s_last_link = PHY_LINK_OFF;

/**
 * @brief Flag indicating whether ETH LED should blink (link down).
 *
 * @details
 * When true, NetTask toggles ETH LED at a fixed rate while no PHY link is
 * present. When false, ETH LED is driven solid according to s_eth_led_state.
 */
static bool s_eth_led_blink = false;

/**
 * @brief Timestamp of last ETH LED toggle for blinking (ms since boot).
 */
static uint32_t s_eth_led_blink_last_ms = 0;

/**
 * @brief Current software state of ETH LED (true = ON, false = OFF).
 */
static bool s_eth_led_state = false;

/**
 * @brief Last observed power state for transition detection.
 */
static power_state_t s_last_power_state = PWR_STATE_RUN;

/**
 * @brief Wait until PHY link is up or timeout expires.
 *
 * @param timeout_ms Timeout in milliseconds.
 * @return true if link is up before timeout, false if timeout occurred.
 *
 * @details
 * Polls the W5500 PHY link status at 100 ms cadence, feeding NetTask's health
 * heartbeat while waiting. Intended for short guards around reinit, not long
 * blocking waits.
 */
static bool wait_for_link_up(uint32_t timeout_ms) {
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    while (w5500_get_link_status() != PHY_LINK_ON) {
        if (xTaskGetTickCount() - start >= timeout) {
            return false;
        }
        Health_Heartbeat(HEALTH_ID_NET);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

/**
 * @brief Convert StorageTask networkInfo into driver configuration and init HW.
 *
 * @param ni Pointer to networkInfo loaded from EEPROM or defaults.
 * @return true on success, false on unrecoverable hardware failure.
 *
 * @details
 * This helper applies IP, subnet, gateway, DNS, MAC and DHCP/static mode
 * into the W5500 driver and performs the low-level hardware bring-up.
 *
 * Link presence is *not* treated as fatal:
 * - If W5500 hardware init fails, the function returns false and NetTask
 *   enters a safe loop.
 * - If network application (ethernet_apply_network_from_storage) fails due to
 *   missing PHY link, the error is logged but the function still returns true
 *   so the rest of the PDU can operate without Ethernet.
 */
static bool net_apply_config_and_init(const networkInfo *ni) {
    if (!ni) {
        return false;
    }

    /* W5500 low level bring-up (GPIO, SPI, reset) */
    if (!w5500_hw_init()) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NETTASK, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s w5500_hw_init failed\r\n", NET_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return false;
    }
    Health_Heartbeat(HEALTH_ID_NET);

    /* Map StorageTask schema to driver and init chip.
     * Failure here typically means "no link" -> log and continue;
     * the link supervisor in the main loop will reinit on link-up.
     */
    if (!ethernet_apply_network_from_storage(ni)) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0x1);
        WARNING_PRINT_CODE(errorcode,
                           "%s Network configuration failed (likely no link), "
                           "continuing without Ethernet\r\n",
                           NET_TASK_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
    }
    Health_Heartbeat(HEALTH_ID_NET);

    return true;
}

/**
 * @brief Start all IP-based services (HTTP server + SNMP agent).
 *
 * @return None.
 *
 * @details
 * This function (re)opens all sockets on the W5500 side by calling the
 * HTTP server and SNMP initialization entry points. It is safe to call
 * after a chip reset and is used both at initial bring-up and after each
 * link-up-triggered reinitialization.
 */
static void net_start_services(void) {
    /* HTTP server on dedicated socket */
    http_server_init();
    INFO_PRINT("%s HTTP server initialized\r\n", NET_TASK_TAG);
    Health_Heartbeat(HEALTH_ID_NET);

    /* SNMP agent + trap */
    if (!SNMP_Init(SNMP_SOCKET_NUM, SNMP_PORT_AGENT, TRAP_SOCKET_NUM)) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NETTASK, 0x2);
        ERROR_PRINT_CODE(errorcode, "%s [SNMP] init failed\r\n", NET_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
    } else {
        INFO_PRINT("[SNMP] Agent running on UDP/%u (sock %u)\r\n", (unsigned)SNMP_PORT_AGENT,
                   (unsigned)SNMP_SOCKET_NUM);
    }
    Health_Heartbeat(HEALTH_ID_NET);
}

/**
 * @brief Reinitialize W5500 using cached network configuration on link-up.
 *
 * @return true on success, false on failure.
 *
 * @details
 * Triggered when a PHY link-up event is detected. Performs a software reset
 * of the W5500, waits briefly for link confirmation, reapplies the cached
 * network configuration and restarts all IP-based services.
 *
 * This guarantees a clean link, socket and server state after any interruption,
 * preventing ERR_CONNECTION_REFUSED when the cable is unplugged and replugged.
 */
static bool net_reinit_from_cache(void) {
    /* Software reset the chip */
    w5500_sw_reset();
    Health_Heartbeat(HEALTH_ID_NET);

    /* Guard: ensure the PHY link is actually up (short, non-fatal wait) */
    (void)wait_for_link_up(1000);

    /* Reapply stored network configuration */
    if (!ethernet_apply_network_from_storage(&s_net_cfg)) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0x1);
        WARNING_PRINT_CODE(errorcode, "%s W5500 reinit from cached config failed\r\n",
                           NET_TASK_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
        return false;
    }
    Health_Heartbeat(HEALTH_ID_NET);

    /* Restart HTTP + SNMP so sockets listen again after reset */
    net_start_services();

    return true;
}

/**
 * @brief Network task main function.
 *
 * @param pvParameters Unused.
 * @return None (task function never returns).
 *
 * @details
 * Boot sequence:
 * 1) Wait for StorageTask to signal CFG_READY.
 * 2) Fetch networkInfo via storage_get_network and cache it in s_net_cfg.
 * 3) Bring up W5500 with that config (independent of link presence).
 * 4) Start HTTP server and SNMP agent.
 * 5) Run service loop with:
 *      - Power state supervision (detect STANDBY mode).
 *      - Link supervision (detect plug/unplug).
 *      - ETH LED control (steady ON when linked, blinking when no link).
 *      - Full W5500 + service reinitialization on each link-up event.
 *      - HTTP + SNMP processing with health monitoring.
 *      - In STANDBY: skip all W5500/HTTP/SNMP operations, only heartbeat.
 */
static void NetTask_Function(void *pvParameters) {
    (void)pvParameters;

    ECHO("%s Task started\r\n", NET_TASK_TAG);

    /* 1) Wait for configuration to be ready */
    static uint32_t hb_net_ms = 0;
    while (!storage_wait_ready(5000)) {
        uint32_t __now = to_ms_since_boot(get_absolute_time());
        if ((__now - hb_net_ms) >= NETTASKBEAT_MS) {
            hb_net_ms = __now;
            Health_Heartbeat(HEALTH_ID_NET);
        }
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0x3);
        WARNING_PRINT_CODE(errorcode, "%s waiting for config ready.\r\n", NET_TASK_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    Health_Heartbeat(HEALTH_ID_NET);

    /* 2) Read network configuration from StorageTask and cache it */
    if (!storage_get_network(&s_net_cfg)) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NETTASK, 0x3);
        ERROR_PRINT_CODE(errorcode, "%s storage_get_network failed\r\n", NET_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        /* Fallback: use defaults directly from StorageTask helper */
        s_net_cfg = LoadUserNetworkConfig();
        errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0x4);
        WARNING_PRINT_CODE(errorcode, "%s using fallback network defaults\r\n", NET_TASK_TAG);
        Storage_EnqueueWarningCode(errorcode);
    }
    Health_Heartbeat(HEALTH_ID_NET);

    /* 3) Apply config and initialize Ethernet hardware (but don't depend on link) */
    if (!net_apply_config_and_init(&s_net_cfg)) {
        uint8_t mr = getMR();
        if (mr & MR_PB) {
            setMR(mr & ~MR_PB);
        }
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NETTASK, 0x4);
        ERROR_PRINT_CODE(errorcode, "%s Ethernet HW init failed, entering safe loop\r\n",
                         NET_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif

        for (;;) {
            uint32_t __now = to_ms_since_boot(get_absolute_time());
            if ((__now - hb_net_ms) >= 250U) {
                hb_net_ms = __now;
                Health_Heartbeat(HEALTH_ID_NET);
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    /* Initial link evaluation and ETH LED state */
    s_last_link = w5500_get_link_status();
    if (s_last_link == PHY_LINK_ON) {
        s_eth_led_blink = false;
        s_eth_led_state = true;
        setNetworkLink(true);
        INFO_PRINT("%s Initial PHY link detected, ETH LED ON\r\n", NET_TASK_TAG);
    } else {
        s_eth_led_blink = true;
        s_eth_led_state = false;
        setNetworkLink(false);
        s_eth_led_blink_last_ms = to_ms_since_boot(get_absolute_time());
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0x5);
        WARNING_PRINT_CODE(errorcode, "%s No PHY link at startup\r\n", NET_TASK_TAG);
        Storage_EnqueueWarningCode(errorcode);
#endif
    }

    INFO_PRINT("%s Ethernet HW up, starting services\r\n", NET_TASK_TAG);

    /* 4) Start HTTP + SNMP services on top of configured W5500 */
    net_start_services();

    /* 5) Main service loop */
    for (;;) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        /* 5.0) Power state supervision */
        power_state_t cur_pwr_state = Power_GetState();

        /* Detect transition from STANDBY to RUN */
        if (s_last_power_state == PWR_STATE_STANDBY && cur_pwr_state == PWR_STATE_RUN) {
            INFO_PRINT("%s Exiting STANDBY, reinitializing network\r\n", NET_TASK_TAG);
            /* W5500 RESET was released by Power_ExitStandby(), now reinit */
            vTaskDelay(pdMS_TO_TICKS(100)); /* Brief delay after reset release */
            (void)net_reinit_from_cache();
            s_last_link = PHY_LINK_OFF; /* Force link redetection */
        }
        s_last_power_state = cur_pwr_state;

        /* If in STANDBY mode, skip all W5500/HTTP/SNMP operations */
        if (cur_pwr_state == PWR_STATE_STANDBY) {
            /* Heartbeat at reduced rate to keep HealthTask happy */
            if ((now_ms - hb_net_ms) >= 500U) {
                hb_net_ms = now_ms;
                Health_Heartbeat(HEALTH_ID_NET);
            }
            /* Long delay to minimize CPU usage in standby */
            vTaskDelay(pdMS_TO_TICKS(300));
            continue;
        }

        /* ===== Normal RUN mode operation from here ===== */

        /* 5.1) PHY link supervision and ETH LED control */
        w5500_PhyLink cur_link = w5500_get_link_status();
        if (cur_link != s_last_link) {
            if (cur_link == PHY_LINK_ON) {
                INFO_PRINT("%s PHY link UP detected, full W5500 reinit\r\n", NET_TASK_TAG);
                (void)net_reinit_from_cache();
                s_eth_led_blink = false;
                s_eth_led_state = true;
                setNetworkLink(true);
            } else {
#if ERRORLOGGER
                uint16_t errorcode =
                    ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_WARNING, ERR_FID_NETTASK, 0x6);
                WARNING_PRINT_CODE(errorcode, "%s PHY link DOWN detected\r\n", NET_TASK_TAG);
                Storage_EnqueueWarningCode(errorcode);
#endif

                s_eth_led_blink = true;
                s_eth_led_state = false;
                setNetworkLink(false);
                s_eth_led_blink_last_ms = now_ms;
            }
            s_last_link = cur_link;
        }

        if (s_eth_led_blink) {
            /* Blink ETH LED while link is down (about 1 Hz, 50% duty) */
            if ((now_ms - s_eth_led_blink_last_ms) >= 500U) {
                s_eth_led_blink_last_ms = now_ms;
                s_eth_led_state = !s_eth_led_state;
                setNetworkLink(s_eth_led_state);
            }
        }

        /* 5.2) NetTask heartbeat */
        if ((now_ms - hb_net_ms) >= 250U) {
            hb_net_ms = now_ms;
            Health_Heartbeat(HEALTH_ID_NET);
        }

        /* 5.3) HTTP service with cooperative pacing */
        uint32_t t0 = to_ms_since_boot(get_absolute_time());
        http_server_process();
        uint32_t dt = to_ms_since_boot(get_absolute_time()) - t0;
        if (dt > 750U) {
            Health_RecordBlocked("http_server_process", dt);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
        taskYIELD();

        /* 5.4) SNMP service */
        t0 = to_ms_since_boot(get_absolute_time());
        SNMP_Tick10ms();
        (void)SNMP_Poll(2);
        dt = to_ms_since_boot(get_absolute_time()) - t0;
        if (dt > 750U) {
            Health_RecordBlocked("snmp_poll", dt);
        }

        Health_Heartbeat(HEALTH_ID_NET);
        vTaskDelay(pdMS_TO_TICKS(NET_TASK_CYCLE_MS));
    }
}

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

/**
 * @brief Initialize and start the network task.
 *
 * @param enable If false, NetTask is not created but the call returns pdPASS.
 * @return pdPASS on successful task creation or when disabled, error code otherwise.
 *
 * @details
 * Performs a deterministic gate on StorageTask readiness, then creates the
 * NetTask FreeRTOS task. The internal READY flag is updated only on success.
 */
BaseType_t NetTask_Init(bool enable) {
    /* TU-local READY flag accessor (no file-scope globals added). */
    static volatile bool ready_val = false;
#define NET_READY() (ready_val)

    NET_READY() = false;

    if (!enable) {
        return pdPASS;
    }

    /* Gate on Storage readiness deterministically */
    extern bool Storage_IsReady(void);
    TickType_t const t0 = xTaskGetTickCount();
    TickType_t const deadline = t0 + pdMS_TO_TICKS(5000);
    while (!Storage_IsReady() && xTaskGetTickCount() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Create the task with a handle name that matches InitTask registration */
    BaseType_t result =
        xTaskCreate(NetTask_Function, "Net", 4096, NULL, NETTASK_PRIORITY, &netTaskHandle);
    if (result == pdPASS) {
        INFO_PRINT("%s Task created successfully\r\n", NET_TASK_TAG);
        NET_READY() = true;
    } else {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NETTASK, 0x5);
        ERROR_PRINT_CODE(errorcode, "%s Failed to create task\r\n", NET_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
    }
    return result;
}

/**
 * @brief Network subsystem readiness query.
 *
 * @return true if NetTask has been successfully created, false otherwise.
 */
bool Net_IsReady(void) { return (netTaskHandle != NULL); }