/*
 * kernel/process/pid_gen.h
 *
 * JARVIS OS — PID generator.
 *
 * Monotonic, never reuses an ID. The counter lives in the process
 * manager so IDs stay unique for the lifetime of a boot.
 */

#ifndef JARVIS_PID_GEN_H
#define JARVIS_PID_GEN_H

typedef struct {
    int next;
} jvk_pid_gen_t;

void pid_gen_init(jvk_pid_gen_t* gen);
int  pid_gen_next(jvk_pid_gen_t* gen);

#endif /* JARVIS_PID_GEN_H */
