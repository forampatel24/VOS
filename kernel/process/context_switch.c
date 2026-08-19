/*
 * kernel/process/context_switch.c
 *
 * JARVIS OS — process context save / restore / swap.
 *
 * ctx_swap delegates to the NASM stub; the C wrapper only packs and
 * unpacks the 32-bit register file into the 64-bit swap blob.
 */

#include "context_switch.h"

#include "../asm/context_switch.h"

void ctx_pack(jvk_ctx_t* ctx, const cpu_regs_t* regs)
{
    for (int i = 0; i < 8; i++) {
        ctx->q[i] = (uint64_t)(uint32_t)regs->r[i];
    }
    ctx->q[8]  = regs->pc;
    ctx->q[9]  = regs->sp;
    ctx->q[10] = regs->ir;
    ctx->q[11] = regs->flags;
}

void ctx_unpack(cpu_regs_t* regs, const jvk_ctx_t* ctx)
{
    for (int i = 0; i < 8; i++) {
        regs->r[i] = (int32_t)(uint32_t)ctx->q[i];
    }
    regs->pc    = (uint32_t)ctx->q[8];
    regs->sp    = (uint32_t)ctx->q[9];
    regs->ir    = (uint32_t)ctx->q[10];
    regs->flags = (uint8_t)ctx->q[11];
}

void ctx_swap(jvk_ctx_t* a, jvk_ctx_t* b)
{
    jvk_context_switch(a->q, b->q, JVK_CTX_QWORDS);
}
