#include "w5500_driver.h"
#include "../CONFIG.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

// --- Default Network Settings (from CONFIG.h) ---
static const uint8_t default_mac[6]     = { 0x00, 0x08, 0xDC, 0x12, 0x34, 0x56 };
static const uint8_t default_ip[4]      = { 192, 168, 0, 100 };
static const uint8_t default_subnet[4]  = { 255, 255, 255, 0 };
static const uint8_t default_gateway[4] = { 192, 168, 0, 1 };

// --- SPI Low–Level Functions ---
// Write "len" bytes starting at address "addr" on the given block.
static int w5500_spi_write(uint16_t addr, uint8_t block, const uint8_t *data, uint16_t len) {
    uint8_t header[3];
    header[0] = (addr >> 8) & 0xFF;
    header[1] = addr & 0xFF;
    header[2] = (block << 3) | W5500_WRITE_OP | W5500_VDM;
    
    gpio_put(W5500_CS, 0);
    spi_write_blocking(W5500_SPI_INSTANCE, header, 3);
    spi_write_blocking(W5500_SPI_INSTANCE, data, len);
    gpio_put(W5500_CS, 1);
    return 0;
}

// Read "len" bytes from address "addr" on the given block.
static int w5500_spi_read(uint16_t addr, uint8_t block, uint8_t *data, uint16_t len) {
    uint8_t header[3];
    header[0] = (addr >> 8) & 0xFF;
    header[1] = addr & 0xFF;
    header[2] = (block << 3) | W5500_READ_OP | W5500_VDM;
    
    gpio_put(W5500_CS, 0);
    spi_write_blocking(W5500_SPI_INSTANCE, header, 3);
    spi_read_blocking(W5500_SPI_INSTANCE, 0x00, data, len);
    gpio_put(W5500_CS, 1);
    return 0;
}

// --- Low–Level Register Access ---
uint8_t w5500_read_reg(uint16_t reg) {
    uint8_t data;
    w5500_spi_read(reg, 0x00, &data, 1);
    return data;
}

void w5500_write_reg(uint16_t reg, uint8_t data) {
    w5500_spi_write(reg, 0x00, &data, 1);
}

// --- W5500 Reset ---
void w5500_reset(void) {
    gpio_put(W5500_RESET, 0);
    sleep_ms(100);
    gpio_put(W5500_RESET, 1);
    sleep_ms(100);
}

// --- Network Configuration ---
// Write Gateway (0x0001), Subnet (0x0005), MAC (0x0009), and Source IP (0x000F)
int w5500_set_network(const uint8_t *mac, const uint8_t *ip, const uint8_t *subnet, const uint8_t *gateway) {
    if (w5500_spi_write(0x0001, 0x00, gateway, 4) != 0) return -1;
    if (w5500_spi_write(0x0005, 0x00, subnet, 4) != 0) return -1;
    if (w5500_spi_write(0x0009, 0x00, mac, 6) != 0) return -1;
    if (w5500_spi_write(0x000F, 0x00, ip, 4) != 0) return -1;
    return 0;
}

// Helper: Calculate the absolute address for a socket register.
static uint16_t socket_reg(uint8_t sock, uint16_t reg_offset) {
    return SOCKET_REG_BASE(sock) + reg_offset;
}

// Helper: Read from a socket register.
static int socket_read(uint8_t sock, uint16_t reg_offset, uint8_t *data, uint16_t len) {
    return w5500_spi_read(socket_reg(sock, reg_offset), sock, data, len);
}

// Helper: Write to a socket register.
static int socket_write(uint8_t sock, uint16_t reg_offset, const uint8_t *data, uint16_t len) {
    return w5500_spi_write(socket_reg(sock, reg_offset), sock, data, len);
}

static int socket_write_reg(uint8_t sock, uint16_t reg_offset, uint8_t data) {
    return socket_write(sock, reg_offset, &data, 1);
}

static uint8_t socket_read_reg(uint8_t sock, uint16_t reg_offset) {
    uint8_t data;
    socket_read(sock, reg_offset, &data, 1);
    return data;
}

