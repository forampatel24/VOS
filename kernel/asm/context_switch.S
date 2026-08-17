; kernel/asm/context_switch.S
;
; JARVIS OS - context-switch stub (NASM, win64).
;
;   void jvk_context_switch(uint64_t* a, uint64_t* b, uint64_t n);
;
; Windows x64 calling convention: rcx = a, rdx = b, r8 = n.
; Swaps n qwords between the two buffers, proving the assembly seam the
; M3 scheduler will use for full register save/restore.
;
; Volatile registers only are clobbered, so no prologue/epilogue needed.

BITS 64

section .text
global jvk_context_switch

jvk_context_switch:
    test r8, r8
    jz   .done
.loop:
    mov  rax, [rcx]     ; load  a[i]
    mov  r9,  [rdx]     ; load  b[i]
    mov  [rdx], rax     ; b[i] = a[i]
    mov  [rcx], r9      ; a[i] = b[i]
    add  rcx, 8
    add  rdx, 8
    dec  r8
    jnz  .loop
.done:
    ret