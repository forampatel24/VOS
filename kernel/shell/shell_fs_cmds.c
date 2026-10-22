/*
 * kernel/shell/shell_fs_cmds.c
 *
 * JARVIS OS — filesystem shell commands (M8).
 * Paths are resolved against the shell cwd before they reach the VFS.
 */

#include <stdio.h>
#include <string.h>

#include "shell_internal.h"

static cJSON* fs_request(sh_ctx_t* ctx, const char* action, const char* path)
{
    (void)ctx;
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", action);
    if (path != NULL) {
        cJSON_AddStringToObject(req, "path", path);
    }
    cJSON_AddNumberToObject(req, "pid", 0);
    return req;
}

static void fs_fail(sh_ctx_t* ctx, const char* cmd, const cJSON* result)
{
    sh_out(ctx, "%s: %s\n", cmd, sh_err(result));
}

static void list_tree(sh_ctx_t* ctx, const char* path, int depth)
{
    cJSON* result = sh_call(ctx, fs_request(ctx, "fs_list", path));
    if (!sh_ok(result)) {
        if (depth == 0) {
            fs_fail(ctx, "files", result);
        }
        cJSON_Delete(result);
        return;
    }
    const cJSON* entries = cJSON_GetObjectItemCaseSensitive(result, "entries");
    for (int i = 0; i < cJSON_GetArraySize(entries); i++) {
        const cJSON* e = cJSON_GetArrayItem(entries, i);
        const char* name = cJSON_GetObjectItemCaseSensitive(e, "name")->valuestring;
        const char* type = cJSON_GetObjectItemCaseSensitive(e, "type")->valuestring;
        const cJSON* e_path = cJSON_GetObjectItemCaseSensitive(e, "path");
        sh_out(ctx, "%*s%s %s\n", depth * 2, "", type, name);
        if (strcmp(type, "dir") == 0 && depth < 3 && cJSON_IsString(e_path)) {
            list_tree(ctx, e_path->valuestring, depth + 1);
        }
    }
    cJSON_Delete(result);
}

