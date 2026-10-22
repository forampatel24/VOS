/*
 * kernel/ipc/ipc_types.c
 *
 * JARVIS OS — IPC status names (stable strings for the JSON ABI).
 */

#include "ipc_types.h"

const char* jvk_ipc_status_name(jvk_ipc_status_t st)
{
    switch (st) {
    case JVK_IPC_OK:        return "ok";
    case JVK_IPC_BLOCKED:   return "blocked";
    case JVK_IPC_ERR_ARG:   return "invalid argument";
    case JVK_IPC_ERR_NOENT: return "no such ipc object";
    case JVK_IPC_ERR_EXIST: return "ipc object already exists";
    case JVK_IPC_ERR_FULL:  return "ipc buffer full";
    case JVK_IPC_ERR_EMPTY: return "ipc buffer empty";
    case JVK_IPC_ERR_AGAIN: return "would block";
    case JVK_IPC_ERR_BUSY:  return "already waiting";
    case JVK_IPC_ERR_PERM:  return "permission denied";
    case JVK_IPC_ERR_DEADLK:return "deadlock detected";
    default:                return "unknown";
    }
}
