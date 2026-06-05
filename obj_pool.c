



#include "obj_pool.h"

#include <stdio.h>
#include <stdlib.h>

#include "errors.h"
#include "logs.h"

Result obj_pool_init(ObjPool* pool, const size_t obj_size, const size_t capacity) {
    pool->obj_size = obj_size;
    pool->capacity = capacity;
    pool->used = 0;
    pool->data = malloc(obj_size * capacity);
    if (pool->data == NULL) {
        LOG(LOG_LEVEL_ERROR, "obj pool data - capacity: %lu\n", capacity);
        return errMallocFailed;
    }
    pool->num_available = 0;
    pool->available = malloc(capacity * sizeof(void*));
    if (pool->available == NULL) {
        LOG(LOG_LEVEL_ERROR, "obj pool available - capacity: %lu\n", capacity);
        return errMallocFailed;
    }
    return OK;
}

Result obj_pool_get(ObjPool* pool, void** dest) {
    if (pool->num_available > 0) {
        *dest = pool->available[--pool->num_available];
        return OK;
    }

    if (pool->used == pool->capacity) {
        pool->capacity += pool->capacity / 2;

        void* ptr = realloc(pool->available, pool->capacity * sizeof(void*));
        if (ptr == NULL) {
            LOG(LOG_LEVEL_ERROR, "obj pool realloc available - capacity: %lu\n", pool->capacity);
            return errReallocFailed;
        }
        pool->available = ptr;

        ptr = realloc(pool->data, pool->capacity * pool->obj_size);
        if (ptr == NULL) {
            LOG(LOG_LEVEL_ERROR, "obj pool realloc data - capacity: %lu\n", pool->capacity);
            return errReallocFailed;
        }
        pool->data = ptr;

        LOG(LOG_LEVEL_PERF, "obj pool realloc - new cap %lu - used: %lu\n", pool->capacity, pool->used);
    }

    *dest = pool->data + pool->used * pool->obj_size;
    pool->used++;
    return OK;
}

void obj_pool_release(ObjPool* pool, void* ptr) {
    pool->available[pool->num_available] = ptr;
    pool->num_available++;
}

void obj_pool_free(ObjPool *pool) {
    if (pool) {
        free(pool->data);
        free(pool->available);
        free(pool);
    }
}
