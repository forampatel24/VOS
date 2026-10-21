/*
 * kernel/core/kernel_fs_cmds.c
 *
 * JARVIS OS — fs_* command handlers (JSON in, JSON out).
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "kernel_fs_internal.h"

/* ---- request helpers --------------------------------------------------- */

static int req_int(const cJSON* req, const char* key, int def)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, key);
    return cJSON_IsNumber(item) ? (int)item->valuedouble : def;
}

static const char* req_str(const cJSON* req, const char* key,
                           const char* def)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, key);
    return cJSON_IsString(item) ? item->valuestring : def;
}

static int req_bool(const cJSON* req, const char* key, int def)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, key);
    return cJSON_IsBool(item) ? (cJSON_IsTrue(item) ? 1 : 0) : def;
}

static int req_pid(const cJSON* req)
{
    int pid = req_int(req, "pid", -1);
    if (pid < 0) {
        pid = req_int(req, "owner", 0);
    }
    return pid < 0 ? 0 : pid;
}

static unsigned int req_mode(const cJSON* req, unsigned int def)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, "mode");
    if (cJSON_IsNumber(item)) {
        return (unsigned int)item->valuedouble & 0777u;
    }
    if (cJSON_IsString(item)) {
        unsigned int mode = 0;
        if (jvk_fs_mode_parse(item->valuestring, &mode)) {
            return mode;
        }
    }
    return def;
}

static int fail_status(cJSON* result, jvk_fs_status_t st)
{
    cJSON_AddBoolToObject(result, "ok", 0);
    cJSON_AddStringToObject(result, "error", jvk_fs_status_name(st));
    return 1;
}

static void add_matching_entries(jvk_vfs_t* fs, const int* ids, int count,
                                 cJSON* result)
{
    cJSON* entries = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        const jvk_inode_t* ino = vfs_inode(fs, ids[i]);
        if (ino == NULL) {
            continue;
        }
        cJSON* item = cJSON_CreateObject();
        kfs_add_entry_json(fs, ino, item);
        cJSON_AddItemToArray(entries, item);
    }
    cJSON_AddItemToObject(result, "entries", entries);
    cJSON_AddNumberToObject(result, "count", cJSON_GetArraySize(entries));
    cJSON_AddBoolToObject(result, "ok", 1);
}

/* ---- handlers ---------------------------------------------------------- */

static int handle_list(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    (void)ticks;
    const char* path = req_str(req, "path", "/");
    int dir_id = 0;
    if (!vfs_resolve(fs, path, &dir_id)) {
        return fail_status(result, JVK_FS_ERR_NOENT);
    }
    const jvk_inode_t* dir = vfs_inode(fs, dir_id);
    if (dir == NULL || dir->type != JVK_FS_DIR) {
        return fail_status(result, JVK_FS_ERR_NOTDIR);
    }
    int ids[JVK_FS_MAX_INODES];
    int n = vfs_list(fs, path, ids, JVK_FS_MAX_INODES);
    if (n < 0) {
        n = 0;
    }
    char canonical[JVK_FS_PATH_LEN];
    vfs_build_path(fs, dir_id, canonical, (int)sizeof(canonical));
    add_matching_entries(fs, ids, n, result);
    cJSON_AddStringToObject(result, "path", canonical);
    cJSON_AddNumberToObject(result, "inode_id", dir_id);
    return 1;
}

static int handle_stat(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    (void)ticks;
    int id = 0;
    const char* path = req_str(req, "path", NULL);
    if (path == NULL || !vfs_resolve(fs, path, &id)) {
        return fail_status(result, JVK_FS_ERR_NOENT);
    }
    cJSON* entry = cJSON_CreateObject();
    kfs_add_entry_json(fs, vfs_inode(fs, id), entry);
    cJSON_AddItemToObject(result, "entry", entry);
    cJSON_AddBoolToObject(result, "ok", 1);
    return 1;
}

