/* SPDX-License-Identifier: Apache-2.0 */
/* Single-producer (ISR) / single-consumer (main loop) byte ring. */
#ifndef RING_H
#define RING_H

#include <stdint.h>

#define RING_SIZE 64u /* power of two */

struct ring {
  volatile uint32_t head; /* written by producer */
  volatile uint32_t tail; /* written by consumer */
  uint8_t data[RING_SIZE];
  uint32_t dropped;
};

static inline int ring_push(struct ring *r, uint8_t b) {
  uint32_t h = r->head;
  if (h - r->tail == RING_SIZE) {
    r->dropped++;
    return -1;
  }
  r->data[h & (RING_SIZE - 1)] = b;
  __atomic_store_n(&r->head, h + 1, __ATOMIC_RELEASE);
  return 0;
}

static inline int ring_pop(struct ring *r, uint8_t *b) {
  uint32_t t = r->tail;
  if (__atomic_load_n(&r->head, __ATOMIC_ACQUIRE) == t) return -1;
  *b = r->data[t & (RING_SIZE - 1)];
  __atomic_store_n(&r->tail, t + 1, __ATOMIC_RELEASE);
  return 0;
}

static inline uint32_t ring_len(const struct ring *r) { return r->head - r->tail; }

#endif
