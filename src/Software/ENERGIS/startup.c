/**
 * @file startup.c
 * @author David Sipos
 * @brief Contains the initialization code for the Pico board.
 * @version 1.1
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */

#include "startup.h"

/**
 * @brief Initializes RP2040 pins and peripheral buses.
 *
 * Sets up GPIO directions and functions, and initializes the I2C and SPI buses.
 */
bool startup_init(void) {
    // Initialize stdio (if needed for debugging)
    stdio_init_all();

    // ----- UART Initialization -----
    gpio_set_function(UART0_RX, GPIO_FUNC_UART);
    gpio_set_function(UART0_TX, GPIO_FUNC_UART);

    // ----- I2C Initialization -----
    // I2C0 (for example, used by the relay board or EEPROM)
    i2c_init(i2c0, 400000); // 400 kHz
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);

    // ----- MCP23017 Reset Pins -----
    // For the relay board's MCP23017
    gpio_init(MCP_REL_RST);
    gpio_set_dir(MCP_REL_RST, GPIO_OUT);
    gpio_put(MCP_REL_RST, 1); // Initially not in reset

    // I2C1 (used by the display board's MCP23017, EEPROM, etc.)
    i2c_init(i2c1, 400000); // 400 kHz
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);

    // For the display board's MCP23017
    gpio_init(MCP_LCD_RST);
    gpio_set_dir(MCP_LCD_RST, GPIO_OUT);
    gpio_put(MCP_LCD_RST, 1); // Initially not in reset

    // ----- SPI Initialization -----
    // SPI for LCD (using ILI9488_SPI_INSTANCE, e.g. SPI1)
    spi_init(ILI9488_SPI_INSTANCE, SPI_SPEED);
    spi_set_format(ILI9488_SPI_INSTANCE, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(LCD_SCLK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MISO, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LCD_CS, GPIO_OUT);

    // LCD control pins
    gpio_init(LCD_DC);
    gpio_set_dir(LCD_DC, GPIO_OUT);

    gpio_init(LCD_BL);
    gpio_set_dir(LCD_BL, GPIO_OUT);

    gpio_init(LCD_RESET);
    gpio_set_dir(LCD_RESET, GPIO_OUT);
    gpio_put(LCD_RESET, 1); // Release LCD reset

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
    gpio_init(ADC_VUSB);
    gpio_set_dir(ADC_VUSB, GPIO_IN);

    gpio_init(ADC_12V_MEA);
    gpio_set_dir(ADC_12V_MEA, GPIO_IN);

    return true;
}

/**
 * @brief Initializes Core 0.
 *
 * Initializes the EEPROM, MCP23017 I/O expanders, and the ILI9488 display.
 * Also initializes the PDU display and updates the status message.
 */
bool core0_init(void) {
    if (DEBUG) {
        i2c_scan_bus(i2c0, "I2C0");
        i2c_scan_bus(i2c1, "I2C1");
    }

    // Initialize EEPROM.
    CAT24C512_Init();
    INFO_PRINT("EEPROM initialized\n\n");
    EEPROM_ReadFactoryDefaults();

    // Initialize MCP23017 I/O expanders.
    mcp_display_init();
    INFO_PRINT("Display-board initialized\n\n");
    mcp_relay_init();
    INFO_PRINT("Relay-board initialized\n\n");

    // Little showoff.
    mcp_display_write_reg(MCP23017_OLATA, 0xFF);
    mcp_display_write_reg(MCP23017_OLATB, 0xFF);
    sleep_ms(500);
    for (uint8_t i = 16; i > 0; i--) {
        mcp_display_write_pin(i - 1, 0);
        sleep_ms(50);
    }
    mcp_display_write_pin(PWR_LED, 1);
    mcp_display_write_pin(FAULT_LED, 1);

    // Initialize the ILI9488 display.
    ILI9488_Init();
    PDU_Display_Init();
    sleep_ms(200);
    PDU_Display_UpdateStatus("System initialized.");

    button_driver_init(); // Initialize buttons

    return true;
}

/**
 * @brief Initializes Core 1.
 *
 * Initializes the W5500 Ethernet controller and sets up the network parameters.
 */
bool core1_init(void) {
    uint8_t server_socket = 0;
    uint16_t port = 80;
    networkInfo netConfig = LoadUserNetworkConfig(); // Get network info from EEPROM

    // Copy data to g_net_info
    memcpy(&g_net_info, &netConfig, sizeof(wiz_NetInfo));
    INFO_PRINT("Loaded network config: IP %d.%d.%d.%d\n", g_net_info.ip[0], g_net_info.ip[1],
               g_net_info.ip[2], g_net_info.ip[3]);

    wizchip_spi_initialize();
    wizchip_cris_initialize();
    wizchip_reset();
    wizchip_initialize();
    wizchip_check();
    network_initialize(g_net_info);
    print_network_information(g_net_info);

    PDU_Display_UpdateIP(g_net_info.ip);
    PDU_Display_UpdateConnectionStatus(1);
    mcp_display_write_pin(ETH_LED, 1);

    if (socket(server_socket, Sn_MR_TCP, port, 0) != server_socket) {
        ERROR_PRINT("Socket failed\n");
        return false;
    }

    if (listen(server_socket) != SOCK_OK) {
        close(server_socket);
        ERROR_PRINT("Listen failed\n");
        return false;
    }

    return true;
}
