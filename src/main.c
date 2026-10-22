/* SPDX-License-Identifier: Apache-2.0 */
/* Bare-metal RV32 firmware for QEMU `virt`: UART console, CLINT timer
 * interrupts, synchronous trap handling and a small command shell. */
#include <stddef.h>
#include <stdint.h>

#include "fmt.h"
#include "hw.h"
#include "uart.h"

int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);

#define TICK_HZ 100u

static volatile uint32_t ticks;
static volatile uint32_t ecalls;
static volatile uint32_t illegal;
static volatile uint32_t last_mcause;

__attribute__((noreturn)) void qemu_exit(uint32_t code) {
  mmio_w32(TEST_BASE, code == 0 ? TEST_PASS : (code << 16) | TEST_FAIL);
  for (;;) __asm__ volatile("wfi");
}

static uint64_t mtime(void) {
  uint32_t hi, lo;
  do {
    hi = mmio_r32(CLINT_MTIME + 4);
    lo = mmio_r32(CLINT_MTIME);
  } while (hi != mmio_r32(CLINT_MTIME + 4));
  return ((uint64_t)hi << 32) | lo;
}

static void set_mtimecmp(uint64_t t) {
  mmio_w32(CLINT_MTIMECMP + 4, 0xffffffffu); /* no spurious match while updating */
  mmio_w32(CLINT_MTIMECMP, (uint32_t)t);
  mmio_w32(CLINT_MTIMECMP + 4, (uint32_t)(t >> 32));
}

static uint64_t next_deadline;

__attribute__((interrupt("machine"), aligned(4))) static void trap_entry(void) {
  uint32_t cause = csr_read(mcause);
  last_mcause = cause;
  if (cause == (MCAUSE_INT | 7u)) { /* machine timer */
    ticks = ticks + 1;
    next_deadline += MTIME_HZ / TICK_HZ;
    set_mtimecmp(next_deadline);
    return;
  }
  uint32_t epc = csr_read(mepc);
  uint16_t insn = *(volatile uint16_t *)(uintptr_t)epc;
  uint32_t len = (insn & 3u) == 3u ? 4u : 2u;
  if (cause == 11u) {
    ecalls = ecalls + 1;
  } else if (cause == 2u) {
    illegal = illegal + 1;
  } else {
    uart_printf("\nfatal trap: mcause=%x mepc=%x mtval=%x\n", cause, epc, csr_read(mtval));
    qemu_exit(100);
  }
  csr_write(mepc, epc + len);
}

static void timer_start(void) {
  next_deadline = mtime() + MTIME_HZ / TICK_HZ;
  set_mtimecmp(next_deadline);
  csr_set(mie, MIE_MTIE);
  csr_set(mstatus, MSTATUS_MIE);
}

/* ---- self tests ------------------------------------------------------ */

static int st_fmt(void) {
  char b[32];
  fmt_snprintf(b, sizeof b, "%d|%08x|%s", -5, 0xbeefu, "ok");
  return strcmp(b, "-5|0000beef|ok") == 0;
}

static int st_ecall(void) {
  uint32_t before = ecalls;
  __asm__ volatile("ecall");
  __asm__ volatile("ecall");
  return ecalls == before + 2;
}

static int st_illegal(void) {
  uint32_t before = illegal;
  __asm__ volatile(".4byte 0xffffffff"); /* 32-bit illegal encoding */
  __asm__ volatile(".2byte 0x0000");     /* 16-bit illegal (c.unimp) */
  return illegal == before + 2 && last_mcause == 2u;
}

static int st_loopback(void) {
  /* In loopback the 16550 routes THR to RBR without touching the line. */
  while (uart_getc_nonblock() >= 0) {
  }
  uart_set_loopback(1);
  const char *msg = "LOOP";
  int ok = 1;
  for (const char *p = msg; *p; p++) {
    mmio_w8(UART0_BASE, (uint8_t)*p);
    int c = -1;
    for (int spin = 0; spin < 100000 && c < 0; spin++) c = uart_getc_nonblock();
    if (c != *p) ok = 0;
  }
  uart_set_loopback(0);
  return ok;
}

