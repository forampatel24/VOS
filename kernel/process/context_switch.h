/*
 * kernel/process/context_switch.h
 *
 * JARVIS OS — process context save / restore / swap.
 *
 * A process context is a flat qword blob (registers + PC + SP + IR +
 * flags). The actual byte swap is performed by the NASM routine
 * jvk_context_switch (kernel/asm/context_switch.S), so every context
 * switch in the simulator runs through the same assembly seam the boot
 * log verifies. For M3 this swaps saved payload blobs, not live
 * hardware registers.
 */

#ifndef JARVIS_PROC_CONTEXT_SWITCH_H
#define JARVIS_PROC_CONTEXT_SWITCH_H

#include <stdint.h>

#include "../cpu/registers.h"

#define JVK_CTX_QWORDS 12 /* R0..R7 + PC + SP + IR + FLAGS */

typedef struct {
    uint64_t q[JVK_CTX_QWORDS];
} jvk_ctx_t;

void ctx_pack(jvk_ctx_t* ctx, const cpu_regs_t* regs);
void ctx_unpack(cpu_regs_t* regs, const jvk_ctx_t* ctx);
void ctx_swap(jvk_ctx_t* a, jvk_ctx_t* b); /* NASM-backed */

#endif /* JARVIS_CONTEXT_SWITCH_H */
