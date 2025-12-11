/**
 * @file SwitchTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.1
 * @date 2025-12-10
 *
 * @details
 * FreeRTOS task that owns all MCP23017 relay operations. Provides non-blocking
 * API for all other tasks to control relays without causing I2C bus contention
 * or watchdog starvation during network stress testing.
 *
 * Architecture:
 * - Single task ownership of MCP relay hardware (mcp_set_channel_state)
 * - All other tasks use Switch_* API which queues commands
 * - Cached state provides instant reads without I2C access
 * - Periodic hardware sync ensures cache accuracy
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define SWITCH_TASK_TAG "[SWITCHTASK]"

/* External storage gate (provided by StorageTask module) */
extern bool Storage_IsReady(void);

/* ==================== Module State ==================== */

/** @brief Command queue handle */
static QueueHandle_t s_switch_queue = NULL;

/** @brief Cached channel states (bit N = channel N, 1=ON, 0=OFF) */
static volatile uint8_t s_channel_states = 0x00;

/** @brief Initialization flag */
static volatile bool s_initialized = false;

/** @brief Last hardware sync timestamp */
static volatile uint32_t s_last_hw_sync_ms = 0;

/* ####################### INTERNAL HELPERS ############################# */

/**
 * @brief Get current milliseconds since boot.
 *
 * @return Milliseconds timestamp.
 */
static inline uint32_t now_ms(void) { return (uint32_t)to_ms_since_boot(get_absolute_time()); }

/**
 * @brief Synchronize cached state from hardware (read all channels).
 *
 * Reads the actual relay state from MCP hardware and updates the cache.
 * This function performs blocking I2C operations and should only be
 * called from SwitchTask context.
 *
 * @return None
 */
static void sync_state_from_hw(void) {
    mcp23017_t *rel = mcp_relay();
    if (!rel) {
        return;
    }

    uint8_t new_state = 0x00;
    for (uint8_t ch = 0; ch < 8; ch++) {
        uint8_t pin_val = mcp_read_pin(rel, ch);
        if (pin_val) {
            new_state |= (1u << ch);
        }
    }

    s_channel_states = new_state;
    s_last_hw_sync_ms = now_ms();
}

/**
 * @brief Execute a single channel set operation.
 *
 * Performs the actual I2C operation to set the relay state AND immediately
 * updates the display LED. This is SAFETY CRITICAL - the operator depends
 * on display LEDs matching relay state with zero latency.
 *
 * @param channel Channel number (0-7)
 * @param state Desired state (1=ON, 0=OFF)
 * @return true on success, false on failure
 */
static bool exec_set_channel(uint8_t channel, uint8_t state) {
    if (channel >= 8) {
        return false;
    }

    /* Normalize state to 0/1 */
    uint8_t val = state ? 1u : 0u;

    /* SAFETY CRITICAL: mcp_set_channel_state() updates BOTH relay AND display
     * LED in one operation - display MUST follow relay with zero latency! */
    bool ok = mcp_set_channel_state(channel, val);

    /* Update cached state to match what we just wrote to hardware */
    if (ok) {
        if (val) {
            s_channel_states |= (1u << channel);
        } else {
            s_channel_states &= ~(1u << channel);
        }
    }

    return ok;
}

/**
 * @brief Execute all-on operation with pacing.
 *
 * @return None
 */
static void exec_all_on(void) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        exec_set_channel(ch, 1);
        if (ch < 7) {
            vTaskDelay(pdMS_TO_TICKS(SWITCH_CMD_PACE_MS));
        }
    }
}

/**
 * @brief Execute all-off operation with pacing.
 *
 * @return None
 */
static void exec_all_off(void) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        exec_set_channel(ch, 0);
        if (ch < 7) {
            vTaskDelay(pdMS_TO_TICKS(SWITCH_CMD_PACE_MS));
        }
    }
}

/**
 * @brief Execute set-mask operation with pacing.
 *
 * @param mask Bitmask where bit N = channel N state
 * @return None
 */
static void exec_set_mask(uint8_t mask) {
    for (uint8_t ch = 0; ch < 8; ch++) {
        uint8_t want = (mask & (1u << ch)) ? 1u : 0u;
        uint8_t have = (s_channel_states & (1u << ch)) ? 1u : 0u;
        if (want != have) {
            exec_set_channel(ch, want);
            vTaskDelay(pdMS_TO_TICKS(SWITCH_CMD_PACE_MS));
        }
    }
}

