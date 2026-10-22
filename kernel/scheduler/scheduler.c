#include <stddef.h>
/*
 * kernel/scheduler/scheduler.c
 *
 * JARVIS OS — round-robin scheduler stub.
 *
 * At init the stub exercises the NASM context-switch routine on two
 * dummy buffers so boot-time logs can prove the assembly seam works.
 */

#include <stdio.h>
#include <string.h>

#include "asm/context_switch.h"
#include "scheduler.h"

void scheduler_init(jvk_scheduler_t* sched)
{
    memset(sched, 0, sizeof(*sched));
    sched->algo = SCHED_RR;

    uint64_t ctx_a[4] = {1, 2, 3, 4};
    uint64_t ctx_b[4] = {100, 200, 300, 400};

    jvk_context_switch(ctx_a, ctx_b, 4);

    sched->ctx_verified =
        ctx_a[0] == 100 && ctx_a[3] == 400 &&
        ctx_b[0] == 1   && ctx_b[3] == 4;
}

int scheduler_register(jvk_scheduler_t* sched, int pid, const char* name)
{
    return scheduler_register_ex(sched, pid, name, 0, 0, 0);
}

int scheduler_register_ex(jvk_scheduler_t* sched, int pid, const char* name,
                          int priority, int burst, unsigned long arrival)
{
    if (sched->count >= JVK_MAX_PROCS || pid < 0) {
        return 0;
    }
    sched->pids[sched->count]      = pid;
    sched->ready[sched->count]     = 1;
    sched->priority[sched->count]  = priority;
    sched->burst[sched->count]     = burst;
    sched->arrival[sched->count]   = arrival;
    sched->wait_ticks[sched->count] = 0;
    sched->run_ticks[sched->count]  = 0;
    snprintf(sched->names[sched->count], 32, "%s", name);
    sched->count++;
    return 1;
}

int scheduler_set_algo(jvk_scheduler_t* sched, const char* name)
{
    if (sched == NULL || name == NULL) {
        return 0;
    }
    if (strcmp(name, "rr") == 0 || strcmp(name, "round_robin") == 0) {
        sched->algo = SCHED_RR;
        return 1;
    }
    if (strcmp(name, "fcfs") == 0) {
        sched->algo = SCHED_FCFS;
        return 1;
    }
    if (strcmp(name, "sjf") == 0) {
        sched->algo = SCHED_SJF;
        return 1;
    }
    if (strcmp(name, "priority") == 0) {
        sched->algo = SCHED_PRIORITY;
        return 1;
    }
    return 0;
}

const char* scheduler_algo_name(jvk_sched_algo_t algo)
{
    switch (algo) {
    case SCHED_RR:       return "round_robin";
    case SCHED_FCFS:     return "fcfs";
    case SCHED_SJF:      return "sjf";
    case SCHED_PRIORITY: return "priority";
    default:             return "unknown";
    }
}

static int scheduler_find(const jvk_scheduler_t* sched, int pid)
{
    for (int i = 0; i < sched->count; i++) {
        if (sched->pids[i] == pid) {
            return i;
        }
    }
    return -1;
}

int scheduler_unregister(jvk_scheduler_t* sched, int pid)
{
    int idx = scheduler_find(sched, pid);
    if (idx < 0) {
        return 0;
    }
    /* Final accounting: the finished process contributes one wait sample
       and one turnaround sample (completion tick - arrival tick). */
    sched->completed++;
    sched->wait_total += sched->wait_ticks[idx];
    sched->turnaround_total +=
        sched->ticks > sched->arrival[idx] ? sched->ticks - sched->arrival[idx] : 0;

    for (int i = idx; i < sched->count - 1; i++) {
        sched->pids[i]     = sched->pids[i + 1];
        sched->ready[i]    = sched->ready[i + 1];
        sched->priority[i] = sched->priority[i + 1];
        sched->burst[i]    = sched->burst[i + 1];
        sched->arrival[i]  = sched->arrival[i + 1];
        sched->wait_ticks[i] = sched->wait_ticks[i + 1];
        sched->run_ticks[i]  = sched->run_ticks[i + 1];
        memcpy(sched->names[i], sched->names[i + 1], 32);
    }
    sched->count--;
    if (sched->next >= sched->count && sched->count > 0) {
        sched->next %= sched->count;
    }
    return 1;
}

int scheduler_set_ready(jvk_scheduler_t* sched, int pid, int ready)
{
    int idx = scheduler_find(sched, pid);
    if (idx < 0) {
        return 0;
    }
    sched->ready[idx] = ready;
    return 1;
}

void scheduler_account(jvk_scheduler_t* sched, int running_pid)
{
    if (sched == NULL) {
        return;
    }
    sched->ticks++;
    int r = scheduler_find(sched, running_pid);
    if (r >= 0 && sched->ready[r]) {
        sched->busy_ticks++;
        sched->run_ticks[r]++;
    } else {
        r = -1;
    }
    /* Every other runnable process spent this tick waiting. */
    for (int i = 0; i < sched->count; i++) {
        if (i != r && sched->ready[i]) {
            sched->wait_ticks[i]++;
        }
    }
}

void scheduler_metrics(const jvk_scheduler_t* sched, double* cpu_utilization,
                       double* avg_wait, double* avg_turnaround)
{
    double util = 0.0, wait = 0.0, turn = 0.0;
    if (sched != NULL) {
        if (sched->ticks > 0) {
            util = (double)sched->busy_ticks / (double)sched->ticks;
        }
        if (sched->completed > 0) {
            wait = (double)sched->wait_total / (double)sched->completed;
            turn = (double)sched->turnaround_total / (double)sched->completed;
        }
    }
    if (cpu_utilization != NULL) *cpu_utilization = util;
    if (avg_wait != NULL)       *avg_wait = wait;
    if (avg_turnaround != NULL) *avg_turnaround = turn;
}

int scheduler_schedule(jvk_scheduler_t* sched)
{
    if (sched->count == 0) {
        return -1;
    }
    if (sched->algo == SCHED_RR) {
        for (int i = 0; i < sched->count; i++) {
            int idx = (sched->next + i) % sched->count;
            if (sched->ready[idx]) {
                sched->next = (idx + 1) % sched->count;
                sched->switches++;
                return sched->pids[idx];
            }
        }
        return -1;
    }
    /* For FCFS / SJF / Priority, pick the best ready process without
       rotating the cursor — selection is deterministic on the stored
       attributes. */
    int best = -1;
    for (int i = 0; i < sched->count; i++) {
        if (!sched->ready[i]) {
            continue;
        }
        if (best < 0) {
            best = i;
            continue;
        }
        if (sched->algo == SCHED_FCFS) {
            if (sched->arrival[i] < sched->arrival[best]) {
                best = i;
            }
        } else if (sched->algo == SCHED_SJF) {
            int bi = sched->burst[i] ? sched->burst[i] : 999999;
            int bb = sched->burst[best] ? sched->burst[best] : 999999;
            if (bi < bb || (bi == bb && sched->arrival[i] < sched->arrival[best])) {
                best = i;
            }
        } else if (sched->algo == SCHED_PRIORITY) {
            if (sched->priority[i] > sched->priority[best] ||
                (sched->priority[i] == sched->priority[best] && sched->arrival[i] < sched->arrival[best])) {
                best = i;
            }
        }
    }
    if (best >= 0) {
        sched->switches++;
        return sched->pids[best];
    }
    return -1;
}