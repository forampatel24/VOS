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

    uint64_t ctx_a[4] = {1, 2, 3, 4};
    uint64_t ctx_b[4] = {100, 200, 300, 400};

    jvk_context_switch(ctx_a, ctx_b, 4);

    sched->ctx_verified =
        ctx_a[0] == 100 && ctx_a[3] == 400 &&
        ctx_b[0] == 1   && ctx_b[3] == 4;
}

int scheduler_register(jvk_scheduler_t* sched, int pid, const char* name)
{
    if (sched->count >= JVK_MAX_PROCS || pid < 0) {
        return 0;
    }
    sched->pids[sched->count]    = pid;
    sched->ready[sched->count]   = 1;
    snprintf(sched->names[sched->count], 32, "%s", name);
    sched->count++;
    return 1;
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
    for (int i = idx; i < sched->count - 1; i++) {
        sched->pids[i]  = sched->pids[i + 1];
        sched->ready[i] = sched->ready[i + 1];
        memcpy(sched->names[i], sched->names[i + 1], 32);
    }
    sched->count--;
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

int scheduler_schedule(jvk_scheduler_t* sched)
{
    if (sched->count == 0) {
        return -1;
    }
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