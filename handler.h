



#ifndef RINHA2026_HANDLER_H
#define RINHA2026_HANDLER_H
#include "errors.h"

Result handler_init(bool use_ivf);

Result handle_fraud_check(const char* body, size_t len, uint32_t* num_fraudes);

#endif 
