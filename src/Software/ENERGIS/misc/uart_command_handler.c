#include "uart_command_handler.h"

static char command_buf[UART_CMD_BUF_LEN];
static volatile int cmd_index = 0;
volatile bool bootsel_requested = false;

static void process_command(const char *cmd) {
    if (strcmp(cmd, "ENTER_BOOTSEL") == 0) {
        bootsel_requested = true;
    }
    // Add more commands here later...
}

void uart_command_irq_handler(void) {
    while (uart_is_readable(UART_ID)) {
        char ch = uart_getc(UART_ID);
        if (ch == '\r' || ch == '\n') {
            command_buf[cmd_index] = '\0';
            process_command(command_buf);
            cmd_index = 0;
        } else if (cmd_index < UART_CMD_BUF_LEN - 1) {
            command_buf[cmd_index++] = ch;
        } else {
            cmd_index = 0; // overflow, reset
        }
    }
}

void uart_command_init(void) {
    irq_set_exclusive_handler(UART0_IRQ, uart_command_irq_handler);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(UART_ID, true, false); // RX interrupt only
}
