#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uint8_t *buf;
  size_t buf_len;
  size_t offset;
  size_t prev_offset;
} Stack;

typedef struct {
  size_t padding;
  size_t prev_offset;
} StackAllocationHeader;

void stack_init(Stack *s, void *backing_buffer, size_t backing_buffer_len) {
  s->buf = backing_buffer;
  s->buf_len = backing_buffer_len;
  s->offset = 0;
}

bool is_power_of_two(uintptr_t ptr) { return (ptr & (ptr - 1)) == 0; }

size_t calc_padding_with_header(uintptr_t ptr, uintptr_t alignment,
                                size_t header_size) {
  assert(is_power_of_two(alignment));

  uintptr_t a = alignment;
  uintptr_t modulo = ptr & (a - 1);

  uintptr_t padding = 0;
  uintptr_t needed_space = (uintptr_t)header_size;

  if (modulo != 0) {
    padding = a - modulo;
  }

  if (padding < needed_space) {
    needed_space -= padding;

    if ((needed_space & (a - 1)) != 0) { // needed_space % a != 0
      padding += a * (1 + (needed_space / a));
    } else {
      padding += a * (needed_space / a);
    }
  }

  return (size_t)padding;
}

void *stack_alloc_align(Stack *s, size_t size, size_t alignment) {
  assert(is_power_of_two(alignment));

  if (alignment > 128) {
    // Since the header stores the padding in a single byte, the maximum padding
    // is 128
    alignment = 128;
  }

  uintptr_t curr_addr = (uintptr_t)s->buf + (uintptr_t)s->offset;
  size_t padding = calc_padding_with_header(curr_addr, alignment,
                                            sizeof(StackAllocationHeader));
  if (s->offset + padding + size > s->buf_len) {
    // Out of memory
    return NULL;
  }

  s->prev_offset = s->offset;
  s->offset += padding;

  uintptr_t next_addr = curr_addr + (uintptr_t)padding;
  StackAllocationHeader *header =
      (StackAllocationHeader *)(next_addr - sizeof(StackAllocationHeader));
  header->padding = (uint8_t)padding;
  header->prev_offset = s->prev_offset;

  s->offset += size;

  return memset((void *)next_addr, 0, size);
}

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

void *stack_alloc(Stack *s, size_t size) {
  return stack_alloc_align(s, size, DEFAULT_ALIGNMENT);
}

void stack_free(Stack *s, void *ptr) {
  if (!ptr) {
    return;
  }

  uintptr_t start = (uintptr_t)s->buf;
  uintptr_t end = start + (uintptr_t)s->buf_len;
  uintptr_t curr_addr = (uintptr_t)ptr;

  if (!(start <= curr_addr && curr_addr < end)) {
    // Out of bounds memory
    return;
  }

  if (curr_addr >= start + (uintptr_t)s->offset) {
    // Double free
    return;
  }

  StackAllocationHeader *header =
      (StackAllocationHeader *)(curr_addr - sizeof(StackAllocationHeader));
  size_t prev_offset = (size_t)(curr_addr - (uintptr_t)header->padding - start);

  if (prev_offset != s->prev_offset) {
    // Out of order stack free
    return;
  }

  s->offset = prev_offset;
  s->prev_offset = header->prev_offset;
}

void *stack_resize_align(Stack *s, void *ptr, size_t old_size, size_t new_size,
                         size_t alignment) {
  if (!ptr) {
    return stack_alloc_align(s, new_size, alignment);
  }

  if (new_size == 0) {
    stack_free(s, ptr);
    return NULL;
  }

  size_t min_size = old_size < new_size ? old_size : new_size;

  uintptr_t start = (uintptr_t)s->buf;
  uintptr_t end = start + (uintptr_t)s->buf_len;
  uintptr_t curr_addr = (uintptr_t)ptr;

  StackAllocationHeader *header =
      (StackAllocationHeader *)(curr_addr - sizeof(StackAllocationHeader));

  size_t prev_offset = (size_t)(curr_addr - (uintptr_t)header->padding - start);

  if (!(start <= curr_addr && curr_addr < end)) {
    // Out of bounds memory
    return NULL;
  }

  if (curr_addr >= start + (uintptr_t)s->offset) {
    return NULL;
  }

  if (old_size == new_size) {
    return ptr;
  }

  if (prev_offset == s->prev_offset) {
    s->offset += new_size - old_size;
    return ptr;
  }

  void *new_ptr = stack_alloc_align(s, new_size, alignment);
  memmove(new_ptr, ptr, min_size);
  return new_ptr;
}

void *stack_resize(Stack *s, void *ptr, size_t old_size, size_t new_size) {
  return stack_resize_align(s, ptr, old_size, new_size, DEFAULT_ALIGNMENT);
}

void stack_free_all(Stack *s) { s->offset = 0; }
