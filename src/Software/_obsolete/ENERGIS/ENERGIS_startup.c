/**
 * @file startup.c
 * @author DvidMakesThings - David Sipos
 * @defgroup main03 3. Startup
 * @ingroup main
 * @brief Contains the initialization code for the Pico board.
 * @{
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "ENERGIS_startup.h"

/**
 * @brief Initializes the startup configuration.
 * @return true if successful, false otherwise.
 * @param None
 * @note This function sets up the necessary hardware and peripherals for the application.
 */
bool startup_init(void) {
    // ----- UART Initialization -----
    gpio_set_function(UART0_RX, GPIO_FUNC_UART);
    gpio_set_function(UART0_TX, GPIO_FUNC_UART);

    // ----- Enable Voltage Regulator -----
    gpio_init(VREG_EN);
    gpio_set_dir(VREG_EN, GPIO_OUT);
    gpio_put(VREG_EN, 1); // Initially not in enable

    // Process LED
    gpio_init(PROC_LED);
    gpio_set_dir(PROC_LED, GPIO_OUT);
    gpio_put(PROC_LED, 0); // Initially not set

    // ----- I2C Initialization -----
    // I2C0 (used by the DisplayBoard MCP23017)
    i2c_init(i2c0, I2C_SPEED); // 100 kHz
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);

    // I2C1 (used by the MainBoard MCP23017, EEPROM)
    i2c_init(i2c1, I2C_SPEED); // 100 kHz
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);

    // ----- MCP23017 Reset Pins -----
    gpio_init(MCP_REL_RST);
    gpio_set_dir(MCP_REL_RST, GPIO_OUT);
    gpio_put(MCP_REL_RST, 1);
    gpio_init(MCP_LCD_RST);
    gpio_set_dir(MCP_LCD_RST, GPIO_OUT);
    gpio_put(MCP_LCD_RST, 1);


    // SPI for W5500 Ethernet Module (using W5500_SPI_INSTANCE, e.g. SPI0)
    gpio_init(W5500_CS);
    gpio_set_dir(W5500_CS, GPIO_OUT);
    gpio_put(W5500_CS, 1);

    spi_init(W5500_SPI_INSTANCE, SPI_SPEED_W5500);
    spi_set_format(W5500_SPI_INSTANCE, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(W5500_MOSI, GPIO_FUNC_SPI); // MOSI
    gpio_set_function(W5500_SCK, GPIO_FUNC_SPI);  // SCK
    gpio_set_function(W5500_MISO, GPIO_FUNC_SPI); // MISO
    gpio_init(W5500_INT);
    gpio_set_dir(W5500_INT, GPIO_IN);

    // Reset W5500
    gpio_init(W5500_RESET);
    gpio_set_dir(W5500_RESET, GPIO_OUT);
    gpio_put(W5500_RESET, 1); // Initially not in reset
    sleep_ms(100);
    gpio_put(W5500_RESET, 0); // Reset
    sleep_ms(100);
    gpio_put(W5500_RESET, 1); // Release reset

    // ----- Button Inputs -----
    // Initialize keys; external pull-ups are assumed so no internal pull-ups are set.
    gpio_init(KEY_0);
    gpio_set_dir(KEY_0, GPIO_IN);

    gpio_init(KEY_1);
    gpio_set_dir(KEY_1, GPIO_IN);

    gpio_init(KEY_2);
    gpio_set_dir(KEY_2, GPIO_IN);

    gpio_init(KEY_3);
    gpio_set_dir(KEY_3, GPIO_IN);

    // ----- ADC Inputs -----
    // gpio_init(ADC_VUSB);
    // gpio_set_dir(ADC_VUSB, GPIO_IN);
    // gpio_disable_pulls(ADC_VUSB);
    adc_gpio_init(ADC_VUSB);

    // gpio_init(ADC_12V_MEA);
    // gpio_set_dir(ADC_12V_MEA, GPIO_IN);
    // gpio_disable_pulls(ADC_12V_MEA);
    adc_gpio_init(ADC_12V_MEA);

    return true;
}

/**
 * @brief Initializes Core 0.
 *
 * Initializes the EEPROM, MCP23017 I/O expanders, and the ILI9488 display.
 * Also initializes the PDU display and updates the status message.
 */
