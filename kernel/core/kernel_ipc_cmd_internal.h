/*
 * kernel/core/kernel_ipc_cmd_internal.h
 *
 * Shared helpers between the IPC command handlers (M7).
 */

#ifndef JARVIS_KERNEL_IPC_CMD_INTERNAL_H
#define JARVIS_KERNEL_IPC_CMD_INTERNAL_H

#include "kernel_ipc.h"

int         kipc_req_int(const cJSON* req, const char* key, int def);
const char* kipc_req_str(const cJSON* req, const char* key, const char* def);
int         kipc_req_bool(const cJSON* req, const char* key, int def);
const char* kipc_pick_name(const cJSON* req, const char* primary);
int         kipc_valid_pid(kipc_ctx_t* ctx, int pid, cJSON* result);

/* Apply wakeups, map BLOCKED to pm_block + scheduler, add ok/error. */
int         kipc_finish(kipc_ctx_t* ctx, cJSON* result, jvk_ipc_status_t st,
                        int pid, const char* what);

/* Mutex / semaphore / deadlock / recovery actions (sync cmds file). */
int         kipc_handle_sync(kipc_ctx_t* ctx, const char* action,
                             const cJSON* req, cJSON* result,
                             unsigned long ticks);

#endif /* JARVIS_KERNEL_IPC_CMD_INTERNAL_H */
