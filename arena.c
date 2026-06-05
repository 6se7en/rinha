



#include "arena.h"

#include <stdlib.h>

#include "errors.h"
#include "logs.h"

Result arena_init(Arena* arena, const size_t capacity) {
    arena->data = malloc(capacity);
    if (!arena->data) {
        LOG(LOG_LEVEL_ERROR, "Arena allocation failed\n");
        return errMallocFailed;
    }
    arena->used = 0;
    arena->capacity = capacity;
    return OK;
}

Result arena_alloc(Arena* arena, const size_t size, void** dest) {
    if (arena->used + size > arena->capacity) {
        arena->capacity += arena->capacity / 4;
        void* ptr = realloc(arena->data, arena->capacity);
        if (ptr == NULL) {
            LOG(LOG_LEVEL_ERROR, "Arena allocation failed\n");
            return errReallocFailed;
        }
        arena->data = ptr;
        LOG(LOG_LEVEL_PERF, "Arena resized: new capacity = %zu\n", arena->capacity);
    }

    *dest = arena->data + arena->used;
    arena->used += size;
    return OK;
}

void arena_free(Arena* arena) {
    if (arena) {
        free(arena->data);
        free(arena);
    }
}
