/*
 * kernel/asm/context_switch.h
 *
 * JARVIS OS — assembly context-switch seam (NASM, win64).
 *
 * M2 ships the raw mechanism: swapping two context buffers. The full
 * register save/restore used by the M3 scheduler builds on this stub.
 */

#ifndef JARVIS_CONTEXT_SWITCH_H
#define JARVIS_CONTEXT_SWITCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Swap n qwords between two buffers. a, b must point to n*8 bytes. */
void jvk_context_switch(uint64_t* a, uint64_t* b, uint64_t n);

#ifdef __cplusplus
}
#endif

#endif /* JARVIS_CONTEXT_SWITCH_H */