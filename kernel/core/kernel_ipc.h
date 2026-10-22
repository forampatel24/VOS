/*
 * kernel/core/kernel_ipc.h
 *
 * JARVIS OS — kernel-layer glue for the IPC subsystem (M7).
 *
 * Translates ipc_* JSON actions, mirrors blocking into the process
 * manager (WAITING) and scheduler (not runnable), applies wakeups,
 * exposes the "ipc" snapshot object and detects deadlocks each tick.
 *
 * The glue is the only bridge between IPC and the process subsystem;
 * the IPC implementation itself never calls the process manager.
 */

#ifndef JARVIS_KERNEL_IPC_H
#define JARVIS_KERNEL_IPC_H

#include "../deps/cJSON/cJSON.h"
#include "../interrupts/error_manager.h"
#include "../ipc/ipc_manager.h"
#include "../process/process_manager.h"
#include "../scheduler/scheduler.h"

typedef struct {
    jvk_ipc_t*             ipc;
    jvk_process_manager_t* pm;
    jvk_scheduler_t*       sched;
    jvk_error_manager_t*   em;
    jvk_ic_t*              ic;
    /* Full kernel kill (process + memory + files). Supplied by kernel.c
       so deadlock recovery goes through the normal termination path. */
    void (*kill)(void* user, int pid);
    void*                  kill_user;
} kipc_ctx_t;

/* Create the standard system IPC objects and the observer. */
int  kipc_boot(jvk_ipc_t* ipc, const cJSON* config_root);
void kipc_shutdown(jvk_ipc_t* ipc);
void kipc_set_observer(jvk_ipc_t* ipc, jvk_ipc_event_fn fn, void* user);

/* Handle one ipc_* / list_ipc action; returns 1 if consumed. */
int  kipc_handle(kipc_ctx_t* ctx, const char* action,
                 const cJSON* req, cJSON* result, unsigned long ticks);

void kipc_snapshot(const jvk_ipc_t* ipc, cJSON* root);

/* Move every pid woken by the IPC layer back to READY. Returns count. */
int  kipc_apply_wakeups(kipc_ctx_t* ctx);

/* Run the wait-for graph detector (called every tick). */
int  kipc_check_deadlock(jvk_ipc_t* ipc);

#endif /* JARVIS_KERNEL_IPC_H */
