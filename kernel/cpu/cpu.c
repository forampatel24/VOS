/*
 * kernel/cpu/cpu.c
 *
 * JARVIS OS — CPU fetch/decode/execute loop.
 */

#include <string.h>

#include "alu.h"
#include "cpu.h"
#include "instruction_set.h"

void cpu_init(jvk_cpu_t* cpu)
{
    memset(cpu, 0, sizeof(*cpu));
}

void cpu_reset(jvk_cpu_t* cpu)
{
    memset(&cpu->regs, 0, sizeof(cpu->regs));
}

int cpu_load_program(jvk_cpu_t* cpu, const uint16_t* words, uint32_t n)
{
    if (n > JVK_PROG_CAP) {
        return 0;
    }
    memset(cpu->program, 0, sizeof(cpu->program));
    for (uint32_t i = 0; i < n; i++) {
        cpu->program[i] = words[i];
    }
    cpu->program_size = n;
    cpu_reset(cpu);
    return 1;
}

static int32_t sign_extend16(uint16_t v)
{
    return (int32_t)(int16_t)v;
}

int cpu_step(jvk_cpu_t* cpu)
{
    cpu_regs_t* r = &cpu->regs;

    if (r->halted) {
        return 0;
    }
    if (r->pc >= cpu->program_size) {
        /* Ran off the end of program memory: stop the machine. */
        r->halted = 1;
        return 0;
    }

    uint16_t word = cpu->program[r->pc];
    uint8_t  op   = (uint8_t)OPCODE(word);
    uint8_t  rd   = REG_D(word) & 7;
    uint8_t  rs   = REG_S(word) & 7;
    r->ir = op;
    r->pc++;

    int32_t out;
    uint8_t fl;

    switch (op) {
    case OP_MOV:
        r->r[rd] = r->r[rs];
        return 1;

    case OP_MOVI:
        if (r->pc >= cpu->program_size) {
            r->halted = 1;
            return 0;
        }
        r->r[rd] = sign_extend16(cpu->program[r->pc]);
        r->pc++;
        return 1;

    case OP_ADD:
        fl = alu_add((uint32_t)r->r[rd], (uint32_t)r->r[rs], &out);
        r->r[rd] = out;
        r->flags = fl;
        return 1;

    case OP_SUB:
        fl = alu_sub((uint32_t)r->r[rd], (uint32_t)r->r[rs], &out);
        r->r[rd] = out;
        r->flags = fl;
        return 1;

    case OP_CMP:
        fl = alu_cmp((uint32_t)r->r[rd], (uint32_t)r->r[rs]);
        r->flags = fl;
        return 1;

    case OP_JMP:
    case OP_JZ:
    case OP_JNZ:
        if (r->pc >= cpu->program_size) {
            r->halted = 1;
            return 0;
        }
        {
            uint32_t addr = cpu->program[r->pc];
            int take = (op == OP_JMP) ||
                       (op == OP_JZ  && (r->flags & FLAG_Z)) ||
                       (op == OP_JNZ && !(r->flags & FLAG_Z));
            r->pc++;
            if (take) {
                r->pc = addr;
            }
        }
        return 1;

    case OP_HALT:
        r->halted = 1;
        r->pc--; /* leave PC on the HALT instruction */
        return 1;

    default:
        return 1; /* NOP for reserved opcodes */
    }
}

int cpu_run(jvk_cpu_t* cpu, int max_steps)
{
    int n = 0;
    while (n < max_steps && !cpu->regs.halted) {
        cpu_step(cpu);
        n++;
    }
    return n;
}