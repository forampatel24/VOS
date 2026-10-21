/*
 * kernel/filesystem/vfs_ops.c
 *
 * JARVIS OS — VFS operations: create, read, write, delete, rename,
 * move, copy, chmod and the open-file descriptor paths.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vfs_internal.h"

/* ---- storage ----------------------------------------------------------- */

jvk_fs_status_t vfs_store(jvk_vfs_t* fs, jvk_inode_t* inode, int pid,
                          const char* data, int len, unsigned long ticks)
{
    (void)pid;
    if (fs == NULL || inode == NULL || inode->type != JVK_FS_FILE) {
        return JVK_FS_ERR_INVAL;
    }
    int bs = fs->disk.block_size;
    if (len < 0) {
        len = 0;
    }
    int needed = (len + bs - 1) / bs;
    if (needed > JVK_FS_MAX_BLOCKS) {
        return JVK_FS_ERR_NOSPC;
    }

    int fresh[JVK_FS_MAX_BLOCKS];
    if (needed > 0 && disk_alloc(&fs->disk, needed, fresh) != 0) {
        return JVK_FS_ERR_NOSPC;
    }

    int old_blocks[JVK_FS_MAX_BLOCKS];
    int old_count = inode->block_count;
    memcpy(old_blocks, inode->blocks, sizeof(old_blocks));

    for (int i = 0; i < needed; i++) {
        inode->blocks[i] = fresh[i];
    }
    for (int i = needed; i < JVK_FS_MAX_BLOCKS; i++) {
        inode->blocks[i] = 0;
    }
    inode->block_count = needed;
    disk_free(&fs->disk, old_blocks, old_count);

    if (needed > 0) {
        disk_write(&fs->disk, inode->blocks, needed, data, len);
    }
    inode->size         = len;
    inode->modified_ticks = ticks;
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_read_content(jvk_vfs_t* fs, int pid, jvk_inode_t* inode,
                                 char** out_buf, int* out_len)
{
    if (fs == NULL || inode == NULL || out_buf == NULL || out_len == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    if (inode->type != JVK_FS_FILE) {
        return JVK_FS_ERR_ISDIR;
    }
    if (!vfs_permitted(inode, pid, JVK_FS_R)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    int size = inode->size;
    if (size < 0) {
        size = 0;
    }
    char* buf = (char*)malloc((size_t)size + 1);
    if (buf == NULL) {
        return JVK_FS_ERR_NOSPC;
    }
    buf[0] = '\0';
    if (size > 0) {
        disk_read(&fs->disk, inode->blocks, inode->block_count,
                  buf, size + 1, size);
    }
    *out_buf = buf;
    *out_len = size;
    return JVK_FS_OK;
}

/* ---- creation ---------------------------------------------------------- */

jvk_fs_status_t vfs_mkdir(jvk_vfs_t* fs, const char* path, int owner,
                          unsigned int mode, unsigned long ticks,
                          int* out_id)
{
    if (fs == NULL || path == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    char parent_path[JVK_FS_PATH_LEN];
    char name[JVK_FS_NAME_LEN];
    jvk_fs_status_t st = vfs_split(path, parent_path, (int)sizeof(parent_path),
                                   name, (int)sizeof(name));
    if (st != JVK_FS_OK) {
        return st;
    }
    int parent_id = 0;
    if (!vfs_resolve(fs, parent_path, &parent_id)) {
        return JVK_FS_ERR_NOENT;
    }
    const jvk_inode_t* parent = vfs_inode(fs, parent_id);
    if (parent == NULL || parent->type != JVK_FS_DIR) {
        return JVK_FS_ERR_NOTDIR;
    }
    if (!vfs_permitted(parent, owner, JVK_FS_W)) {
        fs->denied++;
        vfs_event(fs, "FS_DENIED op=mkdir path=%s", path);
        return JVK_FS_ERR_PERM;
    }
    if (vfs_child(fs, parent_id, name) != 0) {
        return JVK_FS_ERR_EXIST;
    }

    int id = vfs_new_inode(fs);
    if (id < 0) {
        return JVK_FS_ERR_FULL;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, id);
    snprintf(ino->name, sizeof(ino->name), "%s", name);
    ino->type           = JVK_FS_DIR;
    ino->parent         = parent_id;
    ino->owner          = owner;
    ino->mode           = mode != 0 ? mode : 0755u;
    ino->created_ticks  = ticks;
    ino->modified_ticks = ticks;
    fs->mkdirs++;
    vfs_event(fs, "FS_MKDIR path=%s id=%d owner=%d", path, id, owner);
    if (out_id != NULL) {
        *out_id = id;
    }
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_create(jvk_vfs_t* fs, const char* path, int owner,
                           unsigned int mode, const char* content, int len,
                           unsigned long ticks, int* out_id)
{
    if (fs == NULL || path == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    char parent_path[JVK_FS_PATH_LEN];
    char name[JVK_FS_NAME_LEN];
    jvk_fs_status_t st = vfs_split(path, parent_path, (int)sizeof(parent_path),
                                   name, (int)sizeof(name));
    if (st != JVK_FS_OK) {
        return st;
    }
    int parent_id = 0;
    if (!vfs_resolve(fs, parent_path, &parent_id)) {
        return JVK_FS_ERR_NOENT;
    }
    const jvk_inode_t* parent = vfs_inode(fs, parent_id);
    if (parent == NULL || parent->type != JVK_FS_DIR) {
        return JVK_FS_ERR_NOTDIR;
    }
    if (!vfs_permitted(parent, owner, JVK_FS_W)) {
        fs->denied++;
        vfs_event(fs, "FS_DENIED op=create path=%s", path);
        return JVK_FS_ERR_PERM;
    }
    if (vfs_child(fs, parent_id, name) != 0) {
        return JVK_FS_ERR_EXIST;
    }

    int id = vfs_new_inode(fs);
    if (id < 0) {
        return JVK_FS_ERR_FULL;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, id);
    snprintf(ino->name, sizeof(ino->name), "%s", name);
    ino->type           = JVK_FS_FILE;
    ino->parent         = parent_id;
    ino->owner          = owner;
    ino->mode           = mode != 0 ? mode : 0644u;
    ino->created_ticks  = ticks;
    ino->modified_ticks = ticks;

    if (content != NULL && len > 0) {
        st = vfs_store(fs, ino, owner, content, len, ticks);
        if (st != JVK_FS_OK) {
            ino->used = 0;
            fs->inode_count--;
            return st;
        }
    }
    fs->creates++;
    vfs_event(fs, "FS_CREATED path=%s id=%d size=%d owner=%d",
              path, id, ino->size, owner);
    if (out_id != NULL) {
        *out_id = id;
    }
    return JVK_FS_OK;
}

/* ---- read / write ------------------------------------------------------ */

jvk_fs_status_t vfs_read_path(jvk_vfs_t* fs, const char* path, int pid,
                              char* out, int cap, int* out_len)
{
    if (fs == NULL || path == NULL || out == NULL || cap <= 0) {
        return JVK_FS_ERR_INVAL;
    }
    int id = 0;
    if (!vfs_resolve(fs, path, &id)) {
        return JVK_FS_ERR_NOENT;
    }
    char* buf = NULL;
    int len = 0;
    jvk_fs_status_t st = vfs_read_content(fs, pid, vfs_inode_mut(fs, id),
                                          &buf, &len);
    if (st != JVK_FS_OK) {
        return st;
    }
    snprintf(out, (size_t)cap, "%s", buf);
    free(buf);
    fs->reads++;
    if (out_len != NULL) {
        *out_len = len;
    }
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_write_path(jvk_vfs_t* fs, const char* path, int pid,
                               const char* data, int len, int append,
                               unsigned long ticks, int* out_written)
{
    if (fs == NULL || path == NULL || len < 0) {
        return JVK_FS_ERR_INVAL;
    }
    int id = 0;
    if (!vfs_resolve(fs, path, &id)) {
        return JVK_FS_ERR_NOENT;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, id);
    if (ino == NULL) {
        return JVK_FS_ERR_NOENT;
    }
    if (ino->type != JVK_FS_FILE) {
        return JVK_FS_ERR_ISDIR;
    }
    if (!vfs_permitted(ino, pid, JVK_FS_W)) {
        fs->denied++;
        vfs_event(fs, "FS_DENIED op=write path=%s pid=%d", path, pid);
        return ino->readonly ? JVK_FS_ERR_READONLY : JVK_FS_ERR_PERM;
    }

    char* existing = NULL;
    int old_len = 0;
    if (append) {
        jvk_fs_status_t r = vfs_read_content(fs, pid, ino, &existing, &old_len);
        if (r != JVK_FS_OK) {
            return r;
        }
    }

    int total = (append ? old_len : 0) + len;
    char* merged = (char*)malloc((size_t)total + 1);
    if (merged == NULL) {
        free(existing);
        return JVK_FS_ERR_NOSPC;
    }
    if (append && existing != NULL) {
        memcpy(merged, existing, (size_t)old_len);
    }
    if (data != NULL && len > 0) {
        memcpy(merged + (append ? old_len : 0), data, (size_t)len);
    }
    merged[total] = '\0';
    free(existing);

    jvk_fs_status_t st = vfs_store(fs, ino, pid, merged, total, ticks);
    free(merged);
    if (st != JVK_FS_OK) {
        return st;
    }
    fs->writes++;
    vfs_event(fs, "FS_WRITE path=%s bytes=%d size=%d pid=%d",
              path, len, ino->size, pid);
    if (out_written != NULL) {
        *out_written = len;
    }
    return JVK_FS_OK;
}

/* ---- delete / rename / move -------------------------------------------- */

jvk_fs_status_t vfs_delete(jvk_vfs_t* fs, const char* path, int pid)
{
    if (fs == NULL || path == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    int id = 0;
    if (!vfs_resolve(fs, path, &id) || id == JVK_FS_ROOT_ID) {
        return JVK_FS_ERR_NOENT;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, id);
    if (ino == NULL) {
        return JVK_FS_ERR_NOENT;
    }
    jvk_inode_t* parent = vfs_inode_mut(fs, ino->parent);
    if (parent == NULL || !vfs_permitted(parent, pid, JVK_FS_W)) {
        fs->denied++;
        vfs_event(fs, "FS_DENIED op=delete path=%s", path);
        return JVK_FS_ERR_PERM;
    }
    if (ino->type == JVK_FS_DIR && !vfs_dir_is_empty(fs, id)) {
        return JVK_FS_ERR_NOTEMPTY;
    }

    int closed = ot_close_inode(&fs->open, id);
    disk_free(&fs->disk, ino->blocks, ino->block_count);
    memset(ino, 0, sizeof(*ino));
    if (fs->inode_count > 0) {
        fs->inode_count--;
    }
    fs->deletes++;
    vfs_event(fs, "FS_DELETED path=%s fds_closed=%d", path, closed);
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_rename(jvk_vfs_t* fs, const char* path,
                           const char* new_name, int pid,
                           unsigned long ticks)
{
    if (fs == NULL || path == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    if (!vfs_valid_name(new_name)) {
        return JVK_FS_ERR_INVAL;
    }
    int id = 0;
    if (!vfs_resolve(fs, path, &id) || id == JVK_FS_ROOT_ID) {
        return JVK_FS_ERR_NOENT;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, id);
    jvk_inode_t* parent = vfs_inode_mut(fs, ino->parent);
    if (parent == NULL || !vfs_permitted(parent, pid, JVK_FS_W)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    int clash = vfs_child(fs, ino->parent, new_name);
    if (clash != 0 && clash != id) {
        return JVK_FS_ERR_EXIST;
    }
    char old_name[JVK_FS_NAME_LEN];
    snprintf(old_name, sizeof(old_name), "%s", ino->name);
    snprintf(ino->name, sizeof(ino->name), "%s", new_name);
    ino->modified_ticks = ticks;
    fs->renames++;
    vfs_event(fs, "FS_RENAMED path=%s from=%s to=%s", path, old_name, new_name);
    return JVK_FS_OK;
}

static jvk_fs_status_t resolve_destination(jvk_vfs_t* fs, const char* dst,
                                           const char* src_name, int pid,
                                           int* out_parent, char* name,
                                           int name_cap)
{
    int dst_id = 0;
    if (vfs_resolve(fs, dst, &dst_id)) {
        const jvk_inode_t* d = vfs_inode(fs, dst_id);
        if (d == NULL || d->type != JVK_FS_DIR) {
            return JVK_FS_ERR_EXIST;
        }
        if (!vfs_permitted(d, pid, JVK_FS_W)) {
            fs->denied++;
            return JVK_FS_ERR_PERM;
        }
        *out_parent = dst_id;
        snprintf(name, (size_t)name_cap, "%s", src_name);
        return JVK_FS_OK;
    }

    char parent_path[JVK_FS_PATH_LEN];
    jvk_fs_status_t st = vfs_split(dst, parent_path, (int)sizeof(parent_path),
                                   name, name_cap);
    if (st != JVK_FS_OK) {
        return st;
    }
    int parent_id = 0;
    if (!vfs_resolve(fs, parent_path, &parent_id)) {
        return JVK_FS_ERR_NOENT;
    }
    const jvk_inode_t* parent = vfs_inode(fs, parent_id);
    if (parent == NULL || parent->type != JVK_FS_DIR) {
        return JVK_FS_ERR_NOTDIR;
    }
    if (!vfs_permitted(parent, pid, JVK_FS_W)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    *out_parent = parent_id;
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_move(jvk_vfs_t* fs, const char* src, const char* dst,
                         int pid, unsigned long ticks)
{
    if (fs == NULL || src == NULL || dst == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    int src_id = 0;
    if (!vfs_resolve(fs, src, &src_id) || src_id == JVK_FS_ROOT_ID) {
        return JVK_FS_ERR_NOENT;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, src_id);
    jvk_inode_t* old_parent = vfs_inode_mut(fs, ino->parent);
    if (old_parent == NULL || !vfs_permitted(old_parent, pid, JVK_FS_W)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }

    int new_parent = 0;
    char name[JVK_FS_NAME_LEN];
    jvk_fs_status_t st = resolve_destination(fs, dst, ino->name, pid,
                                             &new_parent, name,
                                             (int)sizeof(name));
    if (st != JVK_FS_OK) {
        return st;
    }
    if (new_parent == ino->parent && strcmp(name, ino->name) == 0) {
        return JVK_FS_OK;
    }
    if (vfs_child(fs, new_parent, name) != 0) {
        return JVK_FS_ERR_EXIST;
    }
    if (ino->type == JVK_FS_DIR && vfs_ancestor_of(fs, src_id, new_parent)) {
        return JVK_FS_ERR_INVAL;
    }

    ino->parent = new_parent;
    snprintf(ino->name, sizeof(ino->name), "%s", name);
    ino->modified_ticks = ticks;
    fs->renames++;
    vfs_event(fs, "FS_MOVED from=%s to_parent=%d name=%s", src, new_parent, name);
    return JVK_FS_OK;
}

/* ---- copy -------------------------------------------------------------- */

static jvk_fs_status_t copy_into(jvk_vfs_t* fs, int src_id, int dst_parent,
                                 const char* name, int pid,
                                 unsigned long ticks, int* out_id)
{
    const jvk_inode_t* src = vfs_inode(fs, src_id);
    const jvk_inode_t* parent = vfs_inode(fs, dst_parent);
    if (src == NULL || parent == NULL || parent->type != JVK_FS_DIR) {
        return JVK_FS_ERR_NOTDIR;
    }
    if (!vfs_permitted(parent, pid, JVK_FS_W)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    if (vfs_child(fs, dst_parent, name) != 0) {
        return JVK_FS_ERR_EXIST;
    }

    int new_id = vfs_new_inode(fs);
    if (new_id < 0) {
        return JVK_FS_ERR_FULL;
    }
    jvk_inode_t* dst = vfs_inode_mut(fs, new_id);
    snprintf(dst->name, sizeof(dst->name), "%s", name);
    dst->type           = src->type;
    dst->parent         = dst_parent;
    dst->owner          = src->owner;
    dst->mode           = src->mode;
    dst->hidden         = src->hidden;
    dst->readonly       = src->readonly;
    dst->created_ticks  = ticks;
    dst->modified_ticks = ticks;

    if (src->type == JVK_FS_FILE) {
        char* buf = NULL;
        int len = 0;
        jvk_fs_status_t st = vfs_read_content(fs, 0, (jvk_inode_t*)src,
                                              &buf, &len);
        if (st != JVK_FS_OK) {
            dst->used = 0;
            fs->inode_count--;
            return st;
        }
        st = vfs_store(fs, dst, pid, buf, len, ticks);
        free(buf);
        if (st != JVK_FS_OK) {
            dst->used = 0;
            fs->inode_count--;
            return st;
        }
    } else {
        for (int i = 0; i < JVK_FS_MAX_INODES; i++) {
            if (fs->inodes[i].used && fs->inodes[i].parent == src_id) {
                char child_name[JVK_FS_NAME_LEN];
                snprintf(child_name, sizeof(child_name), "%s",
                         fs->inodes[i].name);
                jvk_fs_status_t st = copy_into(fs, fs->inodes[i].id, new_id,
                                               child_name, pid, ticks, NULL);
                if (st != JVK_FS_OK) {
                    return st;
                }
            }
        }
    }
    if (out_id != NULL) {
        *out_id = new_id;
    }
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_copy(jvk_vfs_t* fs, const char* src, const char* dst,
                         int pid, unsigned long ticks, int* out_id)
{
    if (fs == NULL || src == NULL || dst == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    int src_id = 0;
    if (!vfs_resolve(fs, src, &src_id) || src_id == JVK_FS_ROOT_ID) {
        return JVK_FS_ERR_NOENT;
    }
    const jvk_inode_t* ino = vfs_inode(fs, src_id);
    if (!vfs_permitted(ino, pid, JVK_FS_R)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    int dst_parent = 0;
    char name[JVK_FS_NAME_LEN];
    jvk_fs_status_t st = resolve_destination(fs, dst, ino->name, pid,
                                             &dst_parent, name,
                                             (int)sizeof(name));
    if (st != JVK_FS_OK) {
        return st;
    }
    if (ino->type == JVK_FS_DIR && vfs_ancestor_of(fs, src_id, dst_parent)) {
        return JVK_FS_ERR_INVAL;
    }
    st = copy_into(fs, src_id, dst_parent, name, pid, ticks, out_id);
    if (st == JVK_FS_OK) {
        fs->copies++;
        vfs_event(fs, "FS_COPIED from=%s to_parent=%d name=%s",
                  src, dst_parent, name);
    }
    return st;
}

/* ---- chmod ------------------------------------------------------------- */

jvk_fs_status_t vfs_chmod(jvk_vfs_t* fs, const char* path, int pid,
                          int mode, int hidden, int readonly,
                          unsigned long ticks)
{
    if (fs == NULL || path == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    int id = 0;
    if (!vfs_resolve(fs, path, &id) || id == JVK_FS_ROOT_ID) {
        return JVK_FS_ERR_NOENT;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, id);
    if (ino == NULL) {
        return JVK_FS_ERR_NOENT;
    }
    if (pid != 0 && pid != ino->owner) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    if (mode >= 0) {
        ino->mode = (unsigned int)mode & 0777u;
    }
    if (hidden >= 0) {
        ino->hidden = hidden ? 1 : 0;
    }
    if (readonly >= 0) {
        ino->readonly = readonly ? 1 : 0;
    }
    ino->modified_ticks = ticks;
    vfs_event(fs, "FS_CHMOD path=%s mode=%03o readonly=%d hidden=%d",
              path, ino->mode, ino->readonly, ino->hidden);
    return JVK_FS_OK;
}

/* ---- open file table --------------------------------------------------- */

jvk_fs_status_t vfs_open(jvk_vfs_t* fs, const char* path, int pid,
                         const char* mode, unsigned long ticks, int* out_fd)
{
    if (fs == NULL || path == NULL || mode == NULL || out_fd == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    int id = 0;
    if (!vfs_resolve(fs, path, &id)) {
        return JVK_FS_ERR_NOENT;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, id);
    if (ino == NULL) {
        return JVK_FS_ERR_NOENT;
    }
    if (ino->type != JVK_FS_FILE) {
        return JVK_FS_ERR_ISDIR;
    }
    int want = 0;
    if (strchr(mode, 'r') != NULL) want |= JVK_FS_R;
    if (strchr(mode, 'w') != NULL) want |= JVK_FS_W;
    if (want == 0) {
        return JVK_FS_ERR_INVAL;
    }
    if ((want & JVK_FS_R) && !vfs_permitted(ino, pid, JVK_FS_R)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    if ((want & JVK_FS_W) && !vfs_permitted(ino, pid, JVK_FS_W)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    int fd = ot_open(&fs->open, id, pid, mode, ticks);
    if (fd < 0) {
        return JVK_FS_ERR_FD;
    }
    *out_fd = fd;
    vfs_event(fs, "FS_OPEN fd=%d path=%s pid=%d mode=%s", fd, path, pid, mode);
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_close(jvk_vfs_t* fs, int fd, int pid)
{
    if (fs == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    int idx = ot_find(&fs->open, fd);
    if (idx < 0) {
        return JVK_FS_ERR_FD;
    }
    jvk_open_file_t* e = &fs->open.entries[idx];
    if (pid != 0 && e->owner != pid) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }
    char path[JVK_FS_PATH_LEN];
    vfs_build_path(fs, e->inode_id, path, (int)sizeof(path));
    ot_close(&fs->open, fd);
    vfs_event(fs, "FS_CLOSE fd=%d path=%s pid=%d", fd, path, pid);
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_read_fd(jvk_vfs_t* fs, int fd, int pid,
                            char* out, int cap, int* out_len)
{
    if (fs == NULL || out == NULL || cap <= 0) {
        return JVK_FS_ERR_INVAL;
    }
    int idx = ot_find(&fs->open, fd);
    if (idx < 0) {
        return JVK_FS_ERR_FD;
    }
    jvk_open_file_t* e = &fs->open.entries[idx];
    if (strchr(e->mode, 'r') == NULL) {
        return JVK_FS_ERR_PERM;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, e->inode_id);
    if (ino == NULL) {
        return JVK_FS_ERR_NOENT;
    }
    if (e->cursor >= ino->size) {
        out[0] = '\0';
        if (out_len != NULL) {
            *out_len = 0;
        }
        return JVK_FS_OK;
    }
    int want = ino->size - e->cursor;
    if (want > cap - 1) {
        want = cap - 1;
    }
    char* buf = NULL;
    int total = 0;
    jvk_fs_status_t st = vfs_read_content(fs, pid, ino, &buf, &total);
    if (st != JVK_FS_OK) {
        return st;
    }
    memcpy(out, buf + e->cursor, (size_t)want);
    out[want] = '\0';
    free(buf);
    e->cursor += want;
    fs->reads++;
    if (out_len != NULL) {
        *out_len = want;
    }
    return JVK_FS_OK;
}

jvk_fs_status_t vfs_write_fd(jvk_vfs_t* fs, int fd, int pid,
                             const char* data, int len,
                             unsigned long ticks, int* out_written)
{
    if (fs == NULL || data == NULL || len < 0) {
        return JVK_FS_ERR_INVAL;
    }
    int idx = ot_find(&fs->open, fd);
    if (idx < 0) {
        return JVK_FS_ERR_FD;
    }
    jvk_open_file_t* e = &fs->open.entries[idx];
    if (strchr(e->mode, 'w') == NULL) {
        return JVK_FS_ERR_PERM;
    }
    jvk_inode_t* ino = vfs_inode_mut(fs, e->inode_id);
    if (ino == NULL) {
        return JVK_FS_ERR_NOENT;
    }
    if (!vfs_permitted(ino, pid, JVK_FS_W)) {
        fs->denied++;
        return JVK_FS_ERR_PERM;
    }

    char* buf = NULL;
    int old_len = 0;
    jvk_fs_status_t st = vfs_read_content(fs, 0, ino, &buf, &old_len);
    if (st != JVK_FS_OK) {
        return st;
    }
    int cursor = e->cursor;
    if (cursor < 0) cursor = 0;
    if (cursor > old_len) cursor = old_len;
    int total = old_len + len;
    char* merged = (char*)malloc((size_t)total + 1);
    if (merged == NULL) {
        free(buf);
        return JVK_FS_ERR_NOSPC;
    }
    memcpy(merged, buf, (size_t)cursor);
    memcpy(merged + cursor, data, (size_t)len);
    memcpy(merged + cursor + len, buf + cursor, (size_t)(old_len - cursor));
    merged[total] = '\0';
    free(buf);

    st = vfs_store(fs, ino, pid, merged, total, ticks);
    free(merged);
    if (st != JVK_FS_OK) {
        return st;
    }
    e->cursor = cursor + len;
    fs->writes++;
    if (out_written != NULL) {
        *out_written = len;
    }
    return JVK_FS_OK;
}

int vfs_close_owner(jvk_vfs_t* fs, int pid)
{
    if (fs == NULL) {
        return 0;
    }
    return ot_close_owner(&fs->open, pid);
}
