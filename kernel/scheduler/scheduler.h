/*
 * kernel/scheduler/scheduler.h
 *
 * JARVIS OS — process scheduler (M2 round-robin stub).
 *
 * M2 brings the round-robin machinery and the context-switch seam, but
 * process creation lands in M3, so the ready queue starts empty. The
 * kernel calls scheduler_schedule() every tick; once processes exist it
 * rotates through them and bumps the switch counter.
 */

#ifndef JARVIS_SCHEDULER_H
#define JARVIS_SCHEDULER_H

#include <stdint.h>

#define JVK_MAX_PROCS 8

typedef struct {
    int      count;          /* registered processes */
    int      next;           /* round-robin cursor */
    uint64_t switches;       /* context switches performed */
    int      pids[JVK_MAX_PROCS];
    int      ready[JVK_MAX_PROCS];  /* 1 = runnable */
    char     names[JVK_MAX_PROCS][32];
    int      ctx_verified;   /* asm stub exercised at init */
} jvk_scheduler_t;

void scheduler_init(jvk_scheduler_t* sched);
int  scheduler_register(jvk_scheduler_t* sched, int pid, const char* name);
int  scheduler_unregister(jvk_scheduler_t* sched, int pid);
int  scheduler_set_ready(jvk_scheduler_t* sched, int pid, int ready);
int  scheduler_schedule(jvk_scheduler_t* sched); /* selected pid, or -1 */

#endif /* JARVIS_SCHEDULER_H */