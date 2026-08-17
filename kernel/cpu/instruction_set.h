/*
 * kernel/cpu/instruction_set.h
 *
 * JARVIS OS — instruction set for the simulated CPU.
 *
 * Each instruction is a 16-bit word:
 *
 *     bits 15-12  opcode
 *     bits 11-8   register operand D (Rd)
 *     bits 7-4    register operand S (Rs)
 *     bits 3-0    reserved (0)
 *
 * Instructions with an extra operand (immediate or jump address) are
 * followed by a second word in program memory.
 *
 *   MOV  Rd, Rs    0x0DdS   Rd = Rs
 *   MOV  Rd, imm   0x1Dd0   Rd = imm            (second word = imm16)
 *   ADD  Rd, Rs    0x2DdS   Rd += Rs            (sets flags)
 *   SUB  Rd, Rs    0x3DdS   Rd -= Rs            (sets flags)
 *   CMP  Rd, Rs    0x4DdS   flags only          (sets flags)
 *   JMP  addr      0x5000   PC = addr           (second word = addr)
 *   JZ   addr      0x6000   PC = addr if Z      (second word = addr)
 *   JNZ  addr      0x7000   PC = addr if !Z     (second word = addr)
 *   HALT           0x8000   stop execution
 */

#ifndef JARVIS_INSTRUCTION_SET_H
#define JARVIS_INSTRUCTION_SET_H

typedef enum {
    OP_MOV  = 0x0,
    OP_MOVI = 0x1,
    OP_ADD  = 0x2,
    OP_SUB  = 0x3,
    OP_CMP  = 0x4,
    OP_JMP  = 0x5,
    OP_JZ   = 0x6,
    OP_JNZ  = 0x7,
    OP_HALT = 0x8,
} jvk_opcode;

#define OPCODE(word) (((word) >> 12) & 0x0F)
#define REG_D(word)  (((word) >> 8)  & 0x0F)
#define REG_S(word)  (((word) >> 4)  & 0x0F)

#endif /* JARVIS_INSTRUCTION_SET_H */