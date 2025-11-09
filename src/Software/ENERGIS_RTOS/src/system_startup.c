/**
 * @file system_startup.c
 * @author DvidMakesThings - David Sipos
 * @defgroup tasks Tasks
 * @brief General utility functions for ENERGIS PDU firmware.
 * @{
 *
 * @defgroup tasks01 1. System Startup Initialization
 * @ingroup tasks
 * @brief System hardware initialization implementation (RTOS version)
 * @{
 * @version 1.0.0
 * @date 2025-11-08
 * @details
 * Initializes all hardware peripherals before RTOS scheduler starts.
 * No tasks, no queues - just raw hardware setup.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "CONFIG.h"

/**
 * @brief Initialize all hardware peripherals and GPIOs.
 *
 * Order of initialization:
 * 0. Logger (stdio_init_all + create logging queue/task)
 * 1. Power control (VREG_EN)
 * 2. Status LED (PROC_LED)
 * 3. I2C buses (I2C0, I2C1)
 * 4. MCP reset pins
 * 5. SPI for W5500
 * 6. W5500 control pins and reset sequence
 * 7. Button inputs
 * 8. ADC inputs
 * 9. EEPROM (CAT24C512)
 * 10. Board-level MCP23017 initialization
 * 11. RTOS Tasks (Console, Storage, Button, Net, Meter)
 *
 * Note: LoggerTask won't actually run until vTaskStartScheduler() is called,
 * but the queue is created and stdio is ready for use.
 *
 * @return true if successful, false on error
 */
