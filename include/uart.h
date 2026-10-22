/* SPDX-License-Identifier: Apache-2.0 */
#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>

void uart_init(uint32_t baud);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc_nonblock(void); /* -1 if no byte */
char uart_getc(void);
int uart_printf(const char *f, ...) __attribute__((format(printf, 1, 2)));
/* Reads a line (without '\n') with echo and backspace handling. Returns
 * its length; at most cap-1 bytes are kept. */
size_t uart_readline(char *buf, size_t cap);
/* Toggle the 16550 internal loopback (MCR bit 4). */
void uart_set_loopback(int on);

#endif
