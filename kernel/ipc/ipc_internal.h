/*
 * kernel/ipc/ipc_internal.h
 *
 * JARVIS OS — helpers shared by the IPC implementation files.
 */

#ifndef JARVIS_IPC_INTERNAL_H
#define JARVIS_IPC_INTERNAL_H

#include "ipc_manager.h"

void ipc_emit(jvk_ipc_t* ipc, const char* fmt, ...);
void ipc_wake(jvk_ipc_t* ipc, int pid);

jvk_ipc_delivery_t* ipc_delivery_slot(jvk_ipc_t* ipc, int pid, int create);
int  ipc_delivery_push(jvk_ipc_t* ipc, int pid, const char* data, int len);

int  ipc_array_remove(int* arr, int* count, int value);
int  ipc_array_contains(const int* arr, int count, int value);

void ipc_pipe_push(jvk_ipc_pipe_t* pipe, char c);
char ipc_pipe_pop(jvk_ipc_pipe_t* pipe);

void ipc_sem_add_holder(jvk_ipc_sem_t* sem, int pid);
void ipc_sem_remove_holder(jvk_ipc_sem_t* sem, int pid);

#endif /* JARVIS_IPC_INTERNAL_H */