// --- TX Buffer Wrap–Around Send Function ---
static int socket_send_buffer(uint8_t sock, const uint8_t *data, uint16_t len) {
    uint8_t tx_buf[2];
    socket_read(sock, Sn_TX_WR, tx_buf, 2);
    uint16_t ptr = (tx_buf[0] << 8) | tx_buf[1];
    uint16_t offset = ptr % TX_BUF_SIZE;
    uint16_t first_chunk = TX_BUF_SIZE - offset;
    if (first_chunk > len)
        first_chunk = len;
    // Write first chunk.
    uint16_t addr = SOCKET_TX_BUF_BASE(sock) + offset;
    if (w5500_spi_write(addr, sock, data, first_chunk) != 0) return -1;
    // If wrap–around is needed, write the remainder at the beginning of the buffer.
    if (first_chunk < len) {
        uint16_t second_chunk = len - first_chunk;
        addr = SOCKET_TX_BUF_BASE(sock);
        if (w5500_spi_write(addr, sock, data + first_chunk, second_chunk) != 0) return -1;
    }
    // Update the TX write pointer.
    ptr += len;
    uint8_t new_ptr[2] = { (uint8_t)(ptr >> 8), (uint8_t)(ptr & 0xFF) };
    socket_write(sock, Sn_TX_WR, new_ptr, 2);
    return 0;
}

// Wait for sufficient free space in the TX buffer.
static int socket_wait_tx_free(uint8_t sock, uint16_t size) {
    uint16_t tx_free = 0;
    uint8_t buf[2];
    uint32_t timeout = 1000;
    while (timeout--) {
        socket_read(sock, Sn_TX_FSR, buf, 2);
        tx_free = (buf[0] << 8) | buf[1];
        if (tx_free >= size) return 0;
        sleep_ms(1);
    }
    return -1;
}

// --- Public Socket Functions ---

int w5500_socket_open(uint8_t sock, uint8_t protocol, uint16_t port) {
    if (sock >= MAX_SOCKETS) {
        printf("Socket %d out of range\n", sock);
        return -1;
    }
    // Set local port.
    uint8_t port_buf[2] = { (uint8_t)(port >> 8), (uint8_t)(port & 0xFF) };
    if (socket_write(sock, Sn_PORT, port_buf, 2) != 0) return -1;
    // Set socket mode.
    if (socket_write_reg(sock, Sn_MR, protocol) != 0) return -1;
    // Issue OPEN command.
    if (socket_write_reg(sock, Sn_CR, SOCKET_CMD_OPEN) != 0) return -1;
    // Wait until socket enters INIT state.
    uint32_t timeout = 1000;
    while(timeout--) {
        if (socket_read_reg(sock, Sn_SR) == SOCK_INIT)
            break;
        sleep_ms(1);
    }
    if (socket_read_reg(sock, Sn_SR) != SOCK_INIT) {
        printf("Socket %d failed to open\n", sock);
        return -1;
    }
    return 0;
}

int w5500_socket_close(uint8_t sock) {
    if (socket_write_reg(sock, Sn_CR, SOCKET_CMD_CLOSE) != 0) return -1;
    uint32_t timeout = 1000;
    while(timeout--) {
        if (socket_read_reg(sock, Sn_SR) == SOCK_CLOSED)
            break;
        sleep_ms(1);
    }
    if (socket_read_reg(sock, Sn_SR) != SOCK_CLOSED) {
        printf("Socket %d failed to close\n", sock);
        return -1;
    }
    return 0;
}

int w5500_socket_send(uint8_t sock, const uint8_t *data, uint16_t len) {
    if (socket_wait_tx_free(sock, len) != 0) {
        printf("Socket %d TX buffer full\n", sock);
        return -1;
    }
    if (socket_send_buffer(sock, data, len) != 0) {
        printf("Socket %d failed to send data\n", sock);
        return -1;
    }
    if (socket_write_reg(sock, Sn_CR, SOCKET_CMD_SEND) != 0) return -1;
    uint32_t timeout = 1000;
    while(timeout--) {
        if (socket_read_reg(sock, Sn_CR) == 0)
            break;
        sleep_ms(1);
    }
    return 0;
}

