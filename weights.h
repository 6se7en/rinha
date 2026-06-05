



#ifndef RINHA2026_WEIGHTS_H
#define RINHA2026_WEIGHTS_H

#include <stddef.h>

#include "types.h"

extern const char _binary_weights_bin_start[];
extern const char _binary_weights_bin_end[];
extern const char _binary_weights_bin_size;

void get_weights(u8** dest, size_t* size);

#endif 