/**
 * @brief Execute masked write on relay MCP Port B.
 *
 * Performs a low-level masked write on Port B of the relay MCP23017.
 * This is used by the HLW8032 driver to control the MUX A/B/C/EN lines.
 *
 * @param mask Bitmask of Port B bits to modify.
 * @param value New values for the masked bits.
 * @return None
 */
static void exec_set_relay_portb_mask(uint8_t mask, uint8_t value) {
    if (mask == 0u) {
        return;
    }

    mcp23017_t *rel = mcp_relay();
    if (!rel) {
        return;
    }

    /* Port 1 = MCP23017 Port B */
    mcp_write_mask(rel, 1, mask, value);
}

/**
 * @brief Process a single command from the queue.
 *
 * @param cmd Pointer to command structure
 * @return None
 */
static void process_command(const switch_cmd_t *cmd) {
    switch (cmd->type) {
    case SWITCH_CMD_SET_CHANNEL:
        exec_set_channel(cmd->channel, cmd->state);
        break;

    case SWITCH_CMD_TOGGLE:
        /* IMPORTANT: cmd->state contains the PRE-COMPUTED desired new state.
         * Do NOT re-read s_channel_states here - it was already updated by
         * the optimistic cache update in Switch_Toggle(). Using cmd->state
         * ensures we set the correct value regardless of timing. */
        if (cmd->channel < 8) {
            exec_set_channel(cmd->channel, cmd->state);
        }
        break;

    case SWITCH_CMD_ALL_ON:
        exec_all_on();
        break;

    case SWITCH_CMD_ALL_OFF:
        exec_all_off();
        break;

    case SWITCH_CMD_SET_MASK:
        exec_set_mask(cmd->mask);
        break;

    case SWITCH_CMD_SYNC_FROM_HW:
        sync_state_from_hw();
        break;

    case SWITCH_CMD_SET_RELAY_PORTB_MASK:
        /* channel unused, state = value, mask = port B mask */
        exec_set_relay_portb_mask(cmd->mask, cmd->state);
        break;

    default:
        break;
    }
}

/* ####################### MAIN TASK FUNCTION ########################### */

/**
 * @brief SwitchTask main loop.
 *
 * Processes queued relay commands and periodically syncs state from hardware.
 *
 * @param param Unused task parameter.
 * @return None (never returns)
 */
static void vSwitchTask(void *param) {
    (void)param;

    /* Initial hardware sync */
    sync_state_from_hw();
    INFO_PRINT("%s Initial state: 0x%02X\r\n", SWITCH_TASK_TAG, s_channel_states);

    const TickType_t scan_ticks = pdMS_TO_TICKS(SWITCH_TASK_PERIOD_MS);
    uint32_t hb_switch_ms = now_ms();

    for (;;) {
        uint32_t now = now_ms();

        /* Heartbeat */
        if ((now - hb_switch_ms) >= (uint32_t)SWITCH_TASK_PERIOD_MS * 2) {
            hb_switch_ms = now;
            Health_Heartbeat(HEALTH_ID_SWITCH);
        }

        /* Process up to 4 commands per iteration for good responsiveness.
         * Each command does immediate relay+display update via mcp_set_channel_state().
         * Small delay between commands for cooperative scheduling. */
        uint8_t cmds_processed = 0;
        switch_cmd_t cmd;
        while (cmds_processed < 4 && xQueueReceive(s_switch_queue, &cmd, 0) == pdTRUE) {
            process_command(&cmd);
            cmds_processed++;

            /* Brief yield between commands to let other tasks run */
            if (cmds_processed < 4 && uxQueueMessagesWaiting(s_switch_queue) > 0) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }

        /* Periodic hardware sync to verify cache accuracy */
        if ((now - s_last_hw_sync_ms) >= SWITCH_HW_SYNC_INTERVAL_MS) {
            sync_state_from_hw();
        }

        vTaskDelay(scan_ticks);
    }
}

/* ####################### PUBLIC API IMPLEMENTATION #################### */

/**
 * @brief Initialize and start the SwitchTask.
 *
 * @param enable Set true to create/start; false to skip deterministically.
 * @return pdPASS on success (or when skipped), pdFAIL on error/timeout.
 */
