#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct PoolFreeNode {
  struct PoolFreeNode *next;
} PoolFreeNode;

typedef struct Pool {
  uint8_t *buf;
  size_t buf_len;
  size_t chunk_size;

  PoolFreeNode *head;
} Pool;

bool is_power_of_two(uintptr_t x) { return (x & (x - 1)) == 0; }

intptr_t align_forward(uintptr_t ptr, uintptr_t align) {
  assert(is_power_of_two(align));

  uintptr_t p = ptr;
  uintptr_t a = (uintptr_t)align;

  uintptr_t mod = p & (a - 1);

  if (mod != 0) { // Unaligned address
    p += a - mod;
  }

  return p;
}

void pool_free_all(Pool *p);

void pool_init_align(Pool *p, void *backing_buffer, size_t buf_len,
                     size_t chunk_size, size_t chunk_alignment) {
  uintptr_t inital_start = (uintptr_t)backing_buffer;
  uintptr_t start = align_forward(inital_start, chunk_alignment);
  buf_len -= (size_t)(start - inital_start);

  assert(chunk_size >= sizeof(PoolFreeNode) && "Chunk size too small");
  assert(buf_len >= chunk_size && "Buffer size is smaller than chunk size");

  p->buf = (uint8_t *)backing_buffer;
  p->buf_len = buf_len;
  p->chunk_size = chunk_size;
  p->head = NULL;

  pool_free_all(p);
}

void *pool_alloc(Pool *p) {
  PoolFreeNode *node = p->head;
  if (!node) {
    return NULL; // No memory
  }

  p->head = p->head->next;

  return memset(node, 0, p->chunk_size);
}

void pool_free(Pool *p, void *ptr) {
    PoolFreeNode *old_head = p->head;

    void *start = p->buf;
    void *end =  &p->buf[p->buf_len];

    if (!ptr) {
        return;
    }

    if (!(start <= ptr && ptr < end)) {
        // Pointer is out of bounds of the pool
        return;
    }

    PoolFreeNode *node = (PoolFreeNode *)ptr;
    node->next = p->head;
    p->head = node;
}

void pool_free_all(Pool *p) {
    size_t chunk_count = p->buf_len / p->chunk_size;

    for (size_t i = 0; i < chunk_count; i++) {
        void *ptr = &p->buf[i * p->chunk_size];
        PoolFreeNode *node = (PoolFreeNode *)ptr;

        node->next = p->head;
        p->head = node;
    }
}
