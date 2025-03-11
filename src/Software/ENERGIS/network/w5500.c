#include "w5500.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdio.h>
#include "../CONFIG.h"

/*===========================================================================
  Low-level SPI functions
===========================================================================*/
void w5500_select(void) {
    gpio_put(W5500_CS, 0);
}

void w5500_deselect(void) {
    gpio_put(W5500_CS, 1);
}

/*===========================================================================
  SPI register access functions
===========================================================================*/
void w5500_write_reg(uint16_t addr, uint8_t value) {
    uint8_t tx_buf[4];
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = 0x04; // Control byte: Write, VDM mode
    tx_buf[3] = value;
    w5500_select();
    spi_write_blocking(W5500_SPI_INSTANCE, tx_buf, 4);
    w5500_deselect();
}

uint8_t w5500_read_reg(uint16_t addr) {
    uint8_t tx_buf[3];
    uint8_t rx;
    tx_buf[0] = (addr >> 8) & 0xFF;
    tx_buf[1] = addr & 0xFF;
    tx_buf[2] = 0x00; // Control byte: Read, VDM mode
    w5500_select();
    spi_write_blocking(W5500_SPI_INSTANCE, tx_buf, 3);
    spi_read_blocking(W5500_SPI_INSTANCE, 0xFF, &rx, 1);
    w5500_deselect();
    return rx;
}

void w5500_write_16(uint16_t addr, uint16_t value) {
    w5500_write_reg(addr, (value >> 8) & 0xFF);
    w5500_write_reg(addr + 1, value & 0xFF);
}

uint16_t w5500_read_16(uint16_t addr) {
    uint16_t high = w5500_read_reg(addr);
    uint16_t low  = w5500_read_reg(addr + 1);
    return (high << 8) | low;
}

void w5500_Send(uint16_t offset, uint8_t block_select, uint8_t write, uint8_t *buffer, uint16_t len) {
    // Build the header: 16-bit address + 1 control byte
    uint8_t header[3];
    header[0] = (offset >> 8) & 0xFF;   // High byte of address
    header[1] = offset & 0xFF;          // Low byte of address
    
    // Control byte:
    // - block_select determines the register bank (common or socket)
    // - bit 2: set to 1 for write, 0 for read
    // - bits [1:0]: define access size (1, 2, or 4 bytes)
    uint8_t len_bits = (len == 1) ? 0x01 : (len == 2) ? 0x02 : 0x03;
    header[2] = block_select | (write ? 0x04 : 0x00) | len_bits;

    // Assert chip select
    gpio_put(W5500_CS, 0);

    // Send the header
    spi_write_blocking(W5500_SPI_INSTANCE, header, 3);

    if (write) {
        // Write the data
        spi_write_blocking(W5500_SPI_INSTANCE, buffer, len);
    } else {
        // Read data into buffer
        spi_read_blocking(W5500_SPI_INSTANCE, 0xFF, buffer, len);
    }

    // Deassert chip select
    gpio_put(W5500_CS, 1);
}

void w5500_send_socket_command(uint8_t socket, uint8_t cmd) {
    w5500_Send(W5500_SREG_CR_OFFSET, W5500_BLOCK_S_REG(socket), 1, &cmd, 1);
}

bool w5500_check_phy(void) {
    uint8_t phycfgr;

    // Read current PHYCFGR for debugging.
    phycfgr = w5500_read_reg(W5500_REG_PHYCFGR);
    printf("[W5500-PHY] PHYCFGR before reset: 0x%02X\n", phycfgr);

    // Force a PHY reset by clearing the RST bit (bit 7)
    phycfgr &= ~(1 << 7);
    w5500_write_reg(W5500_REG_PHYCFGR, phycfgr);
    sleep_ms(50);  // Wait for the PHY to reset

    // Now configure the PHY:
    // Set RST bit to indicate no reset is in progress.
    // Set OPMD (bit 6) so that OPMDC bits are used.
    // Clear bits 5-3 (OPMDC) and set them to 0b111 (All capable, Auto-negotiation enabled).
    phycfgr |= (1 << 7);      // Set RST bit
    phycfgr |= (1 << 6);      // Set OPMD bit
    phycfgr &= ~0x38;         // Clear OPMDC bits (bits 5-3)
    phycfgr |= (0x07 << 3);    // Set OPMDC to 0b111

    // Write the new configuration to PHYCFGR.
    w5500_write_reg(W5500_REG_PHYCFGR, phycfgr);

    // Poll until link is up or until 10 seconds have elapsed.
    const uint32_t timeout = 10000;   // 10 seconds in ms
    const uint32_t poll_interval = 100; // poll every 100ms
    uint32_t elapsed = 0;
    do {
        sleep_ms(poll_interval);
        elapsed += poll_interval;
        phycfgr = w5500_read_reg(W5500_REG_PHYCFGR);
        if (phycfgr & 0x01) {
            printf("[W5500-PHY] PHY link is up after %lu ms.\n\n", elapsed);
            return true;
        }
    } while (elapsed < timeout);
}

