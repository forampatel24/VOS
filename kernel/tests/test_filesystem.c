/*
 * kernel/tests/test_filesystem.c
 *
 * JARVIS OS — virtual filesystem smoke test (M5).
 *
 * Covers directory creation, file read/write, append, rename, copy,
 * move, delete, search, permissions, readonly files, open-file table,
 * block accounting and teardown.
 * Run from kernel/:  make test
 */

#include <stdio.h>
#include <string.h>

#include "filesystem/vfs.h"

#define MAX_EVENTS 64

static int  g_failures = 0;
static char g_events[MAX_EVENTS][96];
static int  g_event_count = 0;

static void capture(void* user, const char* message)
{
    (void)user;
    if (g_event_count < MAX_EVENTS) {
        snprintf(g_events[g_event_count], sizeof(g_events[0]), "%s", message);
        g_event_count++;
    }
}

static int has_event(const char* prefix)
{
    size_t len = strlen(prefix);
    for (int i = 0; i < g_event_count; i++) {
        if (strncmp(g_events[i], prefix, len) == 0) {
            return 1;
        }
    }
    return 0;
}

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static void test_basic_tree(void)
{
    jvk_vfs_t fs;
    expect(vfs_init(&fs, 64, 256) == 1, "init vfs");
    vfs_set_observer(&fs, capture, NULL);

    int id = 0;
    expect(vfs_mkdir(&fs, "/Home", 0, 0755u, 10, &id) == JVK_FS_OK, "mkdir /Home");
    expect(vfs_create(&fs, "/Home/a.txt", 0, 0644u, "hello", 5, 11, &id)
               == JVK_FS_OK, "create a.txt");
    expect(has_event("FS_MKDIR"), "mkdir event emitted");
    expect(has_event("FS_CREATE"), "create event emitted");

    char buf[128];
    int len = 0;
    expect(vfs_read_path(&fs, "/Home/a.txt", 0, buf, (int)sizeof(buf), &len)
               == JVK_FS_OK, "read a.txt");
    expect(len == 5 && strcmp(buf, "hello") == 0, "content roundtrip");

    int ids[8];
    int n = vfs_list(&fs, "/Home", ids, 8);
    expect(n == 1, "list /Home has one entry");

    expect(vfs_write_path(&fs, "/Home/a.txt", 0, " world", 6, 1, 12, NULL)
               == JVK_FS_OK, "append to a.txt");
    expect(vfs_read_path(&fs, "/Home/a.txt", 0, buf, (int)sizeof(buf), &len)
               == JVK_FS_OK && len == 11, "append grew file");
    expect(strcmp(buf, "hello world") == 0, "append content");

    expect(vfs_rename(&fs, "/Home/a.txt", "b.txt", 0, 13) == JVK_FS_OK,
           "rename a.txt -> b.txt");
    expect(vfs_resolve(&fs, "/Home/b.txt", &id) == 1, "renamed file resolves");
    expect(vfs_resolve(&fs, "/Home/a.txt", &id) == 0, "old name gone");

    int copy_id = 0;
    expect(vfs_copy(&fs, "/Home/b.txt", "/Home/c.txt", 0, 14, &copy_id)
               == JVK_FS_OK, "copy b.txt -> c.txt");
    expect(vfs_read_path(&fs, "/Home/c.txt", 0, buf, (int)sizeof(buf), &len)
               == JVK_FS_OK && strcmp(buf, "hello world") == 0,
           "copy preserved content");

    expect(vfs_move(&fs, "/Home/c.txt", "/c.txt", 0, 15) == JVK_FS_OK,
           "move c.txt to root");
    expect(vfs_resolve(&fs, "/c.txt", &id) == 1, "moved file at root");
    expect(vfs_resolve(&fs, "/Home/c.txt", &id) == 0, "source removed");

    expect(vfs_delete(&fs, "/c.txt", 0) == JVK_FS_OK, "delete /c.txt");
    expect(vfs_resolve(&fs, "/c.txt", &id) == 0, "deleted file gone");
    expect(has_event("FS_DELETE"), "delete event emitted");

    vfs_shutdown(&fs);
}

static void test_paths_and_search(void)
{
    jvk_vfs_t fs;
    vfs_init(&fs, 32, 256);
    int id = 0;
    vfs_mkdir(&fs, "/Docs", 0, 0755u, 1, &id);
    vfs_mkdir(&fs, "/Docs/Sub", 0, 0755u, 2, &id);
    vfs_create(&fs, "/Docs/Sub/notes.md", 0, 0644u, "n", 1, 3, &id);

    expect(vfs_resolve(&fs, "/Docs/Sub/../Sub/notes.md", &id) == 1,
           "dot-dot resolves");
    expect(vfs_resolve(&fs, "Docs//Sub", &id) == 1, "double slash tolerated");

    int ids[8];
    int n = vfs_search(&fs, "notes", ids, 8);
    expect(n == 1, "search finds notes.md");
    n = vfs_search(&fs, "SUB", ids, 8);
    expect(n == 1, "search is case-insensitive");

    char path[JVK_FS_PATH_LEN];
    vfs_build_path(&fs, id, path, (int)sizeof(path));
    expect(strcmp(path, "/") == 0 || path[0] == '/', "build_path rooted");
    vfs_shutdown(&fs);
}

static void test_permissions(void)
{
    jvk_vfs_t fs;
    vfs_init(&fs, 32, 256);
    int id = 0;
    vfs_create(&fs, "/locked.txt", 0, 0644u, "secret", 6, 1, &id);
    vfs_chmod(&fs, "/locked.txt", 0, 0600, -1, -1, 2);
    char buf[32];
    int len = 0;
    expect(vfs_read_path(&fs, "/locked.txt", 9, buf, (int)sizeof(buf), &len)
               == JVK_FS_ERR_PERM, "other cannot read 0600 file");
    expect(fs.denied > 0, "denied counter incremented");
    expect(vfs_read_path(&fs, "/locked.txt", 0, buf, (int)sizeof(buf), &len)
               == JVK_FS_OK, "owner can read");

    vfs_chmod(&fs, "/locked.txt", 0, -1, 0, 1, 3);
    expect(vfs_write_path(&fs, "/locked.txt", 0, "x", 1, 0, 4, NULL)
               == JVK_FS_ERR_READONLY, "readonly blocks even owner");

    int fd = -1;
    vfs_chmod(&fs, "/locked.txt", 0, -1, 0, 0, 5);
    expect(vfs_open(&fs, "/locked.txt", 0, "r", 6, &fd) == JVK_FS_OK,
           "open for read");
    expect(vfs_write_fd(&fs, fd, 0, "x", 1, 7, NULL) == JVK_FS_ERR_PERM,
           "read-only descriptor rejects write");
    expect(vfs_close(&fs, fd, 0) == JVK_FS_OK, "close fd");
    vfs_shutdown(&fs);
}

static void test_open_table(void)
{
    jvk_vfs_t fs;
    vfs_init(&fs, 32, 256);
    int id = 0;
    vfs_create(&fs, "/data.txt", 0, 0644u, "abcdef", 6, 1, &id);

    int fd = -1;
    expect(vfs_open(&fs, "/data.txt", 0, "r", 2, &fd) == JVK_FS_OK,
           "open data.txt");
    expect(fd >= 3, "fd starts at 3");
    char buf[16];
    int len = 0;
    expect(vfs_read_fd(&fs, fd, 0, buf, (int)sizeof(buf), &len) == JVK_FS_OK,
           "read via fd");
    expect(len == 6 && strcmp(buf, "abcdef") == 0, "fd content");

    expect(vfs_write_fd(&fs, fd, 0, "X", 1, 3, NULL) == JVK_FS_ERR_PERM,
           "r mode rejects write");
    expect(vfs_close(&fs, fd, 4) == JVK_FS_ERR_PERM,
           "other pid cannot close fd");

    int fd2 = -1;
    expect(vfs_open(&fs, "/data.txt", 0, "w", 4, &fd2) == JVK_FS_OK,
           "open for write");
    expect(vfs_write_fd(&fs, fd2, 0, "xyz", 3, 5, NULL) == JVK_FS_OK,
           "write via fd");
    expect(vfs_close(&fs, fd2, 0) == JVK_FS_OK, "close write fd");
    expect(vfs_close_owner(&fs, 0) == 1, "owner cleanup closes fd 1");
    vfs_shutdown(&fs);
}

static void test_disk_accounting(void)
{
    jvk_vfs_t fs;
    vfs_init(&fs, 16, 128);
    int id = 0;
    int used_before = disk_used_blocks(&fs.disk);
    char big[400];
    memset(big, 'a', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    expect(vfs_create(&fs, "/big.txt", 0, 0644u, big, (int)strlen(big),
                      1, &id) == JVK_FS_OK, "create 399-byte file");
    int used_after = disk_used_blocks(&fs.disk);
    expect(used_after > used_before, "disk usage grew");
    const jvk_inode_t* ino = vfs_inode(&fs, id);
    expect(ino != NULL && ino->block_count >= 4, "file spans >= 4 blocks");
    expect(vfs_delete(&fs, "/big.txt", 0) == JVK_FS_OK, "delete big file");
    expect(disk_used_blocks(&fs.disk) == used_before, "blocks reclaimed");
    vfs_shutdown(&fs);
}

int main(void)
{
    test_basic_tree();
    test_paths_and_search();
    test_permissions();
    test_open_table();
    test_disk_accounting();

    if (g_failures == 0) {
        printf("test_filesystem: all checks passed\n");
        return 0;
    }
    printf("test_filesystem: %d failure(s)\n", g_failures);
    return 1;
}
