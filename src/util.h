#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <gb/bgb_emu.h>

#ifdef BGB_DEBUG

// Prints to GB screen and BGB emulator log, halts execution with busy loop.
#define ASSERT(expr, msg)                                     \
    do                                                        \
    {                                                         \
        if (!(expr))                                          \
        {                                                     \
            printf("ASSERT failed: %s (%s)", msg, #expr);     \
            BGB_printf("ASSERT failed: %s (%s)", msg, #expr); \
            while (1)                                         \
            { /* halt */                                      \
            }                                                 \
        }                                                     \
    } while (0)
#else
#define ASSERT(expr, msg) ((void)0)
#endif

#endif // UTIL_H
