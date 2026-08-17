/*
 * kernel/cpu/registers.h
 *
 * JARVIS OS — CPU register file.
 *
 * 8 general-purpose 32-bit registers (R0..R7), a program counter, a
 * stack pointer, an instruction register (current opcode) and a flags
 * byte. The full set is exposed to the bridge via jvk_snapshot so the
 * UI can render a live register panel.
 */

#ifndef JARVIS_REGISTERS_H
#define JARVIS_REGISTERS_H

#include <stdint.h>

typedef struct {
    int32_t  r[8];     /* general purpose: R0..R7 */
    uint32_t pc;       /* program counter */
    uint32_t sp;       /* stack pointer */
    uint32_t ir;       /* instruction register (last decoded opcode) */
    uint8_t  flags;    /* bit0 Z, bit1 N, bit2 C */
    int      halted;   /* HALT executed (or ran off the end of memory) */
} cpu_regs_t;

#define FLAG_Z 0x01
#define FLAG_N 0x02
#define FLAG_C 0x04

#endif /* JARVIS_REGISTERS_H */