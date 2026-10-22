/*
 * kernel/core/kernel_ipc_cmds.c
 *
 * JARVIS OS — IPC command handlers: queues, shared memory, pipes (M7).
 * Mutex / semaphore / deadlock actions live in kernel_ipc_sync_cmds.c.
 *
 * Every handler validates the actor pid, calls one IPC operation and
 * funnels the resulting status through kipc_finish(), which mirrors a
 * blocking result into the process manager and applies wakeups.
 */

#include <stdio.h>
#include <string.h>

#include "kernel_ipc_cmd_internal.h"
#include "../ipc/ipc_internal.h"

int kipc_req_int(const cJSON* req, const char* key, int def)
{
    if (req == NULL) {
        return def;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, key);
    return cJSON_IsNumber(item) ? (int)item->valuedouble : def;
}

const char* kipc_req_str(const cJSON* req, const char* key, const char* def)
{
    if (req == NULL) {
        return def;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, key);
    return cJSON_IsString(item) ? item->valuestring : def;
}

int kipc_req_bool(const cJSON* req, const char* key, int def)
{
    if (req == NULL) {
        return def;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, key);
    return cJSON_IsBool(item) ? (cJSON_IsTrue(item) ? 1 : 0) : def;
}

const char* kipc_pick_name(const cJSON* req, const char* primary)
{
    const char* name = kipc_req_str(req, primary, NULL);
    if (name == NULL) {
        name = kipc_req_str(req, "name", NULL);
    }
    return name;
}

int kipc_valid_pid(kipc_ctx_t* ctx, int pid, cJSON* result)
{
    if (pid < 0) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "invalid pid");
        return 0;
    }
    if (pid > 0 && ctx->pm != NULL && pm_get(ctx->pm, pid) == NULL) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "no such process");
        cJSON_AddNumberToObject(result, "pid", pid);
        return 0;
    }
    return 1;
}

int kipc_finish(kipc_ctx_t* ctx, cJSON* result, jvk_ipc_status_t st,
                int pid, const char* what)
{
    kipc_apply_wakeups(ctx);
    if (st == JVK_IPC_OK) {
        cJSON_AddBoolToObject(result, "ok", 1);
        return 1;
    }
    if (st == JVK_IPC_BLOCKED) {
        if (pid <= 0 || ctx->pm == NULL || !pm_block(ctx->pm, pid)) {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "would block");
            if (ctx->em != NULL) {
                char msg[128];
                snprintf(msg, sizeof(msg), "ipc would block pid=%d on %s",
                         pid, what != NULL ? what : "?");
                em_record(ctx->em, JVK_ERR_IPC, msg, ctx->ic);
            }
            return 1;
        }
        scheduler_set_ready(ctx->sched, pid, 0);
        ipc_emit(ctx->ipc, "IPC_STATE pid=%d state=WAITING resource=%s",
                 pid, what != NULL ? what : "?");
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddBoolToObject(result, "blocked", 1);
        cJSON_AddStringToObject(result, "state", "WAITING");
        cJSON_AddStringToObject(result, "waiting_on", what != NULL ? what : "?");
        return 1;
    }
    const char* name = jvk_ipc_status_name(st);
    cJSON_AddBoolToObject(result, "ok", 0);
    cJSON_AddStringToObject(result, "error", name);
    if (ctx->em != NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "ipc %s pid=%d on %s", name, pid,
                 what != NULL ? what : "?");
        em_record(ctx->em, JVK_ERR_IPC, msg, ctx->ic);
    }
    return 1;
}

static void add_bytes(cJSON* result, const char* data, int bytes)
{
    cJSON_AddStringToObject(result, "data", data != NULL ? data : "");
    cJSON_AddNumberToObject(result, "bytes", bytes);
}

