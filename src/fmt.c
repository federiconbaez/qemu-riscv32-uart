/* SPDX-License-Identifier: Apache-2.0 */
#include "fmt.h"

struct out {
  char *buf;
  size_t cap;
  size_t len;
};

static void put(struct out *o, char c) {
  if (o->len + 1 < o->cap) o->buf[o->len] = c;
  o->len++;
}

static void put_u(struct out *o, uint32_t v, unsigned base, int width, char pad, int neg) {
  char tmp[11];
  int n = 0;
  do {
    unsigned d = v % base;
    tmp[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    v /= base;
  } while (v);
  int total = n + neg;
  if (neg && pad == '0') put(o, '-');
  for (; total < width; total++) put(o, pad);
  if (neg && pad != '0') put(o, '-');
  while (n) put(o, tmp[--n]);
}

int fmt_vsnprintf(char *buf, size_t cap, const char *f, va_list ap) {
  struct out o = {buf, cap, 0};
  for (; *f; f++) {
    if (*f != '%') {
      put(&o, *f);
      continue;
    }
    f++;
    char pad = ' ';
    int width = 0;
    if (*f == '0') {
      pad = '0';
      f++;
    }
    while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
    switch (*f) {
      case 'd': {
        int32_t v = va_arg(ap, int32_t);
        uint32_t u = v < 0 ? (uint32_t)0 - (uint32_t)v : (uint32_t)v;
        put_u(&o, u, 10, width, pad, v < 0);
        break;
      }
      case 'u':
        put_u(&o, va_arg(ap, uint32_t), 10, width, pad, 0);
        break;
      case 'x':
        put_u(&o, va_arg(ap, uint32_t), 16, width, pad, 0);
        break;
      case 'p':
        put(&o, '0');
        put(&o, 'x');
        put_u(&o, (uint32_t)(uintptr_t)va_arg(ap, void *), 16, 8, '0', 0);
        break;
      case 'c':
        put(&o, (char)va_arg(ap, int));
        break;
      case 's': {
        const char *s = va_arg(ap, const char *);
        if (!s) s = "(null)";
        int n = 0;
        while (s[n]) n++;
        for (; n < width; width--) put(&o, ' ');
        while (*s) put(&o, *s++);
        break;
      }
      case '%':
        put(&o, '%');
        break;
      case '\0':
        f--; /* trailing '%': stop at the terminator */
        break;
      default:
        put(&o, '%');
        put(&o, *f);
    }
  }
  if (cap) buf[o.len < cap ? o.len : cap - 1] = '\0';
  return (int)o.len;
}

int fmt_snprintf(char *buf, size_t cap, const char *f, ...) {
  va_list ap;
  va_start(ap, f);
  int n = fmt_vsnprintf(buf, cap, f, ap);
  va_end(ap);
  return n;
}

int fmt_parse_i32(const char *s, int32_t *out, const char **end) {
  int neg = 0;
  if (*s == '-' || *s == '+') neg = *s++ == '-';
  unsigned base = 10;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    base = 16;
    s += 2;
  }
  uint64_t v = 0;
  int digits = 0;
  for (;; s++, digits++) {
    unsigned d;
    if (*s >= '0' && *s <= '9') d = (unsigned)(*s - '0');
    else if (base == 16 && *s >= 'a' && *s <= 'f') d = (unsigned)(*s - 'a' + 10);
    else if (base == 16 && *s >= 'A' && *s <= 'F') d = (unsigned)(*s - 'A' + 10);
    else break;
    v = v * base + d;
    if (v > (uint64_t)INT32_MAX + (uint64_t)neg) return -1;
  }
  if (!digits) return -1;
  *out = neg ? (int32_t)(0 - v) : (int32_t)v;
  if (end) *end = s;
  return 0;
}

uint64_t fmt_udiv64(uint64_t n, uint32_t d, uint32_t *rem) {
  uint64_t q = 0, r = 0;
  for (int i = 63; i >= 0; i--) {
    r = (r << 1) | ((n >> i) & 1u);
    if (r >= d) {
      r -= d;
      q |= (uint64_t)1 << i;
    }
  }
  if (rem) *rem = (uint32_t)r;
  return q;
}
