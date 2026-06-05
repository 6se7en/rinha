



#ifndef RINHA2026_OBJ_POOL_H
#define RINHA2026_OBJ_POOL_H
#include <stddef.h>

#include "errors.h"

typedef struct {
    size_t obj_size;
    size_t capacity;
    size_t used;
    void* data;

    size_t num_available;
    void** available;
} ObjPool;

Result obj_pool_init(ObjPool* pool, size_t obj_size, size_t capacity);

Result obj_pool_get(ObjPool* pool, void** dest);

void obj_pool_release(ObjPool* pool, void *ptr);

void obj_pool_free(ObjPool *pool);

#endif 
