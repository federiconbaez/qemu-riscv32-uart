/* SPDX-License-Identifier: Apache-2.0 */
/* Host-side unit tests for the freestanding helpers, compared against libc. */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "fmt.h"
#include "ring.h"

static int fails;
#define CHECK(c)                                                  \
  do {                                                            \
    if (!(c)) {                                                   \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);     \
      fails++;                                                    \
    }                                                             \
  } while (0)

#define SAME(...)                                                 \
  do {                                                            \
    char a[64], b[64];                                            \
    int na = fmt_snprintf(a, sizeof a, __VA_ARGS__);              \
    int nb = snprintf(b, sizeof b, __VA_ARGS__);                  \
    if (na != nb || strcmp(a, b)) {                               \
      fprintf(stderr, "%s:%d: fmt=\"%s\"(%d) libc=\"%s\"(%d)\n", __FILE__, __LINE__, a, na, b, nb); \
      fails++;                                                    \
    }                                                             \
  } while (0)

static void test_fmt(void) {
  SAME("plain");
  SAME("%d|%d|%d", 0, -1, 42);
  SAME("%d %d", INT_MIN, INT_MAX);
  SAME("%u %u", 0u, 4294967295u);
  SAME("%x %08x %8x", 0xdeadbeefu, 0x1fu, 0xabcu);
  SAME("%5d|%05d", -42, -42);
  SAME("%s|%5s|%c%%", "ok", "ab", 'Z');
  SAME("%3d", 12345);
  /* truncation keeps snprintf semantics */
  char small[5];
  CHECK(fmt_snprintf(small, sizeof small, "%d", 123456) == 6);
  CHECK(strcmp(small, "1234") == 0);
  CHECK(fmt_snprintf(NULL, 0, "abc") == 3);
  char p[16];
  fmt_snprintf(p, sizeof p, "%p", (void *)(uintptr_t)0x1234);
  CHECK(strcmp(p, "0x00001234") == 0);
  char t[8];
  CHECK(fmt_snprintf(t, sizeof t, "a%") == 1 && strcmp(t, "a") == 0);
}

static void test_parse(void) {
  int32_t v;
  const char *end;
  CHECK(fmt_parse_i32("123 rest", &v, &end) == 0 && v == 123 && strcmp(end, " rest") == 0);
  CHECK(fmt_parse_i32("-2147483648", &v, NULL) == 0 && v == INT_MIN);
  CHECK(fmt_parse_i32("2147483647", &v, NULL) == 0 && v == INT_MAX);
  CHECK(fmt_parse_i32("2147483648", &v, NULL) == -1);
  CHECK(fmt_parse_i32("-2147483649", &v, NULL) == -1);
  CHECK(fmt_parse_i32("0x7fffffff", &v, NULL) == 0 && v == INT_MAX);
  CHECK(fmt_parse_i32("0xFF", &v, NULL) == 0 && v == 255);
  CHECK(fmt_parse_i32("0x", &v, NULL) == -1);
  CHECK(fmt_parse_i32("", &v, NULL) == -1);
  CHECK(fmt_parse_i32("abc", &v, NULL) == -1);
  CHECK(fmt_parse_i32("99999999999999999999", &v, NULL) == -1);
}

static void test_udiv64(void) {
  const uint64_t ns[] = {0, 1, 9999, 10000, 0xffffffffull, 0x100000000ull, 0x123456789abcdefull, UINT64_MAX};
  const uint32_t ds[] = {1, 3, 7, 10000, 0x80000000u, UINT32_MAX};
  for (unsigned i = 0; i < sizeof ns / sizeof ns[0]; i++)
    for (unsigned j = 0; j < sizeof ds / sizeof ds[0]; j++) {
      uint32_t r;
      uint64_t q = fmt_udiv64(ns[i], ds[j], &r);
      CHECK(q == ns[i] / ds[j] && r == ns[i] % ds[j]);
    }
  /* pseudo-random sweep */
  uint64_t x = 0x9e3779b97f4a7c15ull;
  for (int k = 0; k < 100000; k++) {
    x ^= x << 13, x ^= x >> 7, x ^= x << 17;
    uint32_t d = (uint32_t)(x >> 17) | 1u;
    uint32_t r;
    CHECK(fmt_udiv64(x, d, &r) == x / d && r == x % d);
  }
}

static void test_ring(void) {
  struct ring r = {0};
  uint8_t b;
  CHECK(ring_pop(&r, &b) == -1);
  for (unsigned i = 0; i < RING_SIZE; i++) CHECK(ring_push(&r, (uint8_t)i) == 0);
  CHECK(ring_push(&r, 0xff) == -1 && r.dropped == 1);
  CHECK(ring_len(&r) == RING_SIZE);
  for (unsigned i = 0; i < RING_SIZE; i++) CHECK(ring_pop(&r, &b) == 0 && b == (uint8_t)i);
  /* wrap the 32-bit indices */
  r.head = r.tail = UINT32_MAX - 3;
  for (unsigned i = 0; i < 10; i++) CHECK(ring_push(&r, (uint8_t)(i + 1)) == 0);
  for (unsigned i = 0; i < 10; i++) CHECK(ring_pop(&r, &b) == 0 && b == i + 1);
  CHECK(ring_len(&r) == 0);
}

int main(void) {
  test_fmt();
  test_parse();
  test_udiv64();
  test_ring();
  if (fails) {
    fprintf(stderr, "%d failure(s)\n", fails);
    return 1;
  }
  puts("host tests: ok");
  return 0;
}
