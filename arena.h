



#ifndef RINHA2026_ARENA_H
#define RINHA2026_ARENA_H
#include <stddef.h>

#include "errors.h"

typedef struct {
    void* data;
    size_t used;
    size_t capacity;
} Arena;

Result arena_init(Arena* arena, size_t capacity);

Result arena_alloc(Arena* arena, size_t size, void** dest);

void arena_free(Arena* arena);

#endif 
