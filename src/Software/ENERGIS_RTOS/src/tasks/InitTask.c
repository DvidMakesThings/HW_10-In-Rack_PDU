/**
 * @file InitTask.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-08
 *
 * @details InitTask runs at highest priority during system boot to:
 * 1. Initialize all hardware in proper sequence
 * 2. Probe peripherals to verify communication
 * 3. Create subsystem tasks in dependency order
 * 4. Wait for subsystems to report ready
 * 5. Delete itself when system is fully operational
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define INIT_TASK_PRIORITY INITTASK_PRIORITY /* Highest priority */
#define INIT_TASK_STACK_SIZE 2048

#define INIT_TASK_TAG "[INIT]"

/* Event flags for subsystem ready states */
#define READY_LOGGER (1 << 0)
#define READY_STORAGE (1 << 1)
#define READY_NET (1 << 2)
#define READY_METER (1 << 3)
#define READY_ALL (READY_LOGGER | READY_STORAGE | READY_NET | READY_METER)

static EventGroupHandle_t init_events = NULL;

/**
 * @brief Initialize GPIO pins for all peripherals
 *
 * @return None
 */
static void init_gpio(void) {

    /* Enable voltage regulator */
    gpio_init(VREG_EN);
    gpio_set_dir(VREG_EN, GPIO_OUT);
    gpio_put(VREG_EN, 1);

    /* Process LED */
    gpio_init(PROC_LED);
    gpio_set_dir(PROC_LED, GPIO_OUT);
    gpio_put(PROC_LED, 0);

    /* MCP reset pins */
    gpio_init(MCP_MB_RST);
    gpio_set_dir(MCP_MB_RST, GPIO_OUT);
    gpio_put(MCP_MB_RST, 1); /* Not in reset */

    gpio_init(MCP_DP_RST);
    gpio_set_dir(MCP_DP_RST, GPIO_OUT);
    gpio_put(MCP_DP_RST, 1); /* Not in reset */

    /* W5500 chip select */
    gpio_init(W5500_CS);
    gpio_set_dir(W5500_CS, GPIO_OUT);
    gpio_put(W5500_CS, 1); /* CS high (inactive) */

    /* W5500 interrupt pin */
    gpio_init(W5500_INT);
    gpio_set_dir(W5500_INT, GPIO_IN);

    /* W5500 reset sequence */
    gpio_init(W5500_RESET);
    gpio_set_dir(W5500_RESET, GPIO_OUT);
    gpio_put(W5500_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_put(W5500_RESET, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_put(W5500_RESET, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Button inputs (pull-ups enabled by button_task) */
    gpio_init(KEY_0);
    gpio_set_dir(KEY_0, GPIO_IN);
    gpio_init(KEY_1);
    gpio_set_dir(KEY_1, GPIO_IN);
    gpio_init(KEY_2);
    gpio_set_dir(KEY_2, GPIO_IN);
    gpio_init(KEY_3);
    gpio_set_dir(KEY_3, GPIO_IN);

    gpio_set_function(PROC_LED, GPIO_FUNC_PWM);

    static uint32_t s_pwm_slice = 0;
    s_pwm_slice = pwm_gpio_to_slice_num(PROC_LED);
    pwm_set_wrap(s_pwm_slice, 65535U);
    pwm_set_clkdiv(s_pwm_slice, 8.0f);
    pwm_set_enabled(s_pwm_slice, true);

    INFO_PRINT("%s GPIO configured\r\n", INIT_TASK_TAG);
}

/**
 * @brief Initialize I2C buses
 *
 * @return None
 */
static void init_i2c(void) {
    /* I2C0 (Display + Selection MCP23017s) */
    i2c_init(i2c0, I2C_SPEED);
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA);
    gpio_pull_up(I2C0_SCL);

    /* I2C1 (Relay MCP23017 + EEPROM) */
    i2c_init(i2c1, I2C_SPEED);
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);

    vTaskDelay(pdMS_TO_TICKS(10)); /* Allow I2C to stabilize */
    INFO_PRINT("%s I2C buses initialized\r\n", INIT_TASK_TAG);
}

/**
 * @brief Initialize SPI for W5500
 *
 * @return None
 */
static void init_spi(void) {
    spi_init(W5500_SPI_INSTANCE, SPI_SPEED_W5500);
    spi_set_format(W5500_SPI_INSTANCE, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(W5500_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(W5500_SCK, GPIO_FUNC_SPI);
    gpio_set_function(W5500_MISO, GPIO_FUNC_SPI);

    INFO_PRINT("%s SPI initialized for W5500\r\n", INIT_TASK_TAG);
}

/**
 * @brief Initialize ADC for voltage/temperature sensing
 *
 * @return None
 */
static void init_adc(void) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(ADC_VUSB);    /* GPIO26 */
    adc_gpio_init(ADC_12V_MEA); /* GPIO29 */

    INFO_PRINT("%s ADC initialized\r\n", INIT_TASK_TAG);
}

/**
 * @brief Probe I2C device at given address
 *
 * @param i2c I2C instance
 * @param addr 7-bit I2C address
 * @param name Device name for logging
 * @return true if device responds
 */
static bool probe_i2c_device(i2c_inst_t *i2c, uint8_t addr, const char *name) {
    uint8_t dummy;
    int ret = i2c_read_timeout_us(i2c, addr, &dummy, 1, false, 1000);
    bool present = (ret >= 0);

    if (present) {
        INFO_PRINT("%s %s detected at 0x%02X\r\n", INIT_TASK_TAG, name, addr);
    } else {
        INFO_PRINT("%s %s NOT detected at 0x%02X\r\n", INIT_TASK_TAG, name, addr);
    }

    return present;
}

/**
 * @brief Probe all MCP23017 devices
 *
 * @return true if all MCP23017s respond
 */
static bool probe_mcps(void) {
    bool relay_ok = probe_i2c_device(i2c1, MCP_RELAY_ADDR, "Relay MCP23017");
    bool display_ok = probe_i2c_device(i2c0, MCP_DISPLAY_ADDR, "Display MCP23017");
    bool selection_ok = probe_i2c_device(i2c0, MCP_SELECTION_ADDR, "Selection MCP23017");

    if (relay_ok && display_ok && selection_ok) {
        INFO_PRINT("%s All MCP23017s detected\r\n", INIT_TASK_TAG);
        return true;
    } else {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x0);
        ERROR_PRINT_CODE(errorcode, "%s Some MCP23017s missing!\r\n", INIT_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return false;
    }
}

/**
 * @brief Probe EEPROM
 *
 * @return true if EEPROM responds
 */
static bool probe_eeprom(void) {
    return probe_i2c_device(EEPROM_I2C, CAT24C256_I2C_ADDR, "EEPROM CAT24C256");
}

/**
 * @brief Read voltage from the selected ADC channel with averaging.
 */
static float adc_read_voltage_avg(uint8_t ch) {
    adc_set_clkdiv(96.0f); /* slow down ADC clock */
    adc_select_input(ch);
    (void)adc_read(); /* throwaway */
    (void)adc_read(); /* throwaway */
    vTaskDelay(pdMS_TO_TICKS(1));
    uint32_t adcread = 0;
    for (int i = 0; i < 16; i++)
        adcread += adc_read();
    uint16_t raw_12v = (uint16_t)(adcread / 16);
    float v_tap = ((float)raw_12v) * (ADC_VREF / (float)ADC_MAX);
    return v_tap * 1;
}

/**
 * @brief InitTask main routine that performs the full bring-alive sequence.
 *
 * @details
 * Sequence implemented exactly as specified:
 *  1) Boot banner + HW init (GPIO, tick/watchdog standby, UART/SPI/I2C, ADC).
 *  2) Rail check (3V3/5V) and peripheral probes:
 *     - MCP23017 x3 (I2C)
 *     - CAT24C256 EEPROM (read sys-info/checksum block)
 *     - HLW8032 power meter (expect sane response if available)
 *     - W5500 (read VERSIONR)
 *  3) Subsystem initialization in strict dependency order:
 *     - LoggerTask (captures all subsequent logs)
 *     - StorageTask (EEPROM owner; loads/validates config)
 *     - SwitchTask (MCP owner; applies SAFE state)         [optional]
 *     - ButtonTask (front-panel scan)                      [optional]
 *     - MeterTask  (HLW8032 polling)
 *     - NetTask    (W5500 + HTTP/SNMP using loaded config)
 *     - ConsoleTask (UART/USB-CDC console online)
 *     - HealthTask (watchdog, heartbeats)                  [optional]
 *  4) Configuration distribution:
 *     - Wait for StorageTask -> config ready
 *     - Fetch validated network config and passively rely on NetTask to use it
 *  5) Finalization:
 *     - LEDs to indicate RUNNING
 *     - Log "System bring-alive complete" and self-delete
 *
 * @param pvParameters Unused.
 */
static void InitTask(void *pvParameters) {
    (void)pvParameters;

    /* ===== PHASE 0: Start logger FIRST to avoid USB-CDC stalls ===== */
    LoggerTask_Init(true);
    {
        const TickType_t t0 = xTaskGetTickCount();
        const TickType_t deadline = t0 + pdMS_TO_TICKS(2000);
        while (!Logger_IsReady() && xTaskGetTickCount() < deadline) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    /* ===== Banner (after logger is ready; never before) ===== */
    log_printf("\r\n");
    INFO_PRINT("========================================\r\n");
    INFO_PRINT("  ENERGIS PDU - System Bring-Alive\r\n");
    INFO_PRINT("========================================\r\n");
    INFO_PRINT("Firmware: %s\r\n", SWVERSION);
    INFO_PRINT("Serial: %s\r\n", DEFAULT_SN);
    INFO_PRINT("========================================\r\n\r\n");

    /* ===== PHASE 1: Hardware Initialization ===== */
    INFO_PRINT("%s ===== Phase 1: Hardware Init =====\r\n\r\n", INIT_TASK_TAG);

    init_gpio();
    init_i2c();
    init_spi();
    init_adc();
    CAT24C256_Init();
    MCP2017_Init(); /* Registers and initializes 0x20/0x21/0x23 MCPs */
    vTaskDelay(pdMS_TO_TICKS(100));

    while (adc_read_voltage_avg(V_SUPPLY) * SUPPLY_DIVIDER < 10.0f) {

#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x7);
        ERROR_PRINT_CODE(errorcode, "%s 12V rail low, %f waiting...\r\n", INIT_TASK_TAG,
                         adc_read_voltage_avg(V_SUPPLY) * SUPPLY_DIVIDER);
        Storage_EnqueueErrorCode(errorcode);
#endif
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* ===== PHASE 2: Peripheral Probing (non-blocking; HLW deferred) ===== */
    log_printf("\r\n");
    INFO_PRINT("%s ===== Phase 2: Peripheral Probing =====\r\n", INIT_TASK_TAG);
    (void)probe_mcps();
    (void)probe_eeprom();

    /* ===== PHASE 3: Deterministic Subsystem Bring-up ===== */
    log_printf("\r\n");
    INFO_PRINT("%s ===== Phase 3: Subsystem Bring-up (deterministic) =====\r\n", INIT_TASK_TAG);
    const TickType_t step_timeout = pdMS_TO_TICKS(5000);
    const TickType_t poll_10ms = pdMS_TO_TICKS(10);

    /* 1) Logger (already started above, just report state) */
    if (Logger_IsReady())
        INFO_PRINT("%s LoggerTask ready\r\n", INIT_TASK_TAG);
    else {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s LoggerTask NOT ready!\r\n", INIT_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
    }

    /* 2) Console */
    ConsoleTask_Init(true);
    {
        TickType_t t0 = xTaskGetTickCount();
        while (!Console_IsReady() && (xTaskGetTickCount() - t0) < step_timeout) {
            vTaskDelay(poll_10ms);
        }
        if (Console_IsReady())
            INFO_PRINT("%s ConsoleTask ready\r\n", INIT_TASK_TAG);
        else {
#if ERRORLOGGER
            uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x2);
            ERROR_PRINT_CODE(errorcode, "%s ConsoleTask NOT ready!\r\n", INIT_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
        }
    }

    /* 3) Storage */
    StorageTask_Init(true);
    {
        TickType_t t0 = xTaskGetTickCount();
        while (!Storage_IsReady() && (xTaskGetTickCount() - t0) < step_timeout) {
            vTaskDelay(poll_10ms);
        }
        if (Storage_IsReady())
            INFO_PRINT("%s StorageTask ready\r\n", INIT_TASK_TAG);
        else {
#if ERRORLOGGER
            uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x3);
            ERROR_PRINT_CODE(errorcode, "%s StorageTask not ready (timeout)\r\n", INIT_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
        }
    }

    /* 3.5) Switch - relay control task (must start before ButtonTask and NetTask) */
    SwitchTask_Init(true);
    {
        TickType_t t0 = xTaskGetTickCount();
        while (!Switch_IsReady() && (xTaskGetTickCount() - t0) < step_timeout) {
            vTaskDelay(poll_10ms);
        }
        if (Switch_IsReady())
            INFO_PRINT("%s SwitchTask ready\r\n", INIT_TASK_TAG);
        else {
#if ERRORLOGGER
            uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0xA);
            ERROR_PRINT_CODE(errorcode, "%s SwitchTask not ready (timeout)\r\n", INIT_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
        }
    }

    /* 4) Button */
    ButtonTask_Init(true);
    {
        const TickType_t step_timeout = pdMS_TO_TICKS(5000);
        const TickType_t poll_10ms = pdMS_TO_TICKS(10);
        TickType_t t0 = xTaskGetTickCount();

        while (!Button_IsReady() && (xTaskGetTickCount() - t0) < step_timeout) {
            vTaskDelay(poll_10ms);
        }

        if (Button_IsReady()) {
            INFO_PRINT("%s ButtonTask ready\r\n", INIT_TASK_TAG);
        } else {
#if ERRORLOGGER
            uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x4);
            ERROR_PRINT_CODE(errorcode, "%s ButtonTask NOT ready (timeout)\r\n", INIT_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
        }
    }

    /* 5) Network */
    if (NetTask_Init(true) != pdPASS) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x5);
        ERROR_PRINT_CODE(errorcode, "%s Failed to create NetTask\r\n", INIT_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
    }
    {
        TickType_t t0 = xTaskGetTickCount();
        while (!Net_IsReady() && (xTaskGetTickCount() - t0) < step_timeout) {
            vTaskDelay(poll_10ms);
        }
        if (Net_IsReady())
            INFO_PRINT("%s NetTask ready\r\n", INIT_TASK_TAG);
        else {
#if ERRORLOGGER
            uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x5);
            ERROR_PRINT_CODE(errorcode, "%s NetTask not ready (timeout)\r\n", INIT_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
        }
    }

    /* 6) Meter (HLW handled asynchronously by the task) */
    if (MeterTask_Init(true) != pdPASS) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x6);
        ERROR_PRINT_CODE(errorcode, "%s Failed to create MeterTask\r\n", INIT_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
    }
    {
        TickType_t t0 = xTaskGetTickCount();
        while (!Meter_IsReady() && (xTaskGetTickCount() - t0) < step_timeout) {
            vTaskDelay(poll_10ms);
        }
        if (Meter_IsReady())
            INFO_PRINT("%s MeterTask ready\r\n", INIT_TASK_TAG);
        else {
#if ERRORLOGGER
            uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x7);
            ERROR_PRINT_CODE(errorcode, "%s MeterTask not ready (timeout)\r\n", INIT_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
        }
    }

    /* ===== PHASE 4: Configuration Load & Distribution ===== */
    log_printf("\r\n");
    INFO_PRINT("%s ===== Phase 4: Configuration Load =====\r\n", INIT_TASK_TAG);
    if (!storage_wait_ready(10000)) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x8);
        ERROR_PRINT_CODE(errorcode, "%s Storage config NOT ready (timeout)\r\n", INIT_TASK_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
    } else {
        INFO_PRINT("%s Storage config ready\r\n", INIT_TASK_TAG);
    }

    networkInfo ni;
    if (storage_get_network(&ni)) {
        INFO_PRINT("%s Saved Network config: \r\n", INIT_TASK_TAG);
        INFO_PRINT("                 IP  : %u.%u.%u.%u\r\n", ni.ip[0], ni.ip[1], ni.ip[2],
                   ni.ip[3]);
        INFO_PRINT("                 SM  : %u.%u.%u.%u\r\n", ni.sn[0], ni.sn[1], ni.sn[2],
                   ni.sn[3]);
        INFO_PRINT("                 GW  : %u.%u.%u.%u\r\n", ni.gw[0], ni.gw[1], ni.gw[2],
                   ni.gw[3]);
        INFO_PRINT("                 DNS : %u.%u.%u.%u\r\n", ni.dns[0], ni.dns[1], ni.dns[2],
                   ni.dns[3]);
    }

    /* ===== PHASE 5: Finalization ===== */
    log_printf("\r\n");
    INFO_PRINT("%s ===== Finalization: entering =====\r\n", INIT_TASK_TAG);

    /* ===== Health task wiring (register existing tasks, then start) ===== */
    {
        TaskHandle_t h;

        h = xTaskGetHandle("Logger");
        if (h)
            Health_RegisterTask(HEALTH_ID_LOGGER, h, "LoggerTask");

        h = xTaskGetHandle("Console");
        if (h)
            Health_RegisterTask(HEALTH_ID_CONSOLE, h, "ConsoleTask");

        h = xTaskGetHandle("Storage");
        if (h)
            Health_RegisterTask(HEALTH_ID_STORAGE, h, "StorageTask");

        h = xTaskGetHandle("ButtonTask");
        if (h)
            Health_RegisterTask(HEALTH_ID_BUTTON, h, "ButtonTask");

        h = xTaskGetHandle("SwitchTask");
        if (h)
            Health_RegisterTask(HEALTH_ID_SWITCH, h, "SwitchTask");

        h = xTaskGetHandle("Net");
        if (h)
            Health_RegisterTask(HEALTH_ID_NET, h, "NetTask");

        h = xTaskGetHandle("MeterTask");
        if (h)
            Health_RegisterTask(HEALTH_ID_METER, h, "MeterTask");

        /* Start Health only after Meter is actually ready */
        if (!Meter_IsReady()) {
            INFO_PRINT("%s Waiting for Meter to be ready before starting Health...\r\n",
                       INIT_TASK_TAG);
            const TickType_t t0 = xTaskGetTickCount();
            const TickType_t max_wait = pdMS_TO_TICKS(30000); /* give it up to 30 s */
            while (!Meter_IsReady() && (xTaskGetTickCount() - t0) < max_wait) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }

        if (Meter_IsReady()) {
            HealthTask_Start();
        } else {
#if ERRORLOGGER
            uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_INIT, ERR_SEV_ERROR, ERR_FID_INITTASK, 0x9);
            ERROR_PRINT_CODE(errorcode, "%s Meter not ready after wait; NOT starting Health.\r\n",
                             INIT_TASK_TAG);
            Storage_EnqueueErrorCode(errorcode);
#endif
        }
    }

    INFO_PRINT("%s ===================================\r\n", INIT_TASK_TAG);
    xEventGroupWaitBits(cfgEvents, CFG_READY_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    INFO_PRINT("%s \tCrash logs:\r\n", INIT_TASK_TAG);
    Helpers_LateBootDumpAndClear();
    INFO_PRINT("%s ===================================\r\n\r\n", INIT_TASK_TAG);
    log_printf("\r\n");

    INFO_PRINT("%s System bring-alive complete =====\r\n\r\n", INIT_TASK_TAG);
    INFO_PRINT("==========================================\r\n");
    INFO_PRINT("               SYSTEM READY               \r\n");
    INFO_PRINT("==========================================\r\n\r\n");

    vTaskDelete(NULL);
}

/* ##################################################################### */
/*                       PUBLIC API FUNCTIONS                            */
/* ##################################################################### */

/**
 * @brief Create and start the InitTask at highest priority.
 *
 * @details
 * Call this from main() before vTaskStartScheduler(). The task will execute the
 * deterministic bring-up sequence (Logger -> Console -> Storage -> Button -> Net -> Meter),
 * then delete itself when all subsystems are ready or have timed out.
 */
void InitTask_Create(void) {
    xTaskCreate(InitTask, "InitTask", INIT_TASK_STACK_SIZE, NULL, INIT_TASK_PRIORITY, NULL);
}