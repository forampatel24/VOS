/*
 * kernel/process/process_manager.c
 *
 * JARVIS OS — process lifecycle authority.
 *
 * State machine:
 *
 *   create -> READY (ready queue)
 *   READY  -> SUSPENDED (suspend)
 *   SUSPENDED -> READY (resume)
 *   READY | WAITING | SUSPENDED -> TERMINATED (kill; record in terminated)
 */

#include <stdio.h>
#include <string.h>

#include "process_manager.h"

static jvk_pcb_t* find_pcb(jvk_process_manager_t* pm, int pid)
{
    for (int i = 0; i < JVK_MAX_PROCS; i++) {
        if (pm->pcbs[i].pid == pid) {
            return &pm->pcbs[i];
        }
    }
    return NULL;
}

static int find_free_slot(jvk_process_manager_t* pm)
{
    for (int i = 0; i < JVK_MAX_PROCS; i++) {
        if (pm->pcbs[i].pid == 0) {
            return i;
        }
    }
    return -1;
}

void pm_init(jvk_process_manager_t* pm)
{
    memset(pm, 0, sizeof(*pm));
    pid_gen_init(&pm->pid_gen);
    queue_init(&pm->ready);
    queue_init(&pm->waiting);
    queue_init(&pm->suspended);
    queue_init(&pm->terminated);
}

int pm_create(jvk_process_manager_t* pm, const char* name,
              int priority, unsigned long ticks, int* out_pid)
{
    int slot = find_free_slot(pm);
    if (slot < 0) {
        return 0;
    }
    if (name == NULL) {
        name = "process";
    }

    int pid = pid_gen_next(&pm->pid_gen);
    jvk_pcb_t* p = &pm->pcbs[slot];
    memset(p, 0, sizeof(*p));
    p->pid = pid;
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->state = PROC_STATE_READY;
    p->priority = priority;
    p->created_ticks = ticks;

    queue_push(&pm->ready, pid);
    pm->count++;
    if (out_pid != NULL) {
        *out_pid = pid;
    }
    return 1;
}

int pm_kill(jvk_process_manager_t* pm, int pid)
{
    jvk_pcb_t* p = find_pcb(pm, pid);
    if (p == NULL) {
        return 0;
    }

    queue_remove(&pm->ready, pid);
    queue_remove(&pm->waiting, pid);
    queue_remove(&pm->suspended, pid);

    p->state = PROC_STATE_TERMINATED;
    queue_push(&pm->terminated, pid);
    p->pid = 0; /* free the slot for a future process */
    pm->count--;
    return 1;
}

int pm_suspend(jvk_process_manager_t* pm, int pid)
{
    jvk_pcb_t* p = find_pcb(pm, pid);
    if (p == NULL) {
        return 0;
    }
    if (p->state != PROC_STATE_READY && p->state != PROC_STATE_RUNNING) {
        return 0;
    }

    queue_remove(&pm->ready, pid);
    queue_remove(&pm->waiting, pid);
    p->state = PROC_STATE_SUSPENDED;
    queue_push(&pm->suspended, pid);
    return 1;
}

int pm_resume(jvk_process_manager_t* pm, int pid)
{
    jvk_pcb_t* p = find_pcb(pm, pid);
    if (p == NULL) {
        return 0;
    }
    if (p->state != PROC_STATE_SUSPENDED) {
        return 0;
    }

    queue_remove(&pm->suspended, pid);
    p->state = PROC_STATE_READY;
    queue_push(&pm->ready, pid);
    return 1;
}

jvk_pcb_t* pm_get(jvk_process_manager_t* pm, int pid)
{
    return find_pcb(pm, pid);
}

int pm_next_ready(jvk_process_manager_t* pm)
{
    if (pm->ready.count == 0) {
        return -1;
    }
    int pid = pm->ready.items[0];
    queue_remove(&pm->ready, pid);
    queue_push(&pm->ready, pid); /* move to the back: round-robin */
    pm->switches++;
    return pid;
}

const char* pm_state_name(jvk_proc_state_t state)
{
    switch (state) {
    case PROC_STATE_READY:      return "READY";
    case PROC_STATE_RUNNING:    return "RUNNING";
    case PROC_STATE_WAITING:    return "WAITING";
    case PROC_STATE_SUSPENDED:  return "SUSPENDED";
    case PROC_STATE_TERMINATED: return "TERMINATED";
    default:                    return "UNKNOWN";
    }
}
