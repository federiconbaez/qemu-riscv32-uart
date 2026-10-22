/* SPDX-License-Identifier: Apache-2.0 */
/* Freestanding formatting helpers. No libc; host-testable. */
#ifndef FMT_H
#define FMT_H

#include <stddef.h>
#include <stdint.h>

/* Writes up to cap-1 chars plus NUL; returns the length the full output
 * would have had (like snprintf). Supports %s %c %d %u %x %08x-style zero
 * padding and width, %p and %%. */
int fmt_snprintf(char *buf, size_t cap, const char *f, ...) __attribute__((format(printf, 3, 4)));

#include <stdarg.h>
int fmt_vsnprintf(char *buf, size_t cap, const char *f, va_list ap);

/* Parse a signed decimal or 0x-prefixed hex integer. Returns 0 on success,
 * -1 on syntax error or overflow of int32. *end points past the number. */
int fmt_parse_i32(const char *s, int32_t *out, const char **end);

/* 64/32 division without libgcc/compiler-rt (RV32 has no 64-bit divide).
 * Restoring shift-subtract; d must be non-zero. */
uint64_t fmt_udiv64(uint64_t n, uint32_t d, uint32_t *rem);

#endif
