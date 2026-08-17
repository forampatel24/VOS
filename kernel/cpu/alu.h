/*
 * kernel/cpu/alu.h
 *
 * JARVIS OS — arithmetic/logic unit.
 *
 * Each operation returns the flags byte for the result and (for add/sub)
 * writes the result through `out`.
 */

#ifndef JARVIS_ALU_H
#define JARVIS_ALU_H

#include <stdint.h>

#include "registers.h"

uint8_t alu_add(uint32_t a, uint32_t b, int32_t* out);
uint8_t alu_sub(uint32_t a, uint32_t b, int32_t* out);
uint8_t alu_cmp(uint32_t a, uint32_t b);

#endif /* JARVIS_ALU_H */