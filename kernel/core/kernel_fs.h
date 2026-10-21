/*
 * kernel/core/kernel_fs.h
 *
 * JARVIS OS — kernel-layer glue for the virtual filesystem.
 *
 * Boot config keys (all optional, section "filesystem"):
 *   blocks      number of virtual disk blocks   (default 1024)
 *   blockSize   bytes per block                 (default 4096)
 *
 * All fs_* actions and the "filesystem" snapshot object live here so
 * core/kernel.c stays a thin dispatcher. The subsystem itself never
 * touches JSON.
 */

#ifndef JARVIS_KERNEL_FS_H
#define JARVIS_KERNEL_FS_H

#include "../deps/cJSON/cJSON.h"
#include "../filesystem/vfs.h"

int  kfs_boot(jvk_vfs_t* fs, const cJSON* config_root);

/* Create the default tree and seed files. Call after the observer is set. */
void kfs_seed(jvk_vfs_t* fs, unsigned long ticks);

/* Wipe and re-seed the filesystem with the current disk geometry. */
void kfs_format(jvk_vfs_t* fs, unsigned long ticks);

/* Handle one fs_* action; returns 1 if consumed, 0 if not ours. */
int  kfs_handle(jvk_vfs_t* fs, const char* action,
                const cJSON* req, cJSON* result, unsigned long ticks);

void kfs_snapshot(const jvk_vfs_t* fs, cJSON* root);

/* Close every descriptor owned by a process (called on kill). */
int  kfs_close_owner(jvk_vfs_t* fs, int pid);

#endif /* JARVIS_KERNEL_FS_H */
