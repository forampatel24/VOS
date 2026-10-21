/*
 * kernel/filesystem/vfs.c
 *
 * JARVIS OS — VFS core: inode tree, paths, permissions, lookup.
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "vfs_internal.h"

/* ---- lifecycle --------------------------------------------------------- */

int vfs_init(jvk_vfs_t* fs, int blocks, int block_size)
{
    if (fs == NULL) {
        return 0;
    }
    memset(fs, 0, sizeof(*fs));
    if (!disk_init(&fs->disk, blocks, block_size)) {
        return 0;
    }
    ot_init(&fs->open);

    jvk_inode_t* root = &fs->inodes[0];
    root->used          = 1;
    root->id            = JVK_FS_ROOT_ID;
    root->name[0]       = '/';
    root->name[1]       = '\0';
    root->type          = JVK_FS_DIR;
    root->parent        = 0;
    root->owner         = 0;
    root->mode          = 0755u;
    root->created_ticks = 0;
    root->modified_ticks = 0;

    fs->next_id     = JVK_FS_ROOT_ID + 1;
    fs->inode_count = 1;
    return 1;
}

void vfs_shutdown(jvk_vfs_t* fs)
{
    if (fs == NULL) {
        return;
    }
    disk_shutdown(&fs->disk);
    fs->inode_count = 0;
}

void vfs_set_observer(jvk_vfs_t* fs, vfs_event_fn fn, void* user)
{
    if (fs != NULL) {
        fs->on_event   = fn;
        fs->event_user = user;
    }
}

void vfs_event(jvk_vfs_t* fs, const char* fmt, ...)
{
    if (fs == NULL || fs->on_event == NULL || fmt == NULL) {
        return;
    }
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    fs->on_event(fs->event_user, msg);
}

/* ---- inode helpers ----------------------------------------------------- */

const jvk_inode_t* vfs_inode(const jvk_vfs_t* fs, int id)
{
    if (fs == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_FS_MAX_INODES; i++) {
        if (fs->inodes[i].used && fs->inodes[i].id == id) {
            return &fs->inodes[i];
        }
    }
    return NULL;
}

jvk_inode_t* vfs_inode_mut(jvk_vfs_t* fs, int id)
{
    return (jvk_inode_t*)vfs_inode(fs, id);
}

int vfs_new_inode(jvk_vfs_t* fs)
{
    if (fs == NULL) {
        return -1;
    }
    for (int i = 0; i < JVK_FS_MAX_INODES; i++) {
        if (!fs->inodes[i].used) {
            int id = fs->next_id++;
            memset(&fs->inodes[i], 0, sizeof(fs->inodes[i]));
            fs->inodes[i].used = 1;
            fs->inodes[i].id   = id;
            fs->inode_count++;
            return id;
        }
    }
    return -1;
}

int vfs_child(const jvk_vfs_t* fs, int parent_id, const char* name)
{
    if (fs == NULL || name == NULL) {
        return 0;
    }
    for (int i = 0; i < JVK_FS_MAX_INODES; i++) {
        const jvk_inode_t* ino = &fs->inodes[i];
        if (ino->used && ino->parent == parent_id &&
            strcmp(ino->name, name) == 0) {
            return ino->id;
        }
    }
    return 0;
}

int vfs_ancestor_of(const jvk_vfs_t* fs, int ancestor, int id)
{
    int guard = 0;
    while (id != 0 && guard++ < JVK_FS_MAX_DEPTH) {
        if (id == ancestor) {
            return 1;
        }
        const jvk_inode_t* ino = vfs_inode(fs, id);
        if (ino == NULL) {
            break;
        }
        id = ino->parent;
    }
    return 0;
}

int vfs_dir_is_empty(const jvk_vfs_t* fs, int dir_id)
{
    if (fs == NULL) {
        return 1;
    }
    for (int i = 0; i < JVK_FS_MAX_INODES; i++) {
        if (fs->inodes[i].used && fs->inodes[i].parent == dir_id) {
            return 0;
        }
    }
    return 1;
}

