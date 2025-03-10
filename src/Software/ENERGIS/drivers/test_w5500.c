/**
 * W5500 HTTP Server Example for RP2040
 * 
 * This example demonstrates how to use the W5500 Ethernet controller
 * to create a simple HTTP server on the RP2040 microcontroller.
 *
 * Copyright (c) 2025
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "w5500_rp2040_http.h"

// W5500 Configuration - Updated pin assignments
#define W5500_SPI_PORT    spi0
#define W5500_CS_PIN      17
#define W5500_RST_PIN     20
#define W5500_INT_PIN     21
#define W5500_SPI_SCK     18
#define W5500_SPI_MOSI    19
#define W5500_SPI_MISO    16
#define W5500_SPI_BAUDRATE 10000000

// Network Configuration
#define SERVER_PORT 80

// Static IP configuration
const uint8_t ip_addr[4] = {192, 168, 1, 100};
const uint8_t subnet_mask[4] = {255, 255, 255, 0};
const uint8_t gateway[4] = {192, 168, 1, 1};
const uint8_t mac_addr[6] = {0x00, 0x08, 0xDC, 0x01, 0x02, 0x03};

// HTTP route handlers
void index_handler(http_request_t *req, http_response_t *res);
void sensors_handler(http_request_t *req, http_response_t *res);
void not_found_handler(http_request_t *req, http_response_t *res);

int main() {
    // Initialize stdio
    stdio_init_all();
    
    // Wait a bit for serial terminal to connect
    sleep_ms(2000);
    
    printf("W5500 HTTP Server Example Starting...\n");
    
    // Configure W5500 SPI pins
    gpio_set_function(W5500_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(W5500_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(W5500_SPI_MISO, GPIO_FUNC_SPI);
    
    // CS pin as GPIO
    gpio_init(W5500_CS_PIN);
    gpio_set_dir(W5500_CS_PIN, GPIO_OUT);
    gpio_put(W5500_CS_PIN, 1);  // Deselect by default
    
    // Reset pin
    gpio_init(W5500_RST_PIN);
    gpio_set_dir(W5500_RST_PIN, GPIO_OUT);
    
    // Initialize SPI
    spi_init(W5500_SPI_PORT, W5500_SPI_BAUDRATE);
    spi_set_format(W5500_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    printf("SPI initialized\n");
    
    // Reset W5500
    gpio_put(W5500_RST_PIN, 0);
    sleep_ms(100);
    gpio_put(W5500_RST_PIN, 1);
    sleep_ms(100);
    
    printf("W5500 reset complete\n");
    
    // Initialize W5500
    if (!w5500_init(W5500_SPI_PORT, W5500_CS_PIN)) {
        printf("W5500 initialization failed\n");
        while (1) {
            sleep_ms(1000); // Just wait if error
        }
    }
    
    printf("W5500 initialized successfully\n");
    
    // Configure network
    w5500_set_mac(mac_addr);
    w5500_set_ip(ip_addr);
    w5500_set_subnet(subnet_mask);
    w5500_set_gateway(gateway);
    
    printf("Network configured\n");
    printf("IP Address: %d.%d.%d.%d\n", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
    
    // Initialize HTTP server
    http_server_t server;
    http_server_init(&server, SERVER_PORT);
    
    // Register routes
    http_server_register_route(&server, "/", HTTP_METHOD_GET, index_handler);
    http_server_register_route(&server, "/api/sensors", HTTP_METHOD_GET, sensors_handler);
    http_server_set_not_found_handler(&server, not_found_handler);
    
    printf("HTTP server initialized on port %d\n", SERVER_PORT);
    printf("Routes registered\n");
    
    // Start the HTTP server
    if (!http_server_start(&server)) {
        printf("Failed to start HTTP server\n");
        while (1) {
            sleep_ms(1000); // Just wait if error
        }
    }
    
    printf("HTTP server running\n");
    
    // Main loop - process incoming connections
    while (1) {
        http_server_process(&server);
        sleep_ms(10); // Small delay to prevent tight loop
    }
    
    return 0;
}

// Route handlers implementation
void index_handler(http_request_t *req, http_response_t *res) {
    const char *html = "<!DOCTYPE html>\n"
                       "<html>\n"
                       "<head>\n"
                       "    <title>RP2040 W5500 Web Server</title>\n"
                       "    <style>\n"
                       "        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; }\n"
                       "        h1 { color: #2c3e50; }\n"
                       "        .container { max-width: 800px; margin: 0 auto; }\n"
                       "        .btn { background-color: #3498db; color: white; border: none; padding: 10px 15px;\n"
                       "              border-radius: 4px; cursor: pointer; margin: 5px; }\n"
                       "        .btn:hover { background-color: #2980b9; }\n"
                       "        #sensor-data { margin: 20px 0; padding: 10px; background-color: #f8f9fa; border-radius: 4px; }\n"
                       "    </style>\n"
                       "</head>\n"
                       "<body>\n"
                       "    <div class='container'>\n"
                       "        <h1>RP2040 W5500 Web Server</h1>\n"
                       "        <p>Welcome to the Raspberry Pi Pico W5500 web server demo.</p>\n"
                       "        <h2>Sensor Data</h2>\n"
                       "        <button class='btn' onclick='getSensorData()'>Refresh Sensor Data</button>\n"
                       "        <div id='sensor-data'>No data</div>\n"
                       "    </div>\n"
                       "    <script>\n"
                       "        function getSensorData() {\n"
                       "            fetch('/api/sensors')\n"
                       "                .then(response => response.json())\n"
                       "                .then(data => {\n"
                       "                    document.getElementById('sensor-data').innerText = \n"
                       "                        'Temperature: ' + data.temperature + '°C, ' +\n"
                       "                        'Humidity: ' + data.humidity + '%';\n"
                       "                });\n"
                       "        }\n"
                       "    </script>\n"
                       "</body>\n"
                       "</html>";
    
    http_response_set_status(res, 200);
    http_response_set_header(res, "Content-Type", "text/html");
    http_response_set_body(res, html, strlen(html));
}

void sensors_handler(http_request_t *req, http_response_t *res) {
    // In a real application, we would read actual sensor values
    // For this example, we'll return mock data
    
    // Simulate reading temperature (mock value between 20-25°C)
    float temperature = 20.0 + ((float)rand() / RAND_MAX) * 5.0;
    
    // Simulate reading humidity (mock value between 30-60%)
    int humidity = 30 + (rand() % 31);
    
    // Format the JSON response
    char json_response[100];
    snprintf(json_response, sizeof(json_response), 
             "{\"temperature\":%.1f,\"humidity\":%d}", temperature, humidity);
    
    http_response_set_status(res, 200);
    http_response_set_header(res, "Content-Type", "application/json");
    http_response_set_body(res, json_response, strlen(json_response));
}

void not_found_handler(http_request_t *req, http_response_t *res) {
    const char *response = "404 Not Found";
    http_response_set_status(res, 404);
    http_response_set_header(res, "Content-Type", "text/plain");
    http_response_set_body(res, response, strlen(response));
}