



#include "weights.h"

void get_weights(u8** dest, size_t* size) {
    *dest = (u8*)_binary_weights_bin_start;
    *size = (size_t)(_binary_weights_bin_end - _binary_weights_bin_start);
}