int vfs_valid_name(const char* name)
{
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    if (strlen(name) >= JVK_FS_NAME_LEN) {
        return 0;
    }
    if (strchr(name, '/') != NULL) {
        return 0;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    return 1;
}

int vfs_permitted(const jvk_inode_t* inode, int pid, int want)
{
    if (inode == NULL) {
        return 0;
    }
    if ((want & JVK_FS_W) && inode->readonly) {
        return 0;
    }
    if (pid == 0) {
        return 1; /* system / kernel */
    }
    unsigned int bits = (pid == inode->owner)
                            ? (inode->mode >> 6) & 7u
                            : inode->mode & 7u;
    return (bits & (unsigned int)want) == (unsigned int)want;
}

/* ---- paths ------------------------------------------------------------- */

int vfs_resolve(const jvk_vfs_t* fs, const char* path, int* out_id)
{
    if (fs == NULL || path == NULL || out_id == NULL) {
        return 0;
    }
    char buf[JVK_FS_PATH_LEN];
    snprintf(buf, sizeof(buf), "%s", path);

    int cur = JVK_FS_ROOT_ID;
    char* p = buf;
    while (*p != '\0') {
        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        char* start = p;
        while (*p != '\0' && *p != '/') {
            p++;
        }
        char saved = *p;
        *p = '\0';

        if (strcmp(start, ".") == 0) {
            /* stay */
        } else if (strcmp(start, "..") == 0) {
            const jvk_inode_t* ino = vfs_inode(fs, cur);
            cur = (ino != NULL && ino->parent != 0) ? ino->parent
                                                    : JVK_FS_ROOT_ID;
        } else {
            const jvk_inode_t* dir = vfs_inode(fs, cur);
            if (dir == NULL || dir->type != JVK_FS_DIR) {
                return 0;
            }
            int child = vfs_child(fs, cur, start);
            if (child == 0) {
                return 0;
            }
            cur = child;
        }
        *p = saved;
    }
    *out_id = cur;
    return 1;
}

void vfs_build_path(const jvk_vfs_t* fs, int id, char* out, int cap)
{
    if (out == NULL || cap <= 0) {
        return;
    }
    if (fs == NULL || id == JVK_FS_ROOT_ID) {
        snprintf(out, (size_t)cap, "/");
        return;
    }

    int chain[JVK_FS_MAX_DEPTH];
    int depth = 0;
    int cur = id;
    while (cur != 0 && cur != JVK_FS_ROOT_ID && depth < JVK_FS_MAX_DEPTH) {
        chain[depth++] = cur;
        const jvk_inode_t* ino = vfs_inode(fs, cur);
        if (ino == NULL) {
            break;
        }
        cur = ino->parent;
    }

    out[0] = '\0';
    int used = 0;
    for (int i = depth - 1; i >= 0; i--) {
        const jvk_inode_t* ino = vfs_inode(fs, chain[i]);
        if (ino == NULL) {
            continue;
        }
        int n = snprintf(out + used, (size_t)(cap - used), "/%s", ino->name);
        if (n < 0 || used + n >= cap) {
            break;
        }
        used += n;
    }
    if (used == 0) {
        snprintf(out, (size_t)cap, "/");
    }
}

jvk_fs_status_t vfs_split(const char* path, char* parent_path, int parent_cap,
                          char* name, int name_cap)
{
    if (path == NULL || parent_path == NULL || name == NULL) {
        return JVK_FS_ERR_INVAL;
    }
    const char* last = strrchr(path, '/');
    if (last == NULL) {
        snprintf(parent_path, (size_t)parent_cap, "/");
        snprintf(name, (size_t)name_cap, "%s", path);
    } else {
        int plen = (int)(last - path);
        if (plen == 0) {
            snprintf(parent_path, (size_t)parent_cap, "/");
        } else {
            snprintf(parent_path, (size_t)parent_cap, "%.*s", plen, path);
        }
        snprintf(name, (size_t)name_cap, "%s", last + 1);
    }
    return vfs_valid_name(name) ? JVK_FS_OK : JVK_FS_ERR_INVAL;
}

/* ---- listing ----------------------------------------------------------- */

int vfs_list(const jvk_vfs_t* fs, const char* path, int* ids, int cap)
{
    int dir_id = 0;
    if (fs == NULL || ids == NULL || cap <= 0) {
        return -1;
    }
    if (!vfs_resolve(fs, path != NULL ? path : "/", &dir_id)) {
        return -1;
    }
    const jvk_inode_t* dir = vfs_inode(fs, dir_id);
    if (dir == NULL || dir->type != JVK_FS_DIR) {
        return -1;
    }
    int count = 0;
    for (int i = 0; i < JVK_FS_MAX_INODES && count < cap; i++) {
        if (fs->inodes[i].used && fs->inodes[i].parent == dir_id) {
            ids[count++] = fs->inodes[i].id;
        }
    }
    return count;
}

static int contains_ci(const char* haystack, const char* needle)
{
    if (needle[0] == '\0') {
        return 1;
    }
    size_t nlen = strlen(needle);
    for (const char* h = haystack; *h != '\0'; h++) {
        size_t i = 0;
        while (i < nlen && h[i] != '\0') {
            char a = h[i];
            char b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) {
                break;
            }
            i++;
        }
        if (i == nlen) {
            return 1;
        }
    }
    return 0;
}

int vfs_search(const jvk_vfs_t* fs, const char* pattern, int* ids, int cap)
{
    if (fs == NULL || pattern == NULL || ids == NULL || cap <= 0) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < JVK_FS_MAX_INODES && count < cap; i++) {
        const jvk_inode_t* ino = &fs->inodes[i];
        if (!ino->used || ino->id == JVK_FS_ROOT_ID) {
            continue;
        }
        if (contains_ci(ino->name, pattern)) {
            ids[count++] = ino->id;
        }
    }
    return count;
}
