/*
 * kernel/cpu/cpu.h
 *
 * JARVIS OS — CPU / virtual machine.
 *
 * The CPU owns program memory (up to JVK_PROG_CAP 16-bit words) and the
 * register file. Programs are loaded as JSON arrays of words through the
 * `cpu_load_program` kernel action and executed by the clock loop.
 */

#ifndef JARVIS_CPU_H
#define JARVIS_CPU_H

#include <stdint.h>

#include "registers.h"

#define JVK_PROG_CAP 4096

typedef struct {
    uint16_t    program[JVK_PROG_CAP];
    uint32_t    program_size;   /* in words */
    cpu_regs_t  regs;
} jvk_cpu_t;

void cpu_init(jvk_cpu_t* cpu);
void cpu_reset(jvk_cpu_t* cpu);
int  cpu_load_program(jvk_cpu_t* cpu, const uint16_t* words, uint32_t n);
int  cpu_step(jvk_cpu_t* cpu);              /* execute one instruction */
int  cpu_run(jvk_cpu_t* cpu, int max_steps);/* execute up to max_steps */

#endif /* JARVIS_CPU_H */