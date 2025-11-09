/**
 * @file NetTask.c
 * @author DvidMakesThings - David Sipos
 * @defgroup tasks06 6. Network Task
 * @ingroup tasks
 * @brief Network task handling HTTP server and W5500 Ethernet controller
 * @{
 * @version 1.1.0
 * @date 2025-11-07
 *
 * @details
 * NetTask owns all W5500 operations. It waits for StorageTask to signal that
 * configuration is ready, reads the network config, brings up the W5500, then
 * runs the web server loop.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* Socket allocation (S0 HTTP, S1 SNMP) */
#ifndef HTTP_SOCKET_NUM
#define HTTP_SOCKET_NUM (0u)
#endif
#ifndef SNMP_SOCKET_NUM
#define SNMP_SOCKET_NUM (1u)
#endif
#ifndef TRAP_SOCKET_NUM
#define TRAP_SOCKET_NUM (2u)
#endif

/* Task handle */
static TaskHandle_t netTaskHandle = NULL;

/* Tag for logging */
#define NET_TASK_TAG "[Net]"

/**
 * @brief Wait until PHY link is up or timeout expires.
 */
static bool wait_for_link_up(uint32_t timeout_ms) {
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    while (w5500_get_link_status() != PHY_LINK_ON) {
        if (xTaskGetTickCount() - start >= timeout)
            return false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

/**
 * @brief Convert StorageTask networkInfo into driver configuration.
 *
 * @param ni Pointer to networkInfo loaded from EEPROM or defaults
 * @return true on success, false on invalid input or driver failure
 *
 * @details
 * This helper applies IP, subnet, gateway, DNS, MAC and DHCP/static mode
 * into the W5500 driver. It also performs the final chip init so that
 * sockets can be used by the HTTP server.
 */
static bool net_apply_config_and_init(const networkInfo *ni) {
    if (!ni)
        return false;

    /* W5500 low level bring-up (GPIO, SPI, reset) */
    if (!w5500_hw_init()) {
        ERROR_PRINT("%s w5500_hw_init failed\r\n", NET_TASK_TAG);
        return false;
    }

    /* Map StorageTask schema to driver and init chip */
    if (!ethernet_apply_network_from_storage(ni)) {
        ERROR_PRINT("%s ethernet_apply_network_from_storage failed\r\n", NET_TASK_TAG);
        return false;
    }

    /* Optional: print current network to logs */
    /* driver prints internally or we can call w5500_print_network if needed */

    return true;
}

/**
 * @brief Network task main function.
 * @param pvParameters Unused
 * @return None
 *
 * @details
 * Boot sequence:
 * 1) Wait for StorageTask to signal CFG_READY.
 * 2) Fetch networkInfo via storage_get_network.
 * 3) Bring up W5500 with that config.
 * 4) Start HTTP server and process requests.
 */
static void NetTask_Function(void *pvParameters) {
    (void)pvParameters;

    INFO_PRINT("%s Task started\r\n", NET_TASK_TAG);

    /* 1) Wait for configuration to be ready */
    while (!storage_wait_ready(5000)) {
        WARNING_PRINT("%s waiting for config ready.\r\n", NET_TASK_TAG);
    }

    /* 2) Read network configuration from StorageTask */
    networkInfo neti;
    if (!storage_get_network(&neti)) {
        ERROR_PRINT("%s storage_get_network failed\r\n", NET_TASK_TAG);
        /* Fallback: use defaults directly from StorageTask helper */
        neti = LoadUserNetworkConfig();
        WARNING_PRINT("%s using fallback network defaults\r\n", NET_TASK_TAG);
    }

    /* 3) Apply config and initialize Ethernet */
    if (!net_apply_config_and_init(&neti)) {
        uint8_t mr = getMR();
        if (mr & MR_PB)
            setMR(mr & ~MR_PB); /* Disable ping block if set */
        ERROR_PRINT("%s Ethernet init failed, entering safe loop\r\n", NET_TASK_TAG);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    INFO_PRINT("%s Ethernet up, starting HTTP server\r\n", NET_TASK_TAG);

    // then start servers
    http_server_init();

    /* 4b) Start SNMP agent (uses sockets defined in CONFIG.h) */
    if (!SNMP_Init(SNMP_SOCKET_NUM, SNMP_PORT_AGENT, TRAP_SOCKET_NUM)) {
        ERROR_PRINT("[SNMP] init failed\r\n");
    } else {
        INFO_PRINT("[SNMP] Agent running on UDP/%u (sock %u)\r\n", (unsigned)SNMP_PORT_AGENT,
                   (unsigned)SNMP_SOCKET_NUM);
    }

    /* 5) Main service loop */
    for (;;) {
        http_server_process(); /* nonblocking */

        /* SNMP service: tick + poll a couple of PDUs per cycle */
        SNMP_Tick10ms();
        (void)SNMP_Poll(2);

        vTaskDelay(pdMS_TO_TICKS(NET_TASK_CYCLE_MS));
    }
}

/**
 * @brief Starts the network task with a deterministic enable gate.
 *
 * @details
 * - Deterministic boot order step 5/6. Waits for Storage to be READY.
 * - If @p enable is false, network is skipped and marked NOT ready.
 * - Task still internally waits for cfg ready (storage_wait_ready) as before.
 *
 * @instructions
 * Call after ButtonTask_Init(true) in your bringup sequence:
 *   NetTask_Init(true);
 * Query readiness via Net_IsReady().
 *
 * @param enable Gate that allows or skips starting this subsystem.
 * @return pdPASS if task created successfully, pdFAIL otherwise.
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

    /* Original body preserved */
    extern void NetTask_Function(void *arg);
    BaseType_t result = xTaskCreate(NetTask_Function, "NetTask", 4096, NULL, 3, &netTaskHandle);
    if (result == pdPASS) {
        INFO_PRINT("%s Task created successfully\r\n", NET_TASK_TAG);
        NET_READY() = true;
    } else {
        ERROR_PRINT("%s Failed to create task\r\n", NET_TASK_TAG);
    }
    return result;
}

/**
 * @brief Network subsystem readiness query.
 *
 * @details
 * READY when the Net task handle exists (robust against latch desync).
 *
 * @return true if netTaskHandle is non-NULL, else false.
 */
bool Net_IsReady(void) { return (netTaskHandle != NULL); }

/**
 * @}
 */
