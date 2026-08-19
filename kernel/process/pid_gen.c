/*
 * kernel/process/pid_gen.c
 *
 * JARVIS OS — PID generator.
 */

#include "pid_gen.h"

void pid_gen_init(jvk_pid_gen_t* gen)
{
    gen->next = 1;
}

int pid_gen_next(jvk_pid_gen_t* gen)
{
    int pid = gen->next;
    if (pid == 0) {
        pid = 1; /* wrap past INT_MAX is not handled; keep ids positive */
    }
    gen->next++;
    return pid;
}
