



#ifndef RINHA2026_LOGS_H
#define RINHA2026_LOGS_H

#include <stdio.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_PERF
#endif

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_PERF = 2,
    LOG_LEVEL_WARNING = 3,
    LOG_LEVEL_ERROR = 4,
} LogLevel;

#define LOG(level, fmt, ...) do { \
    if (level >= LOG_LEVEL) { \
        fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        fflush(stderr); \
    } \
} while (0)

#endif 
