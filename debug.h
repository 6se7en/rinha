



#ifndef RINHA2026_DEBUG_H
#define RINHA2026_DEBUG_H

#ifdef DEBUG_MODE
    #include <stdio.h>
    #define DEBUGF(fmt, ...) do {\
        printf(fmt, __VA_ARGS__); \
        fflush(stdout); \
        fflush(stderr); \
    } while (0)

    #define DEBUG(expr) do { \
        expr;
    } while (0)

    #define BREAKPOINT() __asm__("int $3")
#else
    #define DEBUGF(fmt, ...) do { } while (0)

    #define BREAKPOINT() do { } while (0)

    #define DEBUG(expr) do { } while (0)
#endif

#endif 
