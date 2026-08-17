/*
 * kernel/tests/test_cpu.c
 *
 * JARVIS OS — CPU smoke test.
 *
 * Loads a sum-loop program (1+2+...+10) and runs the machine until
 * HALT. Run from the kernel/ directory:
 *
 *     mingw32-make test
 */

#include <stdio.h>
#include <stdint.h>

#include "cpu/cpu.h"

static const uint16_t g_sum_program[] = {
    0x1000, 0x0000, /* MOV R0, 0        */
    0x1100, 0x000A, /* MOV R1, 10       */
    0x2010,         /* loop: ADD R0, R1 */
    0x1200, 0x0001, /* MOV R2, 1        */
    0x3120,         /* SUB R1, R2       */
    0x6000, 12,     /* JZ  halt         */
    0x5000, 4,      /* JMP loop         */
    0x8000,         /* halt: HALT       */
};

static int g_failures = 0;

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

int main(void)
{
    jvk_cpu_t cpu;
    cpu_init(&cpu);

    expect(cpu_load_program(&cpu, g_sum_program,
                            sizeof(g_sum_program) / sizeof(uint16_t)) == 1,
           "program loads");

    int steps = cpu_run(&cpu, 1000);

    expect(cpu.regs.halted == 1, "machine halts");
    expect(cpu.regs.r[0] == 55, "R0 holds 1+2+...+10 == 55");
    expect(cpu.regs.pc == 12, "PC rests on the HALT instruction");
    expect(steps == 52, "executed exactly 52 steps");

    if (g_failures == 0) {
        printf("PASS: cpu smoke test (sum loop, %d steps)\n", steps);
        return 0;
    }
    printf("cpu smoke test FAILED (%d assertion(s))\n", g_failures);
    return 1;
}