BaseType_t SwitchTask_Init(bool enable) {
    s_initialized = false;

    if (!enable) {
        return pdPASS;
    }

    /* Bring-up gate: wait for Storage config to be ready */
    TickType_t t0 = xTaskGetTickCount();
    const TickType_t to = pdMS_TO_TICKS(SWITCH_WAIT_STORAGE_READY_MS);
    while (!Storage_IsReady()) {
        if ((xTaskGetTickCount() - t0) >= to) {
#if ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_SWITCHTASK, 0x1);
            ERROR_PRINT_CODE(errorcode,
                             "%s Storage not ready within timeout, cannot start SwitchTask\r\n",
                             SWITCH_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
            return pdFAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Create command queue */
    s_switch_queue = xQueueCreate(SWITCH_CMD_QUEUE_SIZE, sizeof(switch_cmd_t));
    if (!s_switch_queue) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_SWITCHTASK, 0x2);
        ERROR_PRINT_CODE(errorcode, "%s Failed to create command queue\r\n", SWITCH_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return pdFAIL;
    }

    /* Initialize cached state from hardware */
    sync_state_from_hw();

    /* Create the task */
    if (xTaskCreate(vSwitchTask, "SwitchTask", 1024, NULL, SWITCHTASK_PRIORITY, NULL) != pdPASS) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_BUTTON, ERR_SEV_ERROR, ERR_FID_SWITCHTASK, 0x3);
        ERROR_PRINT_CODE(errorcode, "%s Task create failed\r\n", SWITCH_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        vQueueDelete(s_switch_queue);
        s_switch_queue = NULL;
        return pdFAIL;
    }

    s_initialized = true;
    INFO_PRINT("%s Initialized (queue=%u, pace=%ums)\r\n", SWITCH_TASK_TAG, SWITCH_CMD_QUEUE_SIZE,
               SWITCH_CMD_PACE_MS);
    return pdPASS;
}

/**
 * @brief Check if SwitchTask is ready for commands.
 *
 * @return true if initialized and ready, false otherwise.
 */
bool Switch_IsReady(void) { return s_initialized; }

/* ---------- Non-blocking State Reads ---------- */

/**
 * @brief Get current relay channel state (instant, non-blocking).
 *
 * @param channel Channel number (0-7)
 * @param out_state Pointer to receive current state (true=ON, false=OFF)
 * @return true on success, false if channel invalid or out_state is NULL
 */
bool Switch_GetState(uint8_t channel, bool *out_state) {
    if (channel >= 8 || !out_state) {
        return false;
    }

    /* Atomic read of cached state (no I2C access) */
    *out_state = (s_channel_states & (1u << channel)) != 0;
    return true;
}

/**
 * @brief Get all 8 channel states as a bitmask (instant, non-blocking).
 *
 * @param out_mask Pointer to receive 8-bit state mask (bit=1 means ON)
 * @return true on success, false if out_mask is NULL
 */
bool Switch_GetAllStates(uint8_t *out_mask) {
    if (!out_mask) {
        return false;
    }

    /* Atomic read of cached state */
    *out_mask = s_channel_states;
    return true;
}

/* ---------- Non-blocking State Writes ---------- */

/**
 * @brief Set single relay channel state (non-blocking).
 *
 * @param channel Channel number (0-7)
 * @param state Desired state (true=ON, false=OFF)
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full or invalid params
 */
bool Switch_SetChannel(uint8_t channel, bool state, uint32_t timeout_ms) {
    if (!s_initialized || !s_switch_queue) {
        return false;
    }

    if (channel >= 8) {
        return false;
    }

    switch_cmd_t cmd = {
        .type = SWITCH_CMD_SET_CHANNEL, .channel = channel, .state = state ? 1u : 0u, .mask = 0};

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(s_switch_queue, &cmd, ticks) == pdTRUE) {
        /* Optimistic cache update: immediately reflect intended state so that
         * subsequent reads return the expected value (read-your-writes consistency).
         * This prevents timing issues where SNMP GET returns stale data after SET. */
        if (state) {
            s_channel_states |= (1u << channel);
        } else {
            s_channel_states &= ~(1u << channel);
        }
        return true;
    }
    return false;
}

/**
 * @brief Toggle single relay channel (non-blocking).
 *
 * @param channel Channel number (0-7)
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full or invalid params
 */
