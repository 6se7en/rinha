



#include <string.h>

#include "req_parse.h"

void request_print(const Request* r) {

}

typedef struct {
    const char* data;
    size_t size;
    size_t offset;
} JsonIterator;

static char json_iter_current(const JsonIterator* iter) {
    return iter->data[iter->offset];
}

static Result json_iter_assert(const JsonIterator* iter, const char c) {
    return json_iter_current(iter) == c ? OK : errAssert;
}

static char json_iter_next(JsonIterator * iter) {
    return iter->data[iter->offset++];
}

static Result json_iter_init(JsonIterator* iter, const char * data, const size_t size) {
    iter->data = data;
    iter->size = size;
    iter->offset = 0;
    return OK;
}

static Result json_iter_move_to(JsonIterator* iter, const char target) {
    const char* pos = strchr(iter->data + iter->offset, target);
    if (pos == NULL) {
        return errTargetNotFound;
    }
    iter->offset = pos - iter->data - iter->offset;
    return OK;
}

static Result json_iter_move_to_number(JsonIterator * iter) {
    while (iter->offset < iter->size) {
        const char c = iter->data[iter->offset];
        if (c >= '0' && c <= '9') {
            return OK;
        }
        iter->offset++;
    }
    return errTargetNotFound;
}

static Result json_iter_read_float(JsonIterator* iter, float* f) {
    char* end;
    *f = strtof(iter->data + iter->offset, &end);
    if (*f == 0 && end == iter->data + iter->offset) {
        return errInvalidFloat;
    }
    iter->offset += (end - iter->data - iter->offset - 1);
    return OK;
}

static Result json_iter_read_uint32(JsonIterator* iter, uint32_t* v) {
    uint32_t n = 0;
    const size_t pre = iter->offset;

    while (iter->offset < iter->size) {
        const char c = iter->data[iter->offset];
        if (c < '0' || c > '9') {
            if (pre == iter->offset) {
                return errInvalidNumber;
            }
            break;
        }
        n = n * 10 + (c - '0');
        iter->offset++;
    }

    *v = n;
    return OK;
}


    
    


static Result json_iter_move_to_str(JsonIterator* iter, const char* needle) {
    const char* dest = strstr(iter->data + iter->offset, needle);
    if (dest == NULL) {
        return errTargetNotFound;
    }
    iter->offset += dest - iter->data - iter->offset + strlen(needle);
    return OK;
}

static void json_iter_skip(JsonIterator* iter, const size_t n) {
    iter->offset += n;
}

#define json_iter_skip_str(it, s) \
    do { \
        json_iter_skip(it, sizeof(s)); \
    } while (0)

static Result json_iter_read_bool_str(JsonIterator* iter, bool* b) {
    switch (iter->data[iter->offset]) {
        case 't':
            *b = true;
            iter->offset += 4; 
            break;
        case 'f':
            *b = false;
            iter->offset += 5; 
            break;
        default:
            return errInvalidBool;
    }
    return OK;
}

static char* json_iter_ptr(const JsonIterator * iter) {
    return (char*)(iter->data + iter->offset);
}

static char json_iter_peek(const JsonIterator* iter, const int n) {
    return iter->data[iter->offset + n];
}

static void json_iter_print(const JsonIterator * iter) {
    printf("offset %lu - %%%s\n", iter->offset, json_iter_ptr(iter));
}

Result request_parse(const char* data, const size_t size, Request* req) {
    req->known_n = 0;

    JsonIterator iter;
    try(json_iter_init(&iter, data, size));
    assert_or_fail(json_iter_next(&iter) == '{', "expected '{' at the beginning of JSON");

    
    try(json_iter_move_to(&iter, ','));

    
    json_iter_skip_str(&iter, ",\"transaction\":{\"amount\":");
    try(json_iter_read_float(&iter, &req->amount));

    
    json_iter_skip_str(&iter, ",\"installments\":");
    try(json_iter_read_uint32(&iter, &req->installments));

    
    json_iter_skip_str(&iter, ",\"requested_at\":");
    
    req->requested_at = json_iter_ptr(&iter);
    json_iter_skip(&iter, 19);

    
    json_iter_skip_str(&iter, "\"},\"customer\":{\"avg_amount\":");
    try(json_iter_read_float(&iter, &req->cust_avg));

    
    json_iter_skip_str(&iter, ",\"tx_count_24h\":");
    try(json_iter_read_uint32(&iter, &req->tx_count_24h));

    json_iter_skip_str(&iter, ",\"known_merchants\":[");
    do {
        try(json_iter_move_to_str(&iter, "MERC-"));
        try(json_iter_read_uint32(&iter, &req->known[req->known_n++]));
    } while (json_iter_peek(&iter, 1) != ']');

    
    try(json_iter_move_to_str(&iter, "MERC-"));
    try(json_iter_read_uint32(&iter, &req->merch_id));

    
    json_iter_skip_str(&iter, "\",\"mcc\":");
    try(json_iter_read_uint32(&iter, &req->mcc));

    
    json_iter_skip_str(&iter, "\",\"avg_amount\"");
    try(json_iter_read_float(&iter, &req->merch_avg));

    
    json_iter_skip_str(&iter, "},\"terminal\":{\"is_online\":");
    try(json_iter_read_bool_str(&iter, &req->is_online));

    
    json_iter_skip_str(&iter, ",\"card_present\"");
    try(json_iter_read_bool_str(&iter, &req->card_present));

    
    json_iter_skip_str(&iter, ",\"km_from_home\"");
    try(json_iter_read_float(&iter, &req->km_home));

    
    json_iter_skip_str(&iter, "},\"last_transaction\":");

    if (json_iter_current(&iter) == 'n') { 
        req->has_last = false;
    } else {
        req->has_last = true;

        
        json_iter_skip_str(&iter, "{\"timestamp\":");
        
        req->last_ts = json_iter_ptr(&iter);
        json_iter_skip(&iter, 19);

        
        json_iter_skip_str(&iter, ",\"km_from_current\":");
        json_iter_skip(&iter, 1);
        try(json_iter_read_float(&iter, &req->last_km));
    }

    return OK;
}
