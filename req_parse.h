



#ifndef RINHA2026_REQ_PARSE_H
#define RINHA2026_REQ_PARSE_H
#include <stdint.h>

#include "errors.h"

#define TS_LEN     32
#define MAX_KNOWN  6
#define ID_LEN     16

typedef struct {
    float amount;
    uint32_t installments;
    char* requested_at;
    float cust_avg;
    uint32_t tx_count_24h;
    uint32_t known[MAX_KNOWN];
    uint32_t known_n;
    uint32_t merch_id;
    uint32_t mcc;
    float merch_avg;
    bool is_online, card_present;
    float km_home;
    bool has_last;
    char* last_ts;
    float last_km;
} Request;

Result request_parse(const char* data, size_t size, Request* req);

void request_print(const Request* payload);

#endif 
