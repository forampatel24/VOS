/*
 * kernel/core/kernel_memory.h
 *
 * JARVIS OS — kernel-layer glue for the memory subsystem.
 *
 * Keeps core/kernel.c thin: boot-time config parsing, the mem_* command
 * handlers and the snapshot serialization all live here. The glue layer
 * may read subsystem state (Kernel ↓ Subsystem is an allowed edge); the
 * subsystem itself never touches JSON.
 */

#ifndef JARVIS_KERNEL_MEMORY_H
#define JARVIS_KERNEL_MEMORY_H

#include <stddef.h>

#include "../deps/cJSON/cJSON.h"
#include "../memory/memory_manager.h"

/* Parse the "memory" section of the boot config (defaults apply when
   absent), initialize the manager. Returns 1 on success. */
int  kmem_boot(jvk_memory_manager_t* mm, const cJSON* config_root);

/* Handle one mem_* action; returns 1 if consumed, 0 if not ours. */
int  kmem_handle(jvk_memory_manager_t* mm, const char* action,
                 const cJSON* req, cJSON* result);

/* Add the live "memory" object to a snapshot root. */
void kmem_snapshot(const jvk_memory_manager_t* mm, cJSON* root);

#endif /* JARVIS_KERNEL_MEMORY_H */
