/*
 * kernel/core/kernel_fs_internal.h
 *
 * JARVIS OS — JSON helpers shared by the fs glue and its command handlers.
 */

#ifndef JARVIS_KERNEL_FS_INTERNAL_H
#define JARVIS_KERNEL_FS_INTERNAL_H

#include "kernel_fs.h"

void kfs_add_entry_json(const jvk_vfs_t* fs, const jvk_inode_t* ino,
                        cJSON* obj);
void kfs_add_disk_json(const jvk_vfs_t* fs, cJSON* obj);
void kfs_add_stats_json(const jvk_vfs_t* fs, cJSON* obj);

#endif /* JARVIS_KERNEL_FS_INTERNAL_H */