int w5500_socket_recv(uint8_t sock, uint8_t *buffer, uint16_t bufsize) {
    uint8_t rx_size_buf[2];
    socket_read(sock, Sn_RX_RSR, rx_size_buf, 2);
    uint16_t rx_size = (rx_size_buf[0] << 8) | rx_size_buf[1];
    if (rx_size == 0) return 0;
    uint16_t read_len = (rx_size < bufsize) ? rx_size : bufsize;
    uint8_t rx_rd_buf[2];
    socket_read(sock, Sn_RX_RD, rx_rd_buf, 2);
    uint16_t ptr = (rx_rd_buf[0] << 8) | rx_rd_buf[1];
    uint16_t offset = ptr % RX_BUF_SIZE;
    uint16_t first_chunk = RX_BUF_SIZE - offset;
    if (first_chunk > read_len)
        first_chunk = read_len;
    uint16_t addr = SOCKET_RX_BUF_BASE(sock) + offset;
    if (w5500_spi_read(addr, sock, buffer, first_chunk) != 0) return -1;
    if (first_chunk < read_len) {
        uint16_t second_chunk = read_len - first_chunk;
        addr = SOCKET_RX_BUF_BASE(sock);
        if (w5500_spi_read(addr, sock, buffer + first_chunk, second_chunk) != 0) return -1;
    }
    ptr += read_len;
    uint8_t new_ptr[2] = { (uint8_t)(ptr >> 8), (uint8_t)(ptr & 0xFF) };
    socket_write(sock, Sn_RX_RD, new_ptr, 2);
    socket_write_reg(sock, Sn_CR, SOCKET_CMD_RECV);
    return read_len;
}

// --- Interrupt–Driven Socket Management ---
void w5500_socket_isr(uint8_t sock) {
    uint8_t ir = socket_read_reg(sock, Sn_IR);
    if (ir) {
        // Clear the interrupts by writing back the value.
        socket_write_reg(sock, Sn_IR, ir);
    }
    uint8_t status = socket_read_reg(sock, Sn_SR);
    switch(status) {
        case SOCK_ESTABLISHED:
            // In a real system, signal a task or set a flag.
            printf("Socket %d established.\n", sock);
            break;
        case SOCK_CLOSED:
            printf("Socket %d closed.\n", sock);
            break;
        default:
            // Other events can be handled here.
            break;
    }
}

void w5500_global_int_handler(unsigned int gpio, uint32_t events) {
    // This global handler is attached to W5500_INT.
    // For each socket, call its ISR.
    for (uint8_t sock = 0; sock < MAX_SOCKETS; sock++) {
        w5500_socket_isr(sock);
    }
}

// --- W5500 Initialization ---
int w5500_init(void) {
    spi_init(W5500_SPI_INSTANCE, SPI_SPEED); 
    gpio_set_function(W5500_MISO, GPIO_FUNC_SPI);
    gpio_set_function(W5500_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(W5500_SCK,  GPIO_FUNC_SPI);
    
    gpio_init(W5500_CS);
    gpio_set_dir(W5500_CS, GPIO_OUT);
    gpio_put(W5500_CS, 1);
    
    gpio_init(W5500_RESET);
    gpio_set_dir(W5500_RESET, GPIO_OUT);
    gpio_put(W5500_RESET, 1);
    
    // Configure the W5500 interrupt pin and attach the handler.
    gpio_init(W5500_INT);
    gpio_set_dir(W5500_INT, GPIO_IN);
    gpio_pull_up(W5500_INT);
    gpio_set_irq_enabled_with_callback(W5500_INT, GPIO_IRQ_EDGE_FALL, true, w5500_global_int_handler);
    
    w5500_reset();
    
    if (w5500_set_network(default_mac, default_ip, default_subnet, default_gateway) != 0) {
        printf("Failed to set network configuration.\n");
        return -1;
    }
    
    printf("W5500 initialized with IP %d.%d.%d.%d\n",
           default_ip[0], default_ip[1], default_ip[2], default_ip[3]);
    
    return 0;
}

// --- Flash File Access Stub ---
// Replace this stub with your file system implementation to load HTML files from flash.
static int load_html_file(const char *filename, char *buffer, int max_size) {
    if (strcmp(filename, "index.html") == 0) {
        const char *html_index_file =
            "<html>"
            "<head><title>RP2040 Web Server</title></head>"
            "<body>"
            "<h1>Welcome to RP2040 Web Server</h1>"
            "<p>This is the index page.</p>"
            "<a href=\"/page2\">Go to Page 2</a>"
            "</body>"
            "</html>";
        strncpy(buffer, html_index_file, max_size);
        return strlen(html_index_file);
    } else if (strcmp(filename, "page2.html") == 0) {
        const char *html_page2_file =
            "<html>"
            "<head><title>Page 2</title></head>"
            "<body>"
            "<h1>Page 2</h1>"
            "<p>This is the second page.</p>"
            "<a href=\"/\">Back to Home</a>"
            "</body>"
            "</html>";
        strncpy(buffer, html_page2_file, max_size);
        return strlen(html_page2_file);
    }
    return -1;
}

// --- HTTP Server Task ---
// This function uses the designated socket to accept connections and serve HTML files
// (loaded from flash) over HTTP.
void w5500_http_server_task(uint8_t sock) {
    if (w5500_socket_open(sock, SOCKET_MR_TCP, 80) != 0) {
        printf("Failed to open HTTP socket %d\n", sock);
        return;
    }
    
    if (socket_write_reg(sock, Sn_CR, SOCKET_CMD_LISTEN) != 0) {
        printf("Failed to put socket %d in LISTEN mode\n", sock);
        return;
    }
    
    uint32_t timeout = 1000;
    while(timeout--) {
        if (socket_read_reg(sock, Sn_SR) == SOCK_LISTEN)
            break;
        sleep_ms(1);
    }
    if (socket_read_reg(sock, Sn_SR) != SOCK_LISTEN) {
        printf("Socket %d did not enter LISTEN mode\n", sock);
        return;
    }
    
    printf("HTTP server listening on socket %d, port 80...\n", sock);
    
    uint8_t recv_buf[1024];
    char file_buffer[2048];
    
    while (1) {
        uint8_t status = socket_read_reg(sock, Sn_SR);
        if (status == SOCK_ESTABLISHED) {
            printf("Client connected on socket %d.\n", sock);
            memset(recv_buf, 0, sizeof(recv_buf));
            uint8_t rx_size_buf[2];
            socket_read(sock, Sn_RX_RSR, rx_size_buf, 2);
            uint16_t rx_size = (rx_size_buf[0] << 8) | rx_size_buf[1];
            if (rx_size > 0 && rx_size < sizeof(recv_buf)) {
                int len = w5500_socket_recv(sock, recv_buf, sizeof(recv_buf) - 1);
                if (len > 0) {
                    printf("Received request:\n%s\n", recv_buf);
                    const char *filename = (strstr((char *)recv_buf, "GET /page2") != NULL) ?
                        "page2.html" : "index.html";
                    int file_len = load_html_file(filename, file_buffer, sizeof(file_buffer));
                    if (file_len < 0) {
                        snprintf(file_buffer, sizeof(file_buffer),
                                 "<html><body><h1>404 Not Found</h1></body></html>");
                        file_len = strlen(file_buffer);
                    }
                    
                    char http_response[4096];
                    snprintf(http_response, sizeof(http_response),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: %d\r\n"
                             "\r\n%s", file_len, file_buffer);
                    
                    if (w5500_socket_send(sock, (uint8_t *)http_response, strlen(http_response)) != 0) {
                        printf("Failed to send HTTP response on socket %d\n", sock);
                    }
                }
            }
            w5500_socket_close(sock);
            // Reopen the socket for the next connection.
            w5500_socket_open(sock, SOCKET_MR_TCP, 80);
            socket_write_reg(sock, Sn_CR, SOCKET_CMD_LISTEN);
            sleep_ms(10);
        }
        sleep_ms(50);
    }
}

// --- PDU Command Processing ---
void w5500_process_pdu_command(uint8_t command) {
    switch (command) {
        case 1:
            printf("PDU Command Debug 1 executed.\n");
            break;
        case 2:
            printf("PDU Command Debug 2 executed.\n");
            break;
        default:
            printf("Unknown PDU command: %d\n", command);
            break;
    }
}
