/* SPDX-License-Identifier: Apache-2.0 */
/* Memory map of QEMU's `virt` machine (hw/riscv/virt.c). */
#ifndef HW_H
#define HW_H

#include <stdint.h>

#define UART0_BASE 0x10000000u  /* NS16550A, IRQ 10 on the PLIC */
#define TEST_BASE 0x00100000u   /* sifive_test finisher */
#define CLINT_BASE 0x02000000u
#define CLINT_MTIMECMP (CLINT_BASE + 0x4000u)
#define CLINT_MTIME (CLINT_BASE + 0xbff8u)
#define MTIME_HZ 10000000u      /* virt timebase-frequency */

#define TEST_PASS 0x5555u
#define TEST_FAIL 0x3333u

static inline void mmio_w8(uint32_t a, uint8_t v) { *(volatile uint8_t *)(uintptr_t)a = v; }
static inline uint8_t mmio_r8(uint32_t a) { return *(volatile uint8_t *)(uintptr_t)a; }
static inline void mmio_w32(uint32_t a, uint32_t v) { *(volatile uint32_t *)(uintptr_t)a = v; }
static inline uint32_t mmio_r32(uint32_t a) { return *(volatile uint32_t *)(uintptr_t)a; }

#define csr_read(csr)                                   \
  ({                                                    \
    uint32_t __v;                                       \
    __asm__ volatile("csrr %0, " #csr : "=r"(__v));     \
    __v;                                                \
  })
#define csr_write(csr, v) __asm__ volatile("csrw " #csr ", %0" ::"r"(v))
#define csr_set(csr, v) __asm__ volatile("csrs " #csr ", %0" ::"r"(v))
#define csr_clear(csr, v) __asm__ volatile("csrc " #csr ", %0" ::"r"(v))

#define MSTATUS_MIE (1u << 3)
#define MIE_MTIE (1u << 7)
#define MCAUSE_INT (1u << 31)

/* Ask QEMU to exit with status 0 (pass) or `code` (fail). Never returns. */
__attribute__((noreturn)) void qemu_exit(uint32_t code);

#endif
