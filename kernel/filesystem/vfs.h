/*
 * kernel/filesystem/vfs.h
 *
 * JARVIS OS — Virtual File System authority.
 *
 * The VFS owns the inode tree, the virtual disk and the open file table.
 * Every operation validates its path, checks permissions and updates the
 * disk — there is no alternate route to storage, and no host path is
 * ever touched.
 */

#ifndef JARVIS_VFS_H
#define JARVIS_VFS_H

#include "disk.h"
#include "fs_types.h"
#include "open_table.h"

typedef struct {
    int                used;
    int                id;
    char               name[JVK_FS_NAME_LEN];
    jvk_fs_type_t      type;
    int                parent; /* inode id, 0 for root */
    int                owner;  /* pid; 0 = system */
    unsigned int       mode;   /* UNIX-style 9-bit mask */
    int                size;   /* bytes, files only */
    int                block_count;
    int                blocks[JVK_FS_MAX_BLOCKS];
    unsigned long      created_ticks;
    unsigned long      modified_ticks;
    int                hidden;
    int                readonly;
} jvk_inode_t;

/* Event observer: the kernel subscribes so FS_* events land in the log. */
typedef void (*vfs_event_fn)(void* user, const char* message);

typedef struct {
    jvk_inode_t      inodes[JVK_FS_MAX_INODES];
    jvk_disk_t       disk;
    jvk_open_table_t open;
    int              next_id;
    int              inode_count;
    /* statistics */
    long             creates;
    long             deletes;
    long             mkdirs;
    long             reads;
    long             writes;
    long             renames;
    long             copies;
    long             searches;
    long             denied;
    vfs_event_fn     on_event;
    void*            event_user;
} jvk_vfs_t;

#define JVK_FS_ROOT_ID 1

int  vfs_init(jvk_vfs_t* fs, int blocks, int block_size);
void vfs_shutdown(jvk_vfs_t* fs);
void vfs_set_observer(jvk_vfs_t* fs, vfs_event_fn fn, void* user);

/* Path resolution. Returns 1 and the inode id, or 0 when not found. */
int  vfs_resolve(const jvk_vfs_t* fs, const char* path, int* out_id);
void vfs_build_path(const jvk_vfs_t* fs, int id, char* out, int cap);

/* Directory helpers: return the number of ids written, or -1. */
int  vfs_list(const jvk_vfs_t* fs, const char* path, int* ids, int cap);
int  vfs_search(const jvk_vfs_t* fs, const char* pattern, int* ids, int cap);

const jvk_inode_t* vfs_inode(const jvk_vfs_t* fs, int id);
int  vfs_permitted(const jvk_inode_t* inode, int pid, int want);

/* Mutating operations (all return a status code). */
jvk_fs_status_t vfs_mkdir(jvk_vfs_t* fs, const char* path, int owner,
                          unsigned int mode, unsigned long ticks,
                          int* out_id);
jvk_fs_status_t vfs_create(jvk_vfs_t* fs, const char* path, int owner,
                           unsigned int mode, const char* content, int len,
                           unsigned long ticks, int* out_id);
jvk_fs_status_t vfs_read_path(jvk_vfs_t* fs, const char* path, int pid,
                              char* out, int cap, int* out_len);
jvk_fs_status_t vfs_write_path(jvk_vfs_t* fs, const char* path, int pid,
                               const char* data, int len, int append,
                               unsigned long ticks, int* out_written);
jvk_fs_status_t vfs_delete(jvk_vfs_t* fs, const char* path, int pid);
jvk_fs_status_t vfs_rename(jvk_vfs_t* fs, const char* path,
                           const char* new_name, int pid,
                           unsigned long ticks);
jvk_fs_status_t vfs_move(jvk_vfs_t* fs, const char* src, const char* dst,
                         int pid, unsigned long ticks);
jvk_fs_status_t vfs_copy(jvk_vfs_t* fs, const char* src, const char* dst,
                         int pid, unsigned long ticks, int* out_id);
jvk_fs_status_t vfs_chmod(jvk_vfs_t* fs, const char* path, int pid,
                          int mode, int hidden, int readonly,
                          unsigned long ticks);

/* Open-file table. */
jvk_fs_status_t vfs_open(jvk_vfs_t* fs, const char* path, int pid,
                         const char* mode, unsigned long ticks, int* out_fd);
jvk_fs_status_t vfs_close(jvk_vfs_t* fs, int fd, int pid);
jvk_fs_status_t vfs_read_fd(jvk_vfs_t* fs, int fd, int pid,
                            char* out, int cap, int* out_len);
jvk_fs_status_t vfs_write_fd(jvk_vfs_t* fs, int fd, int pid,
                             const char* data, int len,
                             unsigned long ticks, int* out_written);
int  vfs_close_owner(jvk_vfs_t* fs, int pid);

#endif /* JARVIS_VFS_H */