static int handle_create(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* path = req_str(req, "path", NULL);
    if (path == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    const char* content = req_str(req, "content", "");
    int id = 0;
    jvk_fs_status_t st = vfs_create(fs, path, req_pid(req),
                                    req_mode(req, 0644u), content,
                                    (int)strlen(content), ticks, &id);
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    int hidden = req_bool(req, "hidden", 0);
    int readonly = req_bool(req, "readonly", 0);
    if (hidden || readonly) {
        (void)vfs_chmod(fs, path, 0, -1, hidden, readonly, ticks);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "id", id);
    cJSON_AddStringToObject(result, "path", path);
    return 1;
}

static int handle_mkdir(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* path = req_str(req, "path", NULL);
    if (path == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    int id = 0;
    jvk_fs_status_t st = vfs_mkdir(fs, path, req_pid(req),
                                   req_mode(req, 0755u), ticks, &id);
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "id", id);
    cJSON_AddStringToObject(result, "path", path);
    return 1;
}

static int handle_read(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    (void)ticks;
    int pid = req_pid(req);
    int cap = JVK_FS_MAX_BLOCKS * fs->disk.block_size + 1;
    char* buf = (char*)malloc((size_t)cap);
    if (buf == NULL) {
        return fail_status(result, JVK_FS_ERR_NOSPC);
    }
    int len = 0;
    jvk_fs_status_t st;
    int fd = req_int(req, "fd", -1);
    if (fd >= 0) {
        st = vfs_read_fd(fs, fd, pid, buf, cap, &len);
    } else {
        const char* path = req_str(req, "path", NULL);
        st = path != NULL
                 ? vfs_read_path(fs, path, pid, buf, cap, &len)
                 : JVK_FS_ERR_INVAL;
    }
    if (st != JVK_FS_OK) {
        free(buf);
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "content", buf);
    cJSON_AddNumberToObject(result, "bytes", len);
    if (fd >= 0) {
        int idx = ot_find(&fs->open, fd);
        cJSON_AddNumberToObject(result, "cursor",
                                idx >= 0 ? fs->open.entries[idx].cursor : 0);
    }
    free(buf);
    return 1;
}

static int handle_write(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* content = req_str(req, "content", NULL);
    if (content == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    int pid = req_pid(req);
    int len = (int)strlen(content);
    int written = 0;
    jvk_fs_status_t st;
    int fd = req_int(req, "fd", -1);
    if (fd >= 0) {
        st = vfs_write_fd(fs, fd, pid, content, len, ticks, &written);
    } else {
        const char* path = req_str(req, "path", NULL);
        st = path != NULL
                 ? vfs_write_path(fs, path, pid, content, len,
                                  req_bool(req, "append", 0),
                                  ticks, &written)
                 : JVK_FS_ERR_INVAL;
    }
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "bytes_written", written);
    cJSON_AddNumberToObject(result, "pid", pid);
    return 1;
}

static int handle_delete(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    (void)ticks;
    const char* path = req_str(req, "path", NULL);
    if (path == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    jvk_fs_status_t st = vfs_delete(fs, path, req_pid(req));
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "path", path);
    return 1;
}

static int handle_rename(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* path = req_str(req, "path", NULL);
    const char* name = req_str(req, "new_name", req_str(req, "name", NULL));
    if (path == NULL || name == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    jvk_fs_status_t st = vfs_rename(fs, path, name, req_pid(req),
                                    ticks);
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "path", path);
    cJSON_AddStringToObject(result, "name", name);
    return 1;
}

static int handle_move_copy(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks, int copy)
{
    const char* src = req_str(req, "src", req_str(req, "from", NULL));
    const char* dst = req_str(req, "dst", req_str(req, "to", NULL));
    if (src == NULL || dst == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    int new_id = 0;
    jvk_fs_status_t st = copy
        ? vfs_copy(fs, src, dst, req_pid(req), ticks, &new_id)
        : vfs_move(fs, src, dst, req_pid(req), ticks);
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "src", src);
    cJSON_AddStringToObject(result, "dst", dst);
    if (copy) {
        cJSON_AddNumberToObject(result, "id", new_id);
    }
    return 1;
}

static int handle_search(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    (void)ticks;
    const char* pattern = req_str(req, "pattern", "");
    int ids[JVK_FS_MAX_INODES];
    int n = vfs_search(fs, pattern, ids, JVK_FS_MAX_INODES);
    fs->searches++;
    add_matching_entries(fs, ids, n, result);
    cJSON_AddStringToObject(result, "pattern", pattern);
    return 1;
}

static int handle_open(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* path = req_str(req, "path", NULL);
    if (path == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    int fd = -1;
    jvk_fs_status_t st = vfs_open(fs, path, req_pid(req),
                                  req_str(req, "mode", "r"),
                                  ticks, &fd);
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "fd", fd);
    cJSON_AddStringToObject(result, "path", path);
    return 1;
}

static int handle_close(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    (void)ticks;
    int fd = req_int(req, "fd", -1);
    if (fd < 0) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    jvk_fs_status_t st = vfs_close(fs, fd, req_pid(req));
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "fd", fd);
    return 1;
}

