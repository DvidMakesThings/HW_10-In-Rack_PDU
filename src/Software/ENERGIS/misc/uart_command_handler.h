#ifndef UART_COMMAND_HANDLER_H
#define UART_COMMAND_HANDLER_H

#include "CONFIG.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include <string.h>

/**
 * Poll UART for a complete line and echo it back.
 * Call this each iteration of your main loop.
 */
void uart_command_loop(void);
void bootsel_trigger(void);

void reboot(void);

void dump_eeprom(void);

void set_ip(const char *ip, const char *cmd);
#endif // UART_COMMAND_HANDLER_H