static int handle_queue(kipc_ctx_t* ctx, const char* action,
                        const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* name = kipc_pick_name(req, "queue");
    int pid = kipc_req_int(req, "pid", 0);
    char what[96];
    snprintf(what, sizeof(what), "queue:%s", name != NULL ? name : "?");
    if (name == NULL) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "missing queue name");
        return 1;
    }
    if (!kipc_valid_pid(ctx, pid, result)) {
        return 1;
    }
    jvk_ipc_status_t st;
    if (strcmp(action, "ipc_queue_create") == 0) {
        int cap = kipc_req_int(req, "capacity", ctx->ipc->cfg_queue_capacity);
        st = ipc_queue_create(ctx->ipc, name, pid, cap, ticks);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "queue", name);
        }
    } else if (strcmp(action, "ipc_queue_send") == 0) {
        const char* data = kipc_req_str(req, "data", "");
        st = ipc_queue_send(ctx->ipc, name, pid, data,
                            kipc_req_bool(req, "nonblock", 0));
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "queue", name);
            add_bytes(result, data, (int)strlen(data));
        }
    } else if (strcmp(action, "ipc_queue_recv") == 0) {
        char buf[JVK_IPC_MSG_LEN + 1];
        int bytes = 0;
        st = ipc_queue_recv(ctx->ipc, name, pid,
                            kipc_req_bool(req, "nonblock", 0),
                            buf, JVK_IPC_MSG_LEN, &bytes);
        if (st == JVK_IPC_OK) {
            buf[bytes < 0 ? 0 : (bytes > JVK_IPC_MSG_LEN ? JVK_IPC_MSG_LEN : bytes)] = '\0';
            cJSON_AddStringToObject(result, "queue", name);
            add_bytes(result, buf, bytes);
        }
    } else if (strcmp(action, "ipc_queue_destroy") == 0) {
        st = ipc_queue_destroy(ctx->ipc, name, pid);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "queue", name);
        }
    } else {
        return 0;
    }
    return kipc_finish(ctx, result, st, pid, what);
}

static int handle_shm(kipc_ctx_t* ctx, const char* action,
                      const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* name = kipc_pick_name(req, "segment");
    int pid = kipc_req_int(req, "pid", 0);
    char what[96];
    snprintf(what, sizeof(what), "shm:%s", name != NULL ? name : "?");
    if (name == NULL) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "missing segment name");
        return 1;
    }
    if (!kipc_valid_pid(ctx, pid, result)) {
        return 1;
    }
    jvk_ipc_status_t st;
    if (strcmp(action, "ipc_shm_create") == 0) {
        int size = kipc_req_int(req, "size", ctx->ipc->cfg_shm_bytes);
        st = ipc_shm_create(ctx->ipc, name, pid, size, ticks);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "segment", name);
        }
    } else if (strcmp(action, "ipc_shm_attach") == 0) {
        st = ipc_shm_attach(ctx->ipc, name, pid);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "segment", name);
        }
    } else if (strcmp(action, "ipc_shm_detach") == 0) {
        st = ipc_shm_detach(ctx->ipc, name, pid);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "segment", name);
        }
    } else if (strcmp(action, "ipc_shm_write") == 0) {
        const char* data = kipc_req_str(req, "data", "");
        int offset = kipc_req_int(req, "offset", 0);
        int bytes = 0;
        st = ipc_shm_write(ctx->ipc, name, pid, data, offset, &bytes);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "segment", name);
            cJSON_AddNumberToObject(result, "bytes", bytes);
        }
    } else if (strcmp(action, "ipc_shm_read") == 0) {
        char buf[JVK_IPC_SEG_LEN + 1];
        int bytes = 0;
        int offset = kipc_req_int(req, "offset", 0);
        int max = kipc_req_int(req, "max", JVK_IPC_SEG_LEN);
        if (max <= 0 || max > JVK_IPC_SEG_LEN) {
            max = JVK_IPC_SEG_LEN;
        }
        st = ipc_shm_read(ctx->ipc, name, pid, buf, JVK_IPC_SEG_LEN + 1, &bytes);
        if (st == JVK_IPC_OK) {
            if (offset < 0) {
                offset = 0;
            }
            if (offset > bytes) {
                offset = bytes;
            }
            int n = bytes - offset;
            if (n > max) {
                n = max;
            }
            buf[offset + n] = '\0';
            cJSON_AddStringToObject(result, "segment", name);
            add_bytes(result, buf + offset, n);
        }
    } else if (strcmp(action, "ipc_shm_destroy") == 0) {
        st = ipc_shm_destroy(ctx->ipc, name, pid);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "segment", name);
        }
    } else {
        return 0;
    }
    return kipc_finish(ctx, result, st, pid, what);
}

