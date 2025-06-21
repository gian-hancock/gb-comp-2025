#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <gb/bgb_emu.h>
#include <stdint.h>

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

uint8_t boxes_overap_8x8(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

void set_bkg_tiles_2x2(uint8_t x, uint8_t y, uint8_t base_tile);

#endif // UTIL_H
