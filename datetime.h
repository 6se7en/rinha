



#ifndef RINHA2026_DATETIME_H
#define RINHA2026_DATETIME_H
#include <stdint.h>

typedef struct {
    uint16_t year;
    uint8_t  month, day;
    uint8_t  hour, min, sec;
} DateTime;

void dt_parse(const char* s, DateTime* dt);

uint32_t dt_day_of_week(const DateTime* dt);

uint32_t dt_minutes_since(const DateTime* a, const DateTime* b);

#endif 
