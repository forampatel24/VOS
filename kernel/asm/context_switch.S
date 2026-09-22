; kernel/asm/context_switch.S
;
; JARVIS OS - context-switch stub (NASM, win64).
;
;   void jvk_context_switch(uint64_t* a, uint64_t* b, uint64_t n);
;
; Swaps n qwords between the two buffers, proving the assembly seam the
; M3 scheduler will use for full register save/restore.
; Windows x64: rcx=a, rdx=b, r8=n  |  System V AMD64 (Linux): rdi=a, rsi=b, rdx=n
; Volatile registers only are clobbered, so no prologue/epilogue needed.

BITS 64

section .text
global jvk_context_switch

jvk_context_switch:
%ifdef __linux__
    ; System V AMD64: rdi=a, rsi=b, rdx=n
    test rdx, rdx
    jz   .done
.loop:
    mov  rax, [rdi]
    mov  r8,  [rsi]
    mov  [rsi], rax
    mov  [rdi], r8
    add  rdi, 8
    add  rsi, 8
    dec  rdx
    jnz  .loop
%else
    ; Windows x64: rcx=a, rdx=b, r8=n
    test r8, r8
    jz   .done
.loop:
    mov  rax, [rcx]
    mov  r9,  [rdx]
    mov  [rdx], rax
    mov  [rcx], r9
    add  rcx, 8
    add  rdx, 8
    dec  r8
    jnz  .loop
%endif
.done:
    ret