bool system_startup_init(void) {
    /* ----- 0. Logger Initialization ----- */
    /* Sets up USB-CDC via stdio_init_all(), creates logger queue and LoggerTask */
    LoggerTask_Init();

    /* ----- 1. UART Initialization ----- */
    /* UART0 - Reserved for HLW8032 power meter (configured by MeterTask later) */
    /* UART1 - Not used (console is on USB-CDC via stdio) */
    /* Note: stdio_init_all() called by LoggerTask_Init() above */

    /* ----- 2. Enable Voltage Regulator ----- */
    gpio_init(VREG_EN);
    gpio_set_dir(VREG_EN, GPIO_OUT);
    gpio_put(VREG_EN, 1); /* Enable voltage regulator */

    /* ----- 3. Process LED ----- */
    gpio_init(PROC_LED);
    gpio_set_dir(PROC_LED, GPIO_OUT);
    gpio_put(PROC_LED, 0); /* Initially off */

    /* ----- 4. I2C Initialization ----- */
    /* I2C0 (used by DisplayBoard MCP23017 @ 0x21, SelectionBoard @ 0x23) */
    i2c_init(i2c0, I2C_SPEED); /* 100 kHz standard mode */
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA); /* Enable pull-ups for I2C */
    gpio_pull_up(I2C0_SCL);

    /* I2C1 (used by RelayBoard MCP23017 @ 0x20, EEPROM) */
    i2c_init(i2c1, I2C_SPEED); /* 100 kHz standard mode */
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA); /* Enable pull-ups for I2C */
    gpio_pull_up(I2C1_SCL);

    /* ----- 5. MCP23017 Reset Pins ----- */
    /* Relay board reset (MCP_MB_RST) */
    gpio_init(MCP_MB_RST);
    gpio_set_dir(MCP_MB_RST, GPIO_OUT);
    gpio_put(MCP_MB_RST, 1); /* Not in reset */

    /* Display/Selection board shared reset (MCP_DP_RST) */
    gpio_init(MCP_DP_RST);
    gpio_set_dir(MCP_DP_RST, GPIO_OUT);
    gpio_put(MCP_DP_RST, 1); /* Not in reset */

    /* ----- 6. SPI for W5500 Ethernet Module ----- */
    gpio_init(W5500_CS);
    gpio_set_dir(W5500_CS, GPIO_OUT);
    gpio_put(W5500_CS, 1); /* CS high (inactive) */

    spi_init(W5500_SPI_INSTANCE, SPI_SPEED_W5500); /* 40 MHz */
    spi_set_format(W5500_SPI_INSTANCE, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(W5500_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(W5500_SCK, GPIO_FUNC_SPI);
    gpio_set_function(W5500_MISO, GPIO_FUNC_SPI);

    /* W5500 interrupt pin (input) */
    gpio_init(W5500_INT);
    gpio_set_dir(W5500_INT, GPIO_IN);

    /* ----- 7. W5500 Reset Sequence ----- */
    gpio_init(W5500_RESET);
    gpio_set_dir(W5500_RESET, GPIO_OUT);
    gpio_put(W5500_RESET, 1); /* Release reset */
    sleep_ms(100);            /* Wait for stable power */
    gpio_put(W5500_RESET, 0); /* Assert reset */
    sleep_ms(100);            /* Hold reset */
    gpio_put(W5500_RESET, 1); /* Release reset */
    sleep_ms(100);            /* Wait for W5500 to boot */

    /* ----- 8. Button Inputs ----- */
    /* Assuming external pull-ups exist; set as inputs without internal pulls */
    gpio_init(KEY_0);
    gpio_set_dir(KEY_0, GPIO_IN);
    /* gpio_pull_up(KEY_0); */ /* Uncomment if no external pull-up */

    gpio_init(KEY_1);
    gpio_set_dir(KEY_1, GPIO_IN);
    /* gpio_pull_up(KEY_1); */

    gpio_init(KEY_2);
    gpio_set_dir(KEY_2, GPIO_IN);
    /* gpio_pull_up(KEY_2); */

    gpio_init(KEY_3);
    gpio_set_dir(KEY_3, GPIO_IN);
    /* gpio_pull_up(KEY_3); */

    /* NOTE: button_driver.c will call gpio_pull_up() when it initializes,
     * so we don't strictly need pull-ups here if using button_task. */

    /* ----- 9. ADC Inputs ----- */
    /* Initialize ADC peripheral */
    adc_init();
    adc_set_temp_sensor_enabled(true); /* Enable internal temperature sensor */

    /* Configure ADC GPIO pins */
    adc_gpio_init(ADC_VUSB);    /* GPIO26 -> ADC channel 0 */
    adc_gpio_init(ADC_12V_MEA); /* GPIO29 -> ADC channel 3 */
    INFO_PRINT("[Startup] GPIO initialized\r\n");

    /* ----- 10. EEPROM (CAT24C512) on I2C1 ----- */
    /* Initialize EEPROM driver (uses I2C1 which was initialized above) */
    CAT24C512_Init();

    /* ----- 11. Board-level driver initialization (MCP23017 on I2C0/I2C1) ----- */
    mcp_board_init(); /* Registers and initializes 0x20/0x21/0x23 MCPs */

    /* ===== RTOS TASK INITIALIZATION ===== */

    /* 1. Console task (USB-CDC polling -> command dispatcher) */
    ConsoleTask_Init();

    /* 2. StorageTask - EEPROM/Config management */
    StorageTask_Init(); /* Creates task, checks factory defaults, loads config */

    /* 3. Button task (GPIO polling + debounce -> q_btn) */
    button_task_init();

    /* 4. NetTask (does W5500 bring-up after storage reports cfg ready) */
    if (NetTask_Init() != pdPASS) {
        ERROR_PRINT("Failed to start NetTask\r\n");
        setError(true);
    }

    /* TODO: 5. MeterTask_Start() + enable HLW8032 UART polling */
    /* NOTE: MeterTask initialization is currently disabled to avoid GPIO conflicts.
     * When enabled, ensure HLW8032_UART_ID (UART0) pins don't conflict with USB-CDC or other
     * peripherals. Uncomment the following block to enable:
     */

    uartHlwMtx = xSemaphoreCreateMutex();
    if (uartHlwMtx == NULL) {
        ERROR_PRINT("Failed to create UART HLW mutex\r\n");
        setError(true);
        return false;
    }
#if 0 // BUGGY: MeterTask currently disabled
    if (MeterTask_Init() != pdPASS) {
        ERROR_PRINT("Failed to start MeterTask\r\n");
        setError(true);
        return false;
    }
    INFO_PRINT("[Startup] MeterTask started\r\n");

#endif

    /* TODO: 6. HealthTask_Start() (watchdog kicker) */

    return true;
}

/**
 * @}
 */