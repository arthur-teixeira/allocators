#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t *buf;
    size_t offset;
    size_t size;
} Arena;

Arena new_arena(size_t size) {
    Arena a;
    a.size = size;
    a.offset = 0;
    a.buf = malloc(size);
    memset(a.buf, 0, size);
    assert(a.buf);

    return a;
}

bool is_power_of_two(uintptr_t p) {
    return (p & (p -1)) == 0;
}

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
    arena->offset = offset + size;
    memset(p, 0, size);

    return p;
}

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

void *arena_alloc(Arena *arena, size_t size) {
    return arena_alloc_aligned(arena, size, DEFAULT_ALIGNMENT);
}

void arena_free(Arena *arena) {
    free(arena->buf);
    arena->size = 0;
    arena->buf = NULL;
    arena->offset = 0;
}