bool Switch_Toggle(uint8_t channel, uint32_t timeout_ms) {
    if (!s_initialized || !s_switch_queue) {
        return false;
    }

    if (channel >= 8) {
        return false;
    }

    /* CRITICAL: Compute new state BEFORE optimistic update, store in cmd.state.
     * This avoids the race condition where process_command() re-reads the
     * already-toggled cache and inverts the operation. */
    uint8_t current = (s_channel_states & (1u << channel)) ? 1u : 0u;
    uint8_t new_state = current ? 0u : 1u;

    switch_cmd_t cmd = {.type = SWITCH_CMD_TOGGLE,
                        .channel = channel,
                        .state = new_state, /* Pre-computed desired state */
                        .mask = 0};

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(s_switch_queue, &cmd, ticks) == pdTRUE) {
        /* Optimistic cache update: toggle the bit immediately.
         * This matches the new_state we stored in the command. */
        s_channel_states ^= (1u << channel);
        return true;
    }
    return false;
}

/**
 * @brief Turn all relay channels ON (non-blocking).
 *
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_AllOn(uint32_t timeout_ms) {
    if (!s_initialized || !s_switch_queue) {
        return false;
    }

    switch_cmd_t cmd = {.type = SWITCH_CMD_ALL_ON, .channel = 0, .state = 1, .mask = 0xFF};

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(s_switch_queue, &cmd, ticks) == pdTRUE) {
        /* Optimistic cache update: all channels ON */
        s_channel_states = 0xFF;
        return true;
    }
    return false;
}

/**
 * @brief Turn all relay channels OFF (non-blocking).
 *
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_AllOff(uint32_t timeout_ms) {
    if (!s_initialized || !s_switch_queue) {
        return false;
    }

    switch_cmd_t cmd = {.type = SWITCH_CMD_ALL_OFF, .channel = 0, .state = 0, .mask = 0x00};

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(s_switch_queue, &cmd, ticks) == pdTRUE) {
        /* Optimistic cache update: all channels OFF */
        s_channel_states = 0x00;
        return true;
    }
    return false;
}

/**
 * @brief Set multiple channels via bitmask (non-blocking).
 *
 * @param mask 8-bit state mask for all channels
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_SetMask(uint8_t mask, uint32_t timeout_ms) {
    if (!s_initialized || !s_switch_queue) {
        return false;
    }

    switch_cmd_t cmd = {.type = SWITCH_CMD_SET_MASK, .channel = 0, .state = 0, .mask = mask};

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (xQueueSend(s_switch_queue, &cmd, ticks) == pdTRUE) {
        /* Optimistic cache update: set all channels per mask */
        s_channel_states = mask;
        return true;
    }
    return false;
}

/**
 * @brief Force synchronization of cached state from hardware.
 *
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait)
 * @return true if command enqueued, false if queue full
 */
bool Switch_SyncFromHardware(uint32_t timeout_ms) {
    if (!s_initialized || !s_switch_queue) {
        return false;
    }

    switch_cmd_t cmd = {.type = SWITCH_CMD_SYNC_FROM_HW, .channel = 0, .state = 0, .mask = 0};

    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return (xQueueSend(s_switch_queue, &cmd, ticks) == pdTRUE);
}

/**
 * @brief Queue a masked write to the relay MCP Port B (non-blocking).
 *
 * Used by the HLW8032 driver to control the MUX A/B/C/EN lines which are
 * on Port B bits 0-3. SwitchTask stays the sole owner of the relay MCP,
 * while the HLW driver keeps the revision-dependent bit mapping logic.
 *
 * @param mask Bitmask of Port B bits to modify.
 * @param value New values for the masked bits (only bits in @p mask matter).
 * @param timeout_ms Maximum time to wait for queue space (0 = no wait).
 * @return true if command enqueued, false if queue not ready or full.
 */
bool Switch_SetRelayPortBMasked(uint8_t mask, uint8_t value, uint32_t timeout_ms) {
    if (!s_initialized || !s_switch_queue) {
        return false;
    }

    if (mask == 0u) {
        return true;
    }

    switch_cmd_t cmd = {
        .type = SWITCH_CMD_SET_RELAY_PORTB_MASK,
        .channel = 0u,  /* unused for this command type */
        .state = value, /* carries Port B value */
        .mask = mask    /* carries Port B mask */
    };

    TickType_t ticks = (timeout_ms == 0u) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return (xQueueSend(s_switch_queue, &cmd, ticks) == pdTRUE);
}