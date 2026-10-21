/*
 * kernel/core/kernel_fs.c
 *
 * JARVIS OS — filesystem boot, seeding and snapshot serialization.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "kernel_fs_internal.h"
#include "../filesystem/vfs_internal.h"

/* ---- boot -------------------------------------------------------------- */

int kfs_boot(jvk_vfs_t* fs, const cJSON* config_root)
{
    int blocks     = JVK_DISK_DEFAULT_BLOCKS;
    int block_size = JVK_DISK_DEFAULT_BLOCK_SIZE;

    const cJSON* cfg = cJSON_GetObjectItemCaseSensitive(config_root, "filesystem");
    if (cJSON_IsObject(cfg)) {
        const cJSON* b = cJSON_GetObjectItemCaseSensitive(cfg, "blocks");
        if (cJSON_IsNumber(b) && b->valuedouble > 0) {
            blocks = (int)b->valuedouble;
        }
        const cJSON* s = cJSON_GetObjectItemCaseSensitive(cfg, "blockSize");
        if (cJSON_IsNumber(s) && s->valuedouble > 0) {
            block_size = (int)s->valuedouble;
        }
    }
    return vfs_init(fs, blocks, block_size);
}

static void seed_dir(jvk_vfs_t* fs, const char* path, unsigned long ticks)
{
    (void)vfs_mkdir(fs, path, 0, 0755u, ticks, NULL);
}

static void seed_file(jvk_vfs_t* fs, const char* path, const char* content,
                      unsigned long ticks)
{
    (void)vfs_create(fs, path, 0, 0644u, content,
                     (int)strlen(content), ticks, NULL);
}

void kfs_seed(jvk_vfs_t* fs, unsigned long ticks)
{
    if (fs == NULL) {
        return;
    }
    static const char* dirs[] = {
        "/System",
        "/Agents",
        "/Users",
        "/Users/Admin",
        "/Users/Admin/Desktop",
        "/Users/Admin/Documents",
        "/Users/Admin/Downloads",
        "/Users/Admin/Pictures",
        "/Users/Admin/Music",
        "/Temp",
        "/Logs",
        "/Swap",
    };
    for (int i = 0; i < (int)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        seed_dir(fs, dirs[i], ticks);
    }

    seed_file(fs, "/System/README.txt",
              "JARVIS OS virtual filesystem\n"
              "This storage is entirely in memory. No host path is used.\n"
              "Directories may be created, files renamed, copied and searched.\n",
              ticks);
    seed_file(fs, "/Users/Admin/Documents/welcome.txt",
              "Welcome, Admin.\n"
              "Open File Explorer to browse the virtual disk, or edit this file.\n",
              ticks);
}

void kfs_format(jvk_vfs_t* fs, unsigned long ticks)
{
    if (fs == NULL) {
        return;
    }
    int blocks = fs->disk.total_blocks;
    int block_size = fs->disk.block_size;
    vfs_shutdown(fs);
    if (!vfs_init(fs, blocks > 0 ? blocks : JVK_DISK_DEFAULT_BLOCKS,
                  block_size > 0 ? block_size : JVK_DISK_DEFAULT_BLOCK_SIZE)) {
        return;
    }
    vfs_event(fs, "DISK_FORMATTED blocks=%d block_size=%d",
              fs->disk.total_blocks, fs->disk.block_size);
    kfs_seed(fs, ticks);
}

int kfs_close_owner(jvk_vfs_t* fs, int pid)
{
    return vfs_close_owner(fs, pid);
}

/* ---- JSON -------------------------------------------------------------- */

void kfs_add_entry_json(const jvk_vfs_t* fs, const jvk_inode_t* ino,
                        cJSON* obj)
{
    if (fs == NULL || ino == NULL || obj == NULL) {
        return;
    }
    char mode[JVK_FS_MODE_LEN];
    char path[JVK_FS_PATH_LEN];
    jvk_fs_mode_string(ino->mode, mode, (int)sizeof(mode));
    vfs_build_path(fs, ino->id, path, (int)sizeof(path));

    cJSON_AddNumberToObject(obj, "id", ino->id);
    cJSON_AddStringToObject(obj, "name", ino->name);
    cJSON_AddStringToObject(obj, "type", jvk_fs_type_name(ino->type));
    cJSON_AddNumberToObject(obj, "parent", ino->parent);
    cJSON_AddStringToObject(obj, "path", path);
    cJSON_AddNumberToObject(obj, "size", ino->size);
    cJSON_AddNumberToObject(obj, "owner", ino->owner);
    cJSON_AddStringToObject(obj, "mode", mode);
    cJSON_AddNumberToObject(obj, "mode_bits", (double)ino->mode);
    cJSON_AddNumberToObject(obj, "block_count", ino->block_count);
    cJSON* blocks = cJSON_CreateArray();
    for (int i = 0; i < ino->block_count; i++) {
        cJSON_AddItemToArray(blocks, cJSON_CreateNumber(ino->blocks[i]));
    }
    cJSON_AddItemToObject(obj, "blocks", blocks);
    cJSON_AddNumberToObject(obj, "created_ticks", (double)ino->created_ticks);
    cJSON_AddNumberToObject(obj, "modified_ticks", (double)ino->modified_ticks);
    cJSON_AddBoolToObject(obj, "hidden", ino->hidden != 0);
    cJSON_AddBoolToObject(obj, "readonly", ino->readonly != 0);
}

