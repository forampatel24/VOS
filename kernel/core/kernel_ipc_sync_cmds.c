/*
 * kernel/core/kernel_ipc_sync_cmds.c
 *
 * JARVIS OS — mutex, semaphore, deadlock and recovery actions (M7).
 */

#include <stdio.h>
#include <string.h>

#include "kernel_ipc_cmd_internal.h"
#include "../ipc/ipc_internal.h"

static int handle_mutex(kipc_ctx_t* ctx, const char* action,
                        const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* name = kipc_pick_name(req, "mutex");
    int pid = kipc_req_int(req, "pid", 0);
    char what[96];
    snprintf(what, sizeof(what), "mutex:%s", name != NULL ? name : "?");
    if (name == NULL) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "missing mutex name");
        return 1;
    }
    if (!kipc_valid_pid(ctx, pid, result)) {
        return 1;
    }
    jvk_ipc_status_t st;
    if (strcmp(action, "ipc_mutex_create") == 0) {
        st = ipc_mutex_create(ctx->ipc, name, pid, ticks);
    } else if (strcmp(action, "ipc_mutex_lock") == 0) {
        st = ipc_mutex_lock(ctx->ipc, name, pid,
                            kipc_req_bool(req, "nonblock", 0));
    } else if (strcmp(action, "ipc_mutex_unlock") == 0) {
        st = ipc_mutex_unlock(ctx->ipc, name, pid);
    } else if (strcmp(action, "ipc_mutex_destroy") == 0) {
        st = ipc_mutex_destroy(ctx->ipc, name, pid);
    } else {
        return 0;
    }
    if (st == JVK_IPC_OK) {
        cJSON_AddStringToObject(result, "mutex", name);
    }
    return kipc_finish(ctx, result, st, pid, what);
}

static int handle_sem(kipc_ctx_t* ctx, const char* action,
                      const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* name = kipc_pick_name(req, "semaphore");
    int pid = kipc_req_int(req, "pid", 0);
    char what[96];
    snprintf(what, sizeof(what), "sem:%s", name != NULL ? name : "?");
    if (name == NULL) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "missing semaphore name");
        return 1;
    }
    if (!kipc_valid_pid(ctx, pid, result)) {
        return 1;
    }
    jvk_ipc_status_t st;
    if (strcmp(action, "ipc_sem_create") == 0) {
        int value = kipc_req_int(req, "value", 2);
        int max = kipc_req_int(req, "max", value > 4 ? value : 4);
        st = ipc_sem_create(ctx->ipc, name, pid, value, max, ticks);
    } else if (strcmp(action, "ipc_sem_wait") == 0) {
        st = ipc_sem_wait(ctx->ipc, name, pid,
                          kipc_req_bool(req, "nonblock", 0));
    } else if (strcmp(action, "ipc_sem_post") == 0) {
        st = ipc_sem_post(ctx->ipc, name, pid);
    } else if (strcmp(action, "ipc_sem_destroy") == 0) {
        st = ipc_sem_destroy(ctx->ipc, name, pid);
    } else {
        return 0;
    }
    if (st == JVK_IPC_OK) {
        cJSON_AddStringToObject(result, "semaphore", name);
    }
    return kipc_finish(ctx, result, st, pid, what);
}

static void add_cycle_arrays(jvk_ipc_t* ipc, cJSON* result)
{
    jvk_ipc_edge_t edges[JVK_IPC_MAX_EDGES];
    int edge_count = ipc_wait_edges(ipc, edges, JVK_IPC_MAX_EDGES);
    jvk_ipc_cycle_t cycles[JVK_IPC_MAX_CYCLES];
    int cycle_count = ipc_detect_deadlock(ipc, cycles, JVK_IPC_MAX_CYCLES);

    cJSON_AddBoolToObject(result, "deadlock", cycle_count > 0);
    cJSON_AddBoolToObject(result, "active", ipc->deadlock_active != 0);
    cJSON_AddNumberToObject(result, "cycle_count", cycle_count);
    cJSON* cycle_arr = cJSON_CreateArray();
    for (int c = 0; c < cycle_count; c++) {
        cJSON* one = cJSON_CreateArray();
        for (int i = 0; i < cycles[c].count; i++) {
            cJSON_AddItemToArray(one, cJSON_CreateNumber(cycles[c].pids[i]));
        }
        cJSON_AddItemToArray(cycle_arr, one);
    }
    cJSON_AddItemToObject(result, "cycles", cycle_arr);

    cJSON* edge_arr = cJSON_CreateArray();
    for (int i = 0; i < edge_count; i++) {
        cJSON* o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "pid", edges[i].pid);
        cJSON_AddNumberToObject(o, "target", edges[i].target);
        cJSON_AddStringToObject(o, "kind", edges[i].kind);
        cJSON_AddStringToObject(o, "resource", edges[i].resource);
        cJSON_AddItemToArray(edge_arr, o);
    }
    cJSON_AddItemToObject(result, "edges", edge_arr);
}

static int handle_deadlock(kipc_ctx_t* ctx, cJSON* result)
{
    cJSON_AddBoolToObject(result, "ok", 1);
    add_cycle_arrays(ctx->ipc, result);
    return 1;
}

static int handle_recover(kipc_ctx_t* ctx, const cJSON* req, cJSON* result,
                          unsigned long ticks)
{
    int pid = kipc_req_int(req, "pid", -1);
    if (pid <= 0 || !kipc_valid_pid(ctx, pid, result)) {
        if (pid <= 0) {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "invalid pid");
        }
        return 1;
    }
    if (ctx->em != NULL) {
        char msg[96];
        snprintf(msg, sizeof(msg), "deadlock recovery killed pid=%d", pid);
        em_record(ctx->em, JVK_ERR_IPC, msg, ctx->ic);
    }
    ipc_release_pid(ctx->ipc, pid, ticks);
    ctx->ipc->stats.recoveries++;
    kipc_apply_wakeups(ctx);
    if (ctx->kill != NULL) {
        ctx->kill(ctx->kill_user, pid);
    }
    kipc_check_deadlock(ctx->ipc);
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "recovered", pid);
    cJSON_AddBoolToObject(result, "deadlock", ctx->ipc->deadlock_active != 0);
    return 1;
}

int kipc_handle_sync(kipc_ctx_t* ctx, const char* action,
                     const cJSON* req, cJSON* result, unsigned long ticks)
{
    if (strncmp(action, "ipc_mutex_", 10) == 0) {
        return handle_mutex(ctx, action, req, result, ticks);
    }
    if (strncmp(action, "ipc_sem_", 8) == 0) {
        return handle_sem(ctx, action, req, result, ticks);
    }
    if (strcmp(action, "ipc_deadlock") == 0) {
        return handle_deadlock(ctx, result);
    }
    if (strcmp(action, "ipc_recover") == 0) {
        return handle_recover(ctx, req, result, ticks);
    }
    return 0;
}