static int st_timer(void) {
  uint32_t start = ticks;
  uint64_t t0 = mtime();
  while (ticks - start < 5) __asm__ volatile("wfi");
  uint64_t dt = mtime() - t0;
  /* 5 ticks at 100 Hz: at least 4 full periods must have elapsed. */
  return dt >= 4u * (MTIME_HZ / TICK_HZ) && dt < 50u * (MTIME_HZ / TICK_HZ);
}

struct selftest {
  const char *name;
  int (*fn)(void);
};

static const struct selftest tests[] = {
    {"fmt", st_fmt}, {"ecall", st_ecall}, {"illegal", st_illegal}, {"loopback", st_loopback}, {"timer", st_timer},
};

/* ---- shell ------------------------------------------------------------ */

static const char *skip_ws(const char *s) {
  while (*s == ' ') s++;
  return s;
}

static void cmd_add(const char *args) {
  int32_t a, b;
  const char *p = skip_ws(args);
  if (fmt_parse_i32(p, &a, &p) || fmt_parse_i32(skip_ws(p), &b, &p) || *skip_ws(p)) {
    uart_puts("error: usage add <i32> <i32>\n");
    return;
  }
  int32_t r;
  if (__builtin_add_overflow(a, b, &r)) {
    uart_puts("error: overflow\n");
    return;
  }
  uart_printf("%d\n", r);
}

__attribute__((noreturn, unused)) static void shell(void) {
  char line[96];
  for (;;) {
    uart_puts("> ");
    uart_readline(line, sizeof line);
    const char *cmd = skip_ws(line);
    if (!*cmd) continue;
    if (!strcmp(cmd, "ping")) {
      uart_puts("pong\n");
    } else if (!strncmp(cmd, "add ", 4)) {
      cmd_add(cmd + 4);
    } else if (!strcmp(cmd, "ticks")) {
      uart_printf("ticks=%u\n", ticks);
    } else if (!strcmp(cmd, "uptime")) {
      uart_printf("uptime_ms=%u\n", (uint32_t)fmt_udiv64(mtime(), MTIME_HZ / 1000u, NULL));
    } else if (!strcmp(cmd, "traps")) {
      uart_printf("ecall=%u illegal=%u\n", ecalls, illegal);
    } else if (!strncmp(cmd, "echo ", 5)) {
      uart_printf("%s\n", cmd + 5);
    } else if (!strcmp(cmd, "help")) {
      uart_puts("ping | add a b | ticks | uptime | traps | echo txt | exit | fail n\n");
    } else if (!strcmp(cmd, "exit")) {
      uart_puts("bye\n");
      qemu_exit(0);
    } else if (!strncmp(cmd, "fail ", 5)) {
      int32_t code;
      if (fmt_parse_i32(skip_ws(cmd + 5), &code, NULL) || code < 1 || code > 255) {
        uart_puts("error: fail needs 1..255\n");
        continue;
      }
      uart_printf("failing with %d\n", code);
      qemu_exit((uint32_t)code);
    } else {
      uart_printf("unknown command: %s\n", cmd);
    }
  }
}

int main(void) {
  uart_init(115200);
  csr_write(mtvec, (uint32_t)(uintptr_t)trap_entry);
  uart_printf("rv32 uart demo (hart %u, misa=%x)\n", csr_read(mhartid), csr_read(misa));
  timer_start();
  unsigned passed = 0, n = sizeof tests / sizeof tests[0];
  for (unsigned i = 0; i < n; i++) {
    int ok = tests[i].fn();
    passed += (unsigned)ok;
    uart_printf("  %s: %s\n", tests[i].name, ok ? "ok" : "FAIL");
  }
  uart_printf("selftest: %u/%u passed\n", passed, n);
#ifdef AUTO_EXIT
  return passed == n ? 0 : 1;
#else
  uart_puts("ready\n");
  shell();
#endif
}