void kfs_add_disk_json(const jvk_vfs_t* fs, cJSON* obj)
{
    if (fs == NULL || obj == NULL) {
        return;
    }
    int used = disk_used_blocks(&fs->disk);
    cJSON_AddNumberToObject(obj, "total_blocks", fs->disk.total_blocks);
    cJSON_AddNumberToObject(obj, "block_size", fs->disk.block_size);
    cJSON_AddNumberToObject(obj, "used_blocks", used);
    cJSON_AddNumberToObject(obj, "free_blocks",
                            fs->disk.total_blocks - used);
    cJSON_AddNumberToObject(obj, "largest_free_run",
                            disk_largest_free_run(&fs->disk));
    cJSON_AddNumberToObject(obj, "capacity_bytes",
                            (double)fs->disk.total_blocks * fs->disk.block_size);
    cJSON_AddNumberToObject(obj, "used_bytes",
                            (double)used * fs->disk.block_size);
    cJSON_AddNumberToObject(obj, "read_ops", (double)fs->disk.read_ops);
    cJSON_AddNumberToObject(obj, "write_ops", (double)fs->disk.write_ops);
    cJSON_AddNumberToObject(obj, "alloc_calls", (double)fs->disk.alloc_calls);
    cJSON_AddNumberToObject(obj, "free_calls", (double)fs->disk.free_calls);
}

void kfs_add_stats_json(const jvk_vfs_t* fs, cJSON* obj)
{
    if (fs == NULL || obj == NULL) {
        return;
    }
    cJSON_AddNumberToObject(obj, "inodes_used", fs->inode_count);
    cJSON_AddNumberToObject(obj, "open_files", fs->open.count);
    cJSON_AddNumberToObject(obj, "creates", fs->creates);
    cJSON_AddNumberToObject(obj, "deletes", fs->deletes);
    cJSON_AddNumberToObject(obj, "mkdirs", fs->mkdirs);
    cJSON_AddNumberToObject(obj, "reads", fs->reads);
    cJSON_AddNumberToObject(obj, "writes", fs->writes);
    cJSON_AddNumberToObject(obj, "renames", fs->renames);
    cJSON_AddNumberToObject(obj, "copies", fs->copies);
    cJSON_AddNumberToObject(obj, "searches", fs->searches);
    cJSON_AddNumberToObject(obj, "denied", fs->denied);
}

void kfs_snapshot(const jvk_vfs_t* fs, cJSON* root)
{
    if (fs == NULL || root == NULL) {
        return;
    }
    cJSON* out = cJSON_CreateObject();

    cJSON* config = cJSON_CreateObject();
    cJSON_AddNumberToObject(config, "total_blocks", fs->disk.total_blocks);
    cJSON_AddNumberToObject(config, "block_size", fs->disk.block_size);
    cJSON_AddNumberToObject(config, "max_inodes", JVK_FS_MAX_INODES);
    cJSON_AddNumberToObject(config, "max_blocks_per_file", JVK_FS_MAX_BLOCKS);
    cJSON_AddNumberToObject(config, "max_open_files", JVK_FS_MAX_OPEN);
    cJSON_AddItemToObject(out, "config", config);

    cJSON* stats = cJSON_CreateObject();
    kfs_add_stats_json(fs, stats);
    cJSON_AddItemToObject(out, "stats", stats);

    cJSON* disk = cJSON_CreateObject();
    kfs_add_disk_json(fs, disk);
    cJSON_AddItemToObject(out, "disk", disk);

    cJSON* entries = cJSON_CreateArray();
    for (int i = 0; i < JVK_FS_MAX_INODES; i++) {
        if (!fs->inodes[i].used) {
            continue;
        }
        cJSON* item = cJSON_CreateObject();
        kfs_add_entry_json(fs, &fs->inodes[i], item);
        cJSON_AddItemToArray(entries, item);
    }
    cJSON_AddItemToObject(out, "entries", entries);

    cJSON* open_files = cJSON_CreateArray();
    for (int i = 0; i < JVK_FS_MAX_OPEN; i++) {
        const jvk_open_file_t* e = &fs->open.entries[i];
        if (!e->used) {
            continue;
        }
        const jvk_inode_t* ino = vfs_inode(fs, e->inode_id);
        char path[JVK_FS_PATH_LEN];
        vfs_build_path(fs, e->inode_id, path, (int)sizeof(path));
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "fd", e->fd);
        cJSON_AddNumberToObject(item, "inode_id", e->inode_id);
        cJSON_AddStringToObject(item, "name",
                                ino != NULL ? ino->name : "?");
        cJSON_AddStringToObject(item, "path", path);
        cJSON_AddNumberToObject(item, "owner", e->owner);
        cJSON_AddNumberToObject(item, "cursor", e->cursor);
        cJSON_AddStringToObject(item, "mode", e->mode);
        cJSON_AddNumberToObject(item, "opened_ticks",
                                (double)e->opened_ticks);
        cJSON_AddItemToArray(open_files, item);
    }
    cJSON_AddItemToObject(out, "open_files", open_files);

    cJSON_AddItemToObject(root, "filesystem", out);
}
