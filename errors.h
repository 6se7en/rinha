



#ifndef RINHA2026_ERRORS_H
#define RINHA2026_ERRORS_H

#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

typedef const char* Result;

#define OK NULL

#if defined(IMPLEMENT_ERROR_STRINGS)
    #define errorConst(LABEL, STRING)    const Result err##LABEL = { STRING };
#else
    #define errorConst(LABEL, STRING)    extern const Result err##LABEL;
#endif

#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define LIKELY(x)   __builtin_expect(!!(x), 1)

#define panic(r) do { \
        if (UNLIKELY(r != OK)) {\
            BREAKPOINT(); \
            fprintf(stderr, "%s:%d: %s: %s - %s\n", __FILE__, __LINE__, __func__, #r, r); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

#define try(exp) do { \
        const Result r = (exp); \
        if (UNLIKELY(r)) { \
            BREAKPOINT(); \
            return r; \
        } \
    } while (0)

#define assert_or_fail(exp, msg) do { \
    if (UNLIKELY(!(exp))) return "assertion failed: " #exp " - " msg; \
} while (0)

#define fail(exp) UNLIKELY((exp) != OK)

#define assign(_type, _name, _fn, ...) \
    _type _name; \
    try(_fn(__VA_ARGS__, &_name))

#define on_fail(exp) if (fail(exp))

errorConst(FailedToOpenFile,    "Error opening file");
errorConst(FailedToMemMap,      "Error mapping file");

errorConst(Assert,              "json iter assert failed")
errorConst(Overflow,            "json iter overflow")
errorConst(TargetNotFound,      "move target not found")
errorConst(InvalidBool,         "invalid bool")
errorConst(InvalidFloat,        "invalid float")
errorConst(InvalidNumber,       "invalid number")

errorConst(MallocFailed,        "malloc failed")
errorConst(ReallocFailed,       "realloc failed")
errorConst(CallocFailed,        "calloc failed")

#endif 
