#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  uint8_t *buf;
  size_t offset;
  size_t prev_offset;
  size_t size;
} Arena;

void new_arena(Arena *a, void *backing_buffer, size_t size) {
  a->size = size;
  a->offset = 0;
  a->prev_offset = 0;
  a->buf = (uint8_t *)backing_buffer;
}

bool is_power_of_two(uintptr_t p) { return (p & (p - 1)) == 0; }

intptr_t align_forward(uintptr_t ptr, size_t align) {
  assert(is_power_of_two(align));

  uintptr_t p = ptr;
  uintptr_t a = (uintptr_t)align;

  uintptr_t mod = p & (a - 1);

  if (mod != 0) { // Unaligned address
    p += a - mod;
  }

  return p;
}

void *arena_alloc_aligned(Arena *arena, size_t size, size_t align) {
  uintptr_t curr_ptr = (uintptr_t)arena->buf + (uintptr_t)arena->offset;
  uintptr_t offset = align_forward(curr_ptr, align);
  offset -= (uintptr_t)arena->buf;

  if (offset + size > arena->size) {
    return NULL; // Out of memory
  }

  void *p = &arena->buf[offset];
  arena->prev_offset = offset;
  arena->offset = offset + size;
  memset(p, 0, size);

  return p;
}

void *arena_resize_aligned(Arena *arena, void *old_ptr, size_t old_size,
                           size_t new_size, size_t align) {
  uint8_t *old_mem = (uint8_t *)old_ptr;

  assert(is_power_of_two(align));

  if (!old_mem || old_size == 0) {
    return arena_alloc_aligned(arena, new_size, align);
  }

  if (arena->buf <= old_mem && old_mem < arena->buf + arena->size) {
    if (arena->buf + arena->prev_offset == old_mem) {
      arena->offset = arena->prev_offset + new_size;
      if (new_size > old_size) {
        memset(&arena->buf[arena->offset], 0, new_size - old_size);
      }

      return old_ptr;
    }

    void *new_memory = arena_alloc_aligned(arena, new_size, align);
    size_t copy_size = old_size < new_size ? old_size : new_size;
    memmove(new_memory, old_ptr, copy_size);

    return new_memory;
  }

  return NULL; // Out of bounds
}

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

void *arena_alloc(Arena *arena, size_t size) {
  return arena_alloc_aligned(arena, size, DEFAULT_ALIGNMENT);
}

void *arena_resize(Arena *arena, void *old_ptr, size_t old_size,
                   size_t new_size) {
  return arena_resize_aligned(arena, old_ptr, old_size, new_size,
                              DEFAULT_ALIGNMENT);
}

void arena_free(Arena *arena) {}

void arena_free_all(Arena *arena) {
    arena->offset = 0;
    arena->prev_offset = 0;
}