bool core0_init(void) {
    sleep_ms(2000); // Delay for debugging

    check_and_init_factory_defaults(); // Only on the very first run, then ignored
    getSysInfo();                      // Print system information

    INFO_PRINT("ADC initializing\n");
    adc_init();
    adc_set_temp_sensor_enabled(true); // Enable internal temperature sensor
    DEBUG_PRINT("ADC OK\n");

    uint32_t vreg = *((volatile uint32_t *)0x40064000); // VREG_AND_CHIP_RESET->vreg
    printf("VREG register: 0x%08X\n", vreg);

    // INFO_PRINT("I2C scanning...\n");
    // i2c_scan_bus(i2c0, "I2C0");
    // i2c_scan_bus(i2c1, "I2C1");

    // Initialize EEPROM.

    INFO_PRINT("EEPROM initializing...\n");
    CAT24C512_Init();
    // EEPROM_ReadFactoryDefaults();
    DEBUG_PRINT("EEPROM OK\n");

    // Initialize MCP23017 I/O expanders.

    INFO_PRINT("MCP23017 initializing...\n");
    mcp_display_init();
    mcp_relay_init();
    mcp_selection_init();
    DEBUG_PRINT("MCP23017 OK\n");

    // Little showoff.
    mcp_display_write_reg(MCP23017_OLATA, 0xFF);
    mcp_display_write_reg(MCP23017_OLATB, 0xFF);
    mcp_selection_write_reg(MCP23017_OLATA, 0xFF);
    mcp_selection_write_reg(MCP23017_OLATB, 0xFF);
    sleep_ms(500);
    for (uint8_t i = 16; i > 0; i--) {
        mcp_display_write_pin(i - 1, 0);
        sleep_ms(50);
    }
    for (uint8_t i = 0; i < 16; i++) {
        mcp_selection_write_pin(i, 0);
        sleep_ms(50);
    }
    mcp_display_write_pin(PWR_LED, 1);
    setError(true);

    INFO_PRINT("Initializing HLW8032...\n");
    hlw8032_init();
    DEBUG_PRINT("HLW8032 OK\n");

    INFO_PRINT("Initializing buttons...\n");
    button_driver_init(); // Initialize buttons
    DEBUG_PRINT("Buttons initialized\n");

    return true;
}

/**
 * @brief Initializes Core 1.
 *
 * Initializes the W5500 Ethernet controller and sets up the network parameters.
 */
bool core1_init(void) {
    uint8_t link = 0;
    uint8_t server_socket = 0;
    uint16_t port = 80;
    networkInfo netConfig = LoadUserNetworkConfig(); // Get network info from EEPROM

    // Copy data to g_net_info
    memcpy(&g_net_info, &netConfig, sizeof(wiz_NetInfo));
    DEBUG_PRINT("Loaded network config: IP %d.%d.%d.%d\n", g_net_info.ip[0], g_net_info.ip[1],
                g_net_info.ip[2], g_net_info.ip[3]);
    DEBUG_PRINT("Network config loaded, continuing...\n\n");

    DEBUG_PRINT("Initialize W5500 Ethernet controller SPI\n");
    wizchip_spi_initialize();

    DEBUG_PRINT("Initialize W5500 Ethernet controller CRIS\n");
    wizchip_cris_initialize();

    DEBUG_PRINT("Set W5500 to RESET\n");
    wizchip_reset();

    DEBUG_PRINT("Initialize W5500 Ethernet controller\n");
    wizchip_initialize();

    DEBUG_PRINT("Check W5500 status\n");
    wizchip_check();
    if (ctlwizchip(CW_GET_PHYLINK, (void *)&link) == -1) {
        WARNING_PRINT("W5500 not accessible. Retrying...\n");
        setError(true);
        return false;
    } else if (link == 0) {
        WARNING_PRINT("W5500 PHY link is down, retrying...\n");
        setError(true);
        return false;
    } else {
        INFO_PRINT("W5500 PHY link is up\n");
        setError(false);
    }

    DEBUG_PRINT("Initialize network\n");
    network_initialize(g_net_info);
    print_network_information(g_net_info);

    mcp_display_write_pin(ETH_LED, 1);

    if (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
        setError(true);
        ERROR_PRINT("Socket failed\n");
        return false;
    }

    if (listen(server_socket) != SOCK_OK) {
        close(server_socket);
        setError(true);
        ERROR_PRINT("Listen failed\n");
        return false;
    }

    return true;
}

void check_and_init_factory_defaults(void) {
    const uint16_t MAGIC_ADDR = 0x7FFE; /* Last word in 64K EEPROM */
    const uint16_t MAGIC_VAL = 0xA55A;

    uint16_t magic = 0xFFFF;
    CAT24C512_ReadBuffer(MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));

    if (magic != MAGIC_VAL) {
        INFO_PRINT("First boot detected → writing factory defaults...\n");
        EEPROM_WriteFactoryDefaults();

        magic = MAGIC_VAL;
        CAT24C512_WriteBuffer(MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));

        INFO_PRINT("Factory defaults written. Rebooting...\n");
        sleep_ms(1000);
    }
}