/*
 * kernel/cpu/alu.c
 *
 * JARVIS OS — ALU implementation.
 *
 * Flags follow a simple 32-bit two's-complement convention:
 *   Z set when the result is zero
 *   N set when bit 31 of the result is set (negative)
 *   C set on unsigned carry out (add) or borrow (sub/cmp)
 */

#include "alu.h"

static uint8_t make_flags(uint32_t result, int carry)
{
    uint8_t f = 0;
    if (result == 0) {
        f |= FLAG_Z;
    }
    if (result & 0x80000000u) {
        f |= FLAG_N;
    }
    if (carry) {
        f |= FLAG_C;
    }
    return f;
}

uint8_t alu_add(uint32_t a, uint32_t b, int32_t* out)
{
    uint32_t sum = a + b;
    if (out != NULL) {
        *out = (int32_t)sum;
    }
    return make_flags(sum, sum < a);
}

uint8_t alu_sub(uint32_t a, uint32_t b, int32_t* out)
{
    uint32_t diff = a - b;
    if (out != NULL) {
        *out = (int32_t)diff;
    }
    return make_flags(diff, a < b);
}

uint8_t alu_cmp(uint32_t a, uint32_t b)
{
    return alu_sub(a, b, NULL);
}