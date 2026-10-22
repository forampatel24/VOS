/*
 * kernel/core/kernel_ipc.c
 *
 * JARVIS OS — IPC boot, wake application and deadlock tick hook (M7).
 *
 * Boot config (all optional, section "ipc"):
 *   seed            create the standard system IPC objects (default true)
 *   queueCapacity   default message-queue depth        (default 8)
 *   pipeCapacity    default pipe ring size in bytes    (default 256)
 *   shmBytes        default shared-memory segment size (default 128)
 *   semaphorePermits default counting-semaphore value  (default 2)
 *   semaphoreMax    default counting-semaphore limit   (default 4)
 */

#include <stdio.h>
#include <string.h>

#include "kernel_ipc.h"
#include "../ipc/ipc_internal.h"

static int cfg_int(const cJSON* root, const char* key, int def)
{
    if (!cJSON_IsObject(root)) {
        return def;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsNumber(item) ? (int)item->valuedouble : def;
}

static int cfg_bool(const cJSON* root, const char* key, int def)
{
    if (!cJSON_IsObject(root)) {
        return def;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsBool(item) ? (cJSON_IsTrue(item) ? 1 : 0) : def;
}

int kipc_boot(jvk_ipc_t* ipc, const cJSON* config_root)
{
    if (ipc == NULL) {
        return 0;
    }
    ipc_init(ipc);

    const cJSON* cfg = cJSON_IsObject(config_root)
                           ? cJSON_GetObjectItemCaseSensitive(config_root, "ipc")
                           : NULL;
    ipc->cfg_queue_capacity = cfg_int(cfg, "queueCapacity", 8);
    ipc->cfg_pipe_capacity  = cfg_int(cfg, "pipeCapacity", 256);
    ipc->cfg_shm_bytes      = cfg_int(cfg, "shmBytes", 128);
    if (ipc->cfg_queue_capacity <= 0 || ipc->cfg_queue_capacity > JVK_IPC_QUEUE_CAP) {
        ipc->cfg_queue_capacity = 8;
    }
    if (ipc->cfg_pipe_capacity <= 0 || ipc->cfg_pipe_capacity > JVK_IPC_PIPE_LEN) {
        ipc->cfg_pipe_capacity = 256;
    }
    if (ipc->cfg_shm_bytes <= 0 || ipc->cfg_shm_bytes > JVK_IPC_SEG_LEN) {
        ipc->cfg_shm_bytes = 128;
    }

    if (cfg_bool(cfg, "seed", 1)) {
        int permits = cfg_int(cfg, "semaphorePermits", 2);
        int sem_max = cfg_int(cfg, "semaphoreMax", 4);
        (void)ipc_queue_create(ipc, "agent-bus", 0,
                               ipc->cfg_queue_capacity, 0);
        (void)ipc_shm_create(ipc, "shared-board", 0,
                             ipc->cfg_shm_bytes, 0);
        (void)ipc_pipe_create(ipc, "console-pipe", 0,
                              ipc->cfg_pipe_capacity, 0);
        (void)ipc_mutex_create(ipc, "log-lock", 0, 0);
        (void)ipc_sem_create(ipc, "worker-slots", 0, permits, sem_max, 0);
    }
    return 1;
}

void kipc_shutdown(jvk_ipc_t* ipc)
{
    ipc_shutdown(ipc);
}

void kipc_set_observer(jvk_ipc_t* ipc, jvk_ipc_event_fn fn, void* user)
{
    ipc_set_observer(ipc, fn, user);
}

int kipc_apply_wakeups(kipc_ctx_t* ctx)
{
    if (ctx == NULL || ctx->ipc == NULL) {
        return 0;
    }
    int pids[JVK_IPC_MAX_PROCS];
    int n = ipc_take_wakeups(ctx->ipc, pids, JVK_IPC_MAX_PROCS);
    int applied = 0;
    for (int i = 0; i < n; i++) {
        int pid = pids[i];
        if (pm_get(ctx->pm, pid) == NULL) {
            continue;
        }
        if (pm_wake(ctx->pm, pid)) {
            scheduler_set_ready(ctx->sched, pid, 1);
            applied++;
            ipc_emit(ctx->ipc, "IPC_WAKE pid=%d", pid);
        }
    }
    return applied;
}

int kipc_check_deadlock(jvk_ipc_t* ipc)
{
    if (ipc == NULL) {
        return 0;
    }
    jvk_ipc_cycle_t cycles[JVK_IPC_MAX_CYCLES];
    return ipc_detect_deadlock(ipc, cycles, JVK_IPC_MAX_CYCLES);
}