static int handle_chmod(jvk_vfs_t* fs, const cJSON* req, cJSON* result, unsigned long ticks)
{
    const char* path = req_str(req, "path", NULL);
    if (path == NULL) {
        return fail_status(result, JVK_FS_ERR_INVAL);
    }
    int mode = -1;
    const cJSON* mode_item = cJSON_GetObjectItemCaseSensitive(req, "mode");
    if (cJSON_IsNumber(mode_item)) {
        mode = (int)mode_item->valuedouble & 0777;
    } else if (cJSON_IsString(mode_item)) {
        unsigned int parsed = 0;
        if (jvk_fs_mode_parse(mode_item->valuestring, &parsed)) {
            mode = (int)parsed;
        }
    }
    jvk_fs_status_t st = vfs_chmod(fs, path, req_pid(req), mode,
                                   req_bool(req, "hidden", -1),
                                   req_bool(req, "readonly", -1),
                                   ticks);
    if (st != JVK_FS_OK) {
        return fail_status(result, st);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "path", path);
    return 1;
}

int kfs_handle(jvk_vfs_t* fs, const char* action,
               const cJSON* req, cJSON* result, unsigned long ticks)
{
    if (fs == NULL || action == NULL || req == NULL || result == NULL) {
        return 0;
    }
    if (strcmp(action, "fs_list") == 0)      return handle_list(fs, req, result, ticks);
    if (strcmp(action, "fs_stat") == 0)      return handle_stat(fs, req, result, ticks);
    if (strcmp(action, "fs_create") == 0)    return handle_create(fs, req, result, ticks);
    if (strcmp(action, "fs_mkdir") == 0)     return handle_mkdir(fs, req, result, ticks);
    if (strcmp(action, "fs_read") == 0)      return handle_read(fs, req, result, ticks);
    if (strcmp(action, "fs_write") == 0)     return handle_write(fs, req, result, ticks);
    if (strcmp(action, "fs_delete") == 0)    return handle_delete(fs, req, result, ticks);
    if (strcmp(action, "fs_rename") == 0)    return handle_rename(fs, req, result, ticks);
    if (strcmp(action, "fs_move") == 0)      return handle_move_copy(fs, req, result, ticks, 0);
    if (strcmp(action, "fs_copy") == 0)      return handle_move_copy(fs, req, result, ticks, 1);
    if (strcmp(action, "fs_search") == 0)    return handle_search(fs, req, result, ticks);
    if (strcmp(action, "fs_open") == 0)      return handle_open(fs, req, result, ticks);
    if (strcmp(action, "fs_close") == 0)     return handle_close(fs, req, result, ticks);
    if (strcmp(action, "fs_chmod") == 0)     return handle_chmod(fs, req, result, ticks);
    if (strcmp(action, "fs_format") == 0) {
        kfs_format(fs, ticks);
        cJSON_AddBoolToObject(result, "ok", 1);
        return 1;
    }
    return 0;
}
