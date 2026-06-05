



#ifndef RINHA2026_MMAP_H
#define RINHA2026_MMAP_H
#include "errors.h"
#include "types.h"

Result mopen(const char* path, u8** dest, size_t* dest_size);

void mclose(u8* dest, size_t size);

#endif 