static int cmd_ls(sh_ctx_t* ctx, int argc, char** argv)
{
    char path[SH_PATH_MAX];
    if (!sh_join_path(path, sizeof(path), ctx->shell->cwd,
                      argc > 1 ? argv[1] : ".")) {
        sh_out(ctx, "ls: path too long\n");
        return 1;
    }
    cJSON* result = sh_call(ctx, fs_request(ctx, "fs_list", path));
    if (!sh_ok(result)) {
        fs_fail(ctx, "ls", result);
    } else {
        sh_out(ctx, "%s:\n", path);
        sh_fmt_entries(ctx, result);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_mkdir(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 2) {
        sh_out(ctx, "usage: mkdir <path>\n");
        return 1;
    }
    char path[SH_PATH_MAX];
    if (!sh_join_path(path, sizeof(path), ctx->shell->cwd, argv[1])) {
        sh_out(ctx, "mkdir: path too long\n");
        return 1;
    }
    cJSON* result = sh_call(ctx, fs_request(ctx, "fs_mkdir", path));
    if (sh_ok(result)) {
        sh_out(ctx, "created directory %s\n", path);
    } else {
        fs_fail(ctx, "mkdir", result);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_touch(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 2) {
        sh_out(ctx, "usage: touch <path>\n");
        return 1;
    }
    char path[SH_PATH_MAX];
    if (!sh_join_path(path, sizeof(path), ctx->shell->cwd, argv[1])) {
        sh_out(ctx, "touch: path too long\n");
        return 1;
    }
    cJSON* req = fs_request(ctx, "fs_create", path);
    cJSON_AddStringToObject(req, "content", "");
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        sh_out(ctx, "created %s\n", path);
    } else {
        fs_fail(ctx, "touch", result);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_cat(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 2) {
        sh_out(ctx, "usage: cat <path>\n");
        return 1;
    }
    char path[SH_PATH_MAX];
    if (!sh_join_path(path, sizeof(path), ctx->shell->cwd, argv[1])) {
        sh_out(ctx, "cat: path too long\n");
        return 1;
    }
    cJSON* result = sh_call(ctx, fs_request(ctx, "fs_read", path));
    if (!sh_ok(result)) {
        fs_fail(ctx, "cat", result);
    } else {
        const cJSON* content = cJSON_GetObjectItemCaseSensitive(result, "content");
        const char* text = cJSON_IsString(content) ? content->valuestring : "";
        sh_out(ctx, "%s%s", text, text[0] != '\0' && text[strlen(text) - 1] == '\n' ? "" : "\n");
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_write_file(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 3) {
        sh_out(ctx, "usage: write-file <path> <text>\n");
        return 1;
    }
    char path[SH_PATH_MAX];
    char text[SH_LINE_MAX];
    if (!sh_join_path(path, sizeof(path), ctx->shell->cwd, argv[1])) {
        sh_out(ctx, "write-file: path too long\n");
        return 1;
    }
    sh_join_argv(text, sizeof(text), argv, 2, argc);
    cJSON* req = fs_request(ctx, "fs_write", path);
    cJSON_AddStringToObject(req, "content", text);
    cJSON* result = sh_call(ctx, req);
    if (!sh_ok(result)) {
        const char* err = sh_err(result);
        if (strcmp(err, "no such file or directory") == 0 ||
            strcmp(err, "not found") == 0) {
            cJSON_Delete(result);
            req = fs_request(ctx, "fs_create", path);
            cJSON_AddStringToObject(req, "content", text);
            result = sh_call(ctx, req);
        }
    }
    if (sh_ok(result)) {
        sh_out(ctx, "wrote %d byte(s) to %s\n", (int)strlen(text), path);
    } else {
        fs_fail(ctx, "write-file", result);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_rm(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 2) {
        sh_out(ctx, "usage: rm <path>\n");
        return 1;
    }
    char path[SH_PATH_MAX];
    if (!sh_join_path(path, sizeof(path), ctx->shell->cwd, argv[1])) {
        sh_out(ctx, "rm: path too long\n");
        return 1;
    }
    cJSON* result = sh_call(ctx, fs_request(ctx, "fs_delete", path));
    if (sh_ok(result)) {
        sh_out(ctx, "deleted %s\n", path);
    } else {
        fs_fail(ctx, "rm", result);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_move_copy(sh_ctx_t* ctx, int argc, char** argv, int copy)
{
    if (argc < 3) {
        sh_out(ctx, "usage: %s <src> <dst>\n", argv[0]);
        return 1;
    }
    char src[SH_PATH_MAX];
    char dst[SH_PATH_MAX];
    if (!sh_join_path(src, sizeof(src), ctx->shell->cwd, argv[1]) ||
        !sh_join_path(dst, sizeof(dst), ctx->shell->cwd, argv[2])) {
        sh_out(ctx, "%s: path too long\n", argv[0]);
        return 1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", copy ? "fs_copy" : "fs_move");
    cJSON_AddStringToObject(req, "src", src);
    cJSON_AddStringToObject(req, "dst", dst);
    cJSON_AddNumberToObject(req, "pid", 0);
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        sh_out(ctx, "%s %s -> %s\n", copy ? "copied" : "moved", src, dst);
    } else {
        fs_fail(ctx, argv[0], result);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_chmod(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 3) {
        sh_out(ctx, "usage: chmod <path> <mode>\n");
        return 1;
    }
    char path[SH_PATH_MAX];
    if (!sh_join_path(path, sizeof(path), ctx->shell->cwd, argv[1])) {
        sh_out(ctx, "chmod: path too long\n");
        return 1;
    }
    cJSON* req = fs_request(ctx, "fs_chmod", path);
    cJSON_AddStringToObject(req, "mode", argv[2]);
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        sh_out(ctx, "mode of %s set to %s\n", path, argv[2]);
    } else {
        fs_fail(ctx, "chmod", result);
    }
    cJSON_Delete(result);
    return 1;
}

int sh_run_fs(sh_ctx_t* ctx, int argc, char** argv)
{
    const char* cmd = argv[0];
    if (strcmp(cmd, "files") == 0) {
        list_tree(ctx, "/", 0);
        return 1;
    }
    if (strcmp(cmd, "ls") == 0) {
        return cmd_ls(ctx, argc, argv);
    }
    if (strcmp(cmd, "mkdir") == 0) {
        return cmd_mkdir(ctx, argc, argv);
    }
    if (strcmp(cmd, "touch") == 0) {
        return cmd_touch(ctx, argc, argv);
    }
    if (strcmp(cmd, "cat") == 0) {
        return cmd_cat(ctx, argc, argv);
    }
    if (strcmp(cmd, "write-file") == 0) {
        return cmd_write_file(ctx, argc, argv);
    }
    if (strcmp(cmd, "rm") == 0) {
        return cmd_rm(ctx, argc, argv);
    }
    if (strcmp(cmd, "mv") == 0) {
        return cmd_move_copy(ctx, argc, argv, 0);
    }
    if (strcmp(cmd, "cp") == 0) {
        return cmd_move_copy(ctx, argc, argv, 1);
    }
    if (strcmp(cmd, "chmod") == 0) {
        return cmd_chmod(ctx, argc, argv);
    }
    return 0;
}
