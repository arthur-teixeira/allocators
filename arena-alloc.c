#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

typedef struct {
    void *arena;
    size_t offset;
    size_t size;
} Arena;

Arena new_arena(size_t size) {
    Arena a;
    a.size = size;
    a.offset = 0;
    a.arena = malloc(size);
    assert(a.arena);

    return a;
}

void *arena_alloc(Arena *arena, size_t size) {
    if (arena->offset + size > arena->size) {
        return NULL; // Out of memory
    }

    void *p = &arena->arena + arena->offset;
    arena->offset += size;

    return p;
}

void arena_free(Arena *arena) {
    free(arena->arena);
    arena->size = 0;
    arena->arena = NULL;
    arena->offset = 0;
}
