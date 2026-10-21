/*
 * kernel/filesystem/vfs_internal.h
 *
 * JARVIS OS — helpers shared by vfs.c and vfs_ops.c (not public ABI).
 */

#ifndef JARVIS_VFS_INTERNAL_H
#define JARVIS_VFS_INTERNAL_H

#include "vfs.h"

#define JVK_FS_MAX_DEPTH 32
#define JVK_FS_MAX_FILE_BYTES (JVK_FS_MAX_BLOCKS * JVK_DISK_DEFAULT_BLOCK_SIZE)

jvk_inode_t* vfs_inode_mut(jvk_vfs_t* fs, int id);
int          vfs_new_inode(jvk_vfs_t* fs);
int          vfs_child(const jvk_vfs_t* fs, int parent_id, const char* name);
int          vfs_ancestor_of(const jvk_vfs_t* fs, int ancestor, int id);
int          vfs_dir_is_empty(const jvk_vfs_t* fs, int dir_id);
int          vfs_valid_name(const char* name);
jvk_fs_status_t vfs_split(const char* path, char* parent_path, int parent_cap,
                          char* name, int name_cap);
jvk_fs_status_t vfs_store(jvk_vfs_t* fs, jvk_inode_t* inode, int pid,
                          const char* data, int len, unsigned long ticks);
jvk_fs_status_t vfs_read_content(jvk_vfs_t* fs, int pid, jvk_inode_t* inode,
                                 char** out_buf, int* out_len);
void vfs_event(jvk_vfs_t* fs, const char* fmt, ...);

#endif /* JARVIS_VFS_INTERNAL_H */
