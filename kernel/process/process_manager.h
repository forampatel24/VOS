/*
 * kernel/process/process_manager.h
 *
 * JARVIS OS — process lifecycle authority.
 *
 * Owns the PCB table, the PID generator and the ready / waiting /
 * suspended / terminated queues. Only the process manager may move a
 * process between states (M3a): create, kill, suspend, resume. The
 * kernel coordinates the scheduler through the returned pid.
 *
 * A killed process leaves a record in the terminated queue (its pid
 * stays unique forever); its PCB slot is freed for reuse.
 */

#ifndef JARVIS_PROCESS_MANAGER_H
#define JARVIS_PROCESS_MANAGER_H

#include <stdint.h>

#include "context_switch.h"
#include "pcb.h"
#include "pid_gen.h"
#include "queues.h"

#define JVK_MAX_PROCS 8

typedef struct {
    jvk_pcb_t     pcbs[JVK_MAX_PROCS];
    int           count;        /* live (non-terminated) processes */
    jvk_queue_t   ready;
    jvk_queue_t   waiting;
    jvk_queue_t   suspended;
    jvk_queue_t   terminated;
    jvk_pid_gen_t pid_gen;
    uint64_t      switches;     /* context switches performed */
} jvk_process_manager_t;

void        pm_init(jvk_process_manager_t* pm);
int         pm_create(jvk_process_manager_t* pm, const char* name,
                      int priority, unsigned long ticks, int* out_pid);
int         pm_kill(jvk_process_manager_t* pm, int pid);
int         pm_suspend(jvk_process_manager_t* pm, int pid);
int         pm_resume(jvk_process_manager_t* pm, int pid);
jvk_pcb_t*  pm_get(jvk_process_manager_t* pm, int pid);
int         pm_next_ready(jvk_process_manager_t* pm); /* rotate ready, or -1 */
const char* pm_state_name(jvk_proc_state_t state);

#endif /* JARVIS_PROCESS_MANAGER_H */