/*===========================================================================
  Socket memory configuration: write a 14-byte configuration block into
  the socket’s RX buffer size register area.
===========================================================================*/
bool w5500_socket_memory_setup(uint8_t socket) {
    uint8_t socket_cfg[14] = { 
        W5500_SOCKET_MEM, W5500_SOCKET_MEM, 0x80,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 
    };
    // Calculate the base address for the socket's RX buffer configuration.
    uint16_t base_addr = W5500_SOCKET_REG_BASE(socket) + W5500_SREG_RXBUF_SIZE;
    bool success = true;
    uint8_t reg_value;

    // Print the base address for this socket.
    DEBUG_PRINT("\n[W5500-memory] Socket %d: Base Address = 0x%04X\n", socket, base_addr);

    // Write the configuration values.
    for (uint8_t i = 0; i < 14; i++) {
         w5500_write_reg(base_addr + i, socket_cfg[i]);
    }

    // Verify and print the actual register values.
    DEBUG_PRINT("[W5500-memory] Socket %d: Actual register values:\n", socket);
    for (uint8_t i = 0; i < 14; i++) {
         reg_value = w5500_read_reg(base_addr + i);
         DEBUG_PRINT("  Addr 0x%04X: 0x%02X\n", base_addr + i, reg_value);
         if (reg_value != socket_cfg[i]) {
             DEBUG_PRINT("[W5500-memory] Socket %d: Verification failed at offset %d: expected 0x%02X, got 0x%02X\n",
                    socket, i, socket_cfg[i], reg_value);
             success = false;
         }
    }
    if (success) {
         DEBUG_PRINT("[W5500-memory] Socket %d: Memory configured successfully\n", socket);
    }
    return success;
}

bool w5500_basic_register_setup(void) {
    bool success = true;
    uint8_t reg_val;

    // --- Mode Register (MR) ---
    // For normal operation, MR is typically set to 0x00 (ensure reset bit is cleared).
    uint8_t expected_MR = 0x00;
    w5500_write_reg(W5500_REG_MODE, expected_MR);
    reg_val = w5500_read_reg(W5500_REG_MODE);
    printf("[W5500-basic] MR (Mode Register) at address 0x%04X: Expected 0x%02X, Read 0x%02X\n", 
           W5500_REG_MODE, expected_MR, reg_val);
    if (reg_val != expected_MR) {
        success = false;
    }

    // --- Interrupt Mask Register (IMR) ---
    // For example, enable all interrupts (0xFF) or adjust to your needs.
    uint8_t expected_IMR = 0xFF;
    w5500_write_reg(W5500_REG_IMR, expected_IMR);
    reg_val = w5500_read_reg(W5500_REG_IMR);
    printf("[W5500-basic] IMR (Interrupt Mask Register) at address 0x%04X: Expected 0x%02X, Read 0x%02X\n", 
           W5500_REG_IMR, expected_IMR, reg_val);
    if (reg_val != expected_IMR) {
        success = false;
    }

    // --- Retry Time-value Register (RTR) ---
    // This register is 16 bits wide. For example, setting a retry time value of 0x07D0.
    uint8_t expected_RTR_high = 0x07;
    uint8_t expected_RTR_low  = 0xD0;
    w5500_write_reg(W5500_REG_RTR, expected_RTR_high);
    w5500_write_reg(W5500_REG_RTR + 1, expected_RTR_low);
    uint8_t read_RTR_high = w5500_read_reg(W5500_REG_RTR);
    uint8_t read_RTR_low  = w5500_read_reg(W5500_REG_RTR + 1);
    printf("[W5500-basic] RTR (Retry Time-value Register) at addresses 0x%04X/0x%04X: Expected 0x%02X 0x%02X, Read 0x%02X 0x%02X\n",
           W5500_REG_RTR, W5500_REG_RTR + 1, expected_RTR_high, expected_RTR_low, read_RTR_high, read_RTR_low);
    if (read_RTR_high != expected_RTR_high || read_RTR_low != expected_RTR_low) {
        success = false;
    }

    // --- Retry Count Register (RCR) ---
    // For example, set the retry count to 8.
    uint8_t expected_RCR = 0x08;
    w5500_write_reg(W5500_REG_RCR, expected_RCR);
    reg_val = w5500_read_reg(W5500_REG_RCR);
    printf("[W5500-basic] RCR (Retry Count Register) at address 0x%04X: Expected 0x%02X, Read 0x%02X\n",
           W5500_REG_RCR, expected_RCR, reg_val);
    if (reg_val != expected_RCR) {
        success = false;
    }

    if (success) {
        printf("[W5500-basic] Basic register configuration verified successfully.\n");
    } else {
        printf("[W5500-basic] Basic register configuration verification FAILED.\n");
    }
    return success;
}


/*===========================================================================
  Network initialization and configuration
===========================================================================*/
int w5500_init(uint8_t *gateway, uint8_t *subnet, uint8_t *mac, uint8_t *ip) {
    // Check chip version
    uint8_t ver = w5500_read_reg(W5500_REG_VERSIONR);
    if (ver != 0x04) {
        printf("[W5500-init] Version mismatch: 0x%02X\n", ver);
        return -1;
    } else {
        printf("[W5500-init] Version 0x%02X\n", ver);
    }
    
    // Check PHY link
    if (!w5500_check_phy()) {
        printf("[W5500-init] PHY link down!\n");
        return -1;
    } else {
        printf("[W5500-init] PHY link up.\n");
    }

    // Basic register configuration with debug verification
    if (!w5500_basic_register_setup()) {
        printf("[W5500-init] Basic register configuration verification FAILED.\n");
        return -1;
    }
    
    // Configure socket memory for all sockets and verify configuration.
    bool mem_success = true;
    for (uint8_t s = 0; s < W5500_MAX_SOCKETS; s++) {
         if (!w5500_socket_memory_setup(s)) {
             mem_success = false;
         }
    }
    if (!mem_success) {
         printf("[W5500-init] Memory configuration failed for one or more sockets.\n");
         return -1;
    }
    
    // Set network information if provided
    if (gateway) {
        for (int i = 0; i < 4; i++) {
            w5500_write_reg(W5500_REG_GAR + i, gateway[i]);
        }
        printf("[W5500-init] Gateway set to %d.%d.%d.%d\n", gateway[0], gateway[1], gateway[2], gateway[3]);
    }
    if (subnet) {
        for (int i = 0; i < 4; i++) {
            w5500_write_reg(W5500_REG_SUBR + i, subnet[i]);
        }
        printf("[W5500-init] Subnet set to %d.%d.%d.%d\n", subnet[0], subnet[1], subnet[2], subnet[3]);
    }
    if (mac) {
        for (int i = 0; i < 6; i++) {
            w5500_write_reg(W5500_REG_SHAR + i, mac[i]);
        }
        printf("[W5500-init] MAC set to %02X:%02X:%02X:%02X:%02X:%02X\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    if (ip) {
        for (int i = 0; i < 4; i++) {
            w5500_write_reg(W5500_REG_SIPR + i, ip[i]);
        }
        printf("[W5500-init] IP set to %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    }
    printf("[W5500-init] Initialization complete.\n\n");
    return 0;
}

void w5500_set_mac(const uint8_t* mac) {
    for (int i = 0; i < 6; i++) {
        w5500_write_reg(W5500_REG_SHAR + i, mac[i]);
    }
    printf("[W5500-init] MAC set.\n");
}

void w5500_set_ip(const uint8_t* ip) {
    for (int i = 0; i < 4; i++) {
        w5500_write_reg(W5500_REG_SIPR + i, ip[i]);
    }
    printf("[W5500-init] IP set.\n");
}

void w5500_set_subnet(const uint8_t* subnet) {
    for (int i = 0; i < 4; i++) {
        w5500_write_reg(W5500_REG_SUBR + i, subnet[i]);
    }
    printf("[W5500-init] Subnet set.\n");
}

void w5500_set_gateway(const uint8_t* gateway) {
    for (int i = 0; i < 4; i++) {
        w5500_write_reg(W5500_REG_GAR + i, gateway[i]);
    }
    printf("[W5500-init] Gateway set.\n");
}
