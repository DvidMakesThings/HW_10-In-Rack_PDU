#ifndef UART_COMMAND_HANDLER_H
#define UART_COMMAND_HANDLER_H

#include "CONFIG.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include <string.h>

void uart_command_init(void);        // Call this during startup
void uart_command_irq_handler(void); // UART IRQ callback

#endif // UART_COMMAND_HANDLER_H
