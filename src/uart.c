/* SPDX-License-Identifier: Apache-2.0 */
/* Polled NS16550A driver. */
#include "uart.h"

#include <stdarg.h>

#include "fmt.h"
#include "hw.h"

enum { RBR = 0, THR = 0, DLL = 0, IER = 1, DLM = 1, FCR = 2, LCR = 3, MCR = 4, LSR = 5 };
#define LSR_DR 0x01u
#define LSR_THRE 0x20u
#define LCR_DLAB 0x80u
#define LCR_8N1 0x03u
#define UART_CLK 3686400u /* QEMU virt advertises this clock in the DT */

static inline void w(unsigned r, uint8_t v) { mmio_w8(UART0_BASE + r, v); }
static inline uint8_t rd(unsigned r) { return mmio_r8(UART0_BASE + r); }

void uart_init(uint32_t baud) {
  uint32_t div = UART_CLK / (16u * baud);
  w(IER, 0);
  w(LCR, LCR_DLAB);
  w(DLL, (uint8_t)(div & 0xff));
  w(DLM, (uint8_t)(div >> 8));
  w(LCR, LCR_8N1);
  w(FCR, 0x07); /* enable + clear FIFOs */
  w(MCR, 0x03); /* DTR | RTS */
}

void uart_putc(char c) {
  if (c == '\n') uart_putc('\r');
  while (!(rd(LSR) & LSR_THRE)) {
  }
  w(THR, (uint8_t)c);
}

void uart_puts(const char *s) {
  while (*s) uart_putc(*s++);
}

int uart_getc_nonblock(void) { return (rd(LSR) & LSR_DR) ? rd(RBR) : -1; }

char uart_getc(void) {
  int c;
  while ((c = uart_getc_nonblock()) < 0) {
    __asm__ volatile("nop");
  }
  return (char)c;
}

int uart_printf(const char *f, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, f);
  int n = fmt_vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  uart_puts(buf);
  return n;
}

size_t uart_readline(char *buf, size_t cap) {
  size_t n = 0;
  for (;;) {
    char c = uart_getc();
    if (c == '\r' || c == '\n') {
      uart_putc('\n');
      break;
    }
    if ((c == 0x7f || c == 0x08)) {
      if (n) {
        n--;
        uart_puts("\b \b");
      }
      continue;
    }
    if (n + 1 < cap) {
      buf[n++] = c;
      uart_putc(c);
    }
  }
  buf[n] = '\0';
  return n;
}

void uart_set_loopback(int on) {
  uint8_t m = rd(MCR);
  w(MCR, on ? (uint8_t)(m | 0x10) : (uint8_t)(m & ~0x10));
}