static int handle_pipe(kipc_ctx_t* ctx, const char* action,
                       const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* name = kipc_pick_name(req, "pipe");
    int pid = kipc_req_int(req, "pid", 0);
    char what[96];
    snprintf(what, sizeof(what), "pipe:%s", name != NULL ? name : "?");
    if (name == NULL) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "missing pipe name");
        return 1;
    }
    if (!kipc_valid_pid(ctx, pid, result)) {
        return 1;
    }
    jvk_ipc_status_t st;
    if (strcmp(action, "ipc_pipe_create") == 0) {
        int cap = kipc_req_int(req, "capacity", ctx->ipc->cfg_pipe_capacity);
        st = ipc_pipe_create(ctx->ipc, name, pid, cap, ticks);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "pipe", name);
        }
    } else if (strcmp(action, "ipc_pipe_open") == 0) {
        const char* mode = kipc_req_str(req, "mode", "rw");
        st = ipc_pipe_open(ctx->ipc, name, pid, mode);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "pipe", name);
            cJSON_AddStringToObject(result, "mode", mode);
        }
    } else if (strcmp(action, "ipc_pipe_close") == 0) {
        st = ipc_pipe_close(ctx->ipc, name, pid);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "pipe", name);
        }
    } else if (strcmp(action, "ipc_pipe_write") == 0) {
        const char* data = kipc_req_str(req, "data", "");
        int written = 0;
        st = ipc_pipe_write(ctx->ipc, name, pid, data,
                            kipc_req_bool(req, "nonblock", 0),
                            &written);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "pipe", name);
            cJSON_AddNumberToObject(result, "bytes", written);
            cJSON_AddBoolToObject(result, "partial",
                                  written < (int)strlen(data));
        }
    } else if (strcmp(action, "ipc_pipe_read") == 0) {
        char buf[JVK_IPC_PIPE_LEN + 1];
        int max = kipc_req_int(req, "max", JVK_IPC_PIPE_CHUNK);
        int bytes = 0;
        st = ipc_pipe_read(ctx->ipc, name, pid, max,
                           kipc_req_bool(req, "nonblock", 0),
                           buf, JVK_IPC_PIPE_LEN + 1, &bytes);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "pipe", name);
            add_bytes(result, buf, bytes);
        }
    } else if (strcmp(action, "ipc_pipe_destroy") == 0) {
        st = ipc_pipe_destroy(ctx->ipc, name, pid);
        if (st == JVK_IPC_OK) {
            cJSON_AddStringToObject(result, "pipe", name);
        }
    } else {
        return 0;
    }
    return kipc_finish(ctx, result, st, pid, what);
}

int kipc_handle(kipc_ctx_t* ctx, const char* action,
                const cJSON* req, cJSON* result, unsigned long ticks)
{
    if (ctx == NULL || ctx->ipc == NULL || action == NULL) {
        return 0;
    }
    if (strcmp(action, "list_ipc") == 0) {
        kipc_snapshot(ctx->ipc, result);
        return 1;
    }
    if (strncmp(action, "ipc_queue_", 10) == 0) {
        return handle_queue(ctx, action, req, result, ticks);
    }
    if (strncmp(action, "ipc_shm_", 8) == 0) {
        return handle_shm(ctx, action, req, result, ticks);
    }
    if (strncmp(action, "ipc_pipe_", 9) == 0) {
        return handle_pipe(ctx, action, req, result, ticks);
    }
    return kipc_handle_sync(ctx, action, req, result, ticks);
}
