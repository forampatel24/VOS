/*
 * kernel/filesystem/fs_types.c
 *
 * JARVIS OS — filesystem names and permission-bit helpers.
 */

#include <stdio.h>
#include <string.h>

#include "fs_types.h"

const char* jvk_fs_status_name(jvk_fs_status_t st)
{
    switch (st) {
    case JVK_FS_OK:             return "ok";
    case JVK_FS_ERR_NOENT:      return "no such file or directory";
    case JVK_FS_ERR_EXIST:      return "already exists";
    case JVK_FS_ERR_NOTDIR:     return "not a directory";
    case JVK_FS_ERR_ISDIR:      return "is a directory";
    case JVK_FS_ERR_NOTEMPTY:   return "directory not empty";
    case JVK_FS_ERR_NOSPC:      return "no space left on disk";
    case JVK_FS_ERR_PERM:       return "permission denied";
    case JVK_FS_ERR_FULL:       return "inode table full";
    case JVK_FS_ERR_INVAL:      return "invalid argument";
    case JVK_FS_ERR_READONLY:   return "read-only file";
    case JVK_FS_ERR_FD:         return "bad file descriptor";
    default:                    return "unknown filesystem error";
    }
}

const char* jvk_fs_type_name(jvk_fs_type_t type)
{
    return type == JVK_FS_DIR ? "dir" : "file";
}

void jvk_fs_mode_string(unsigned int mode, char* out, int cap)
{
    if (out == NULL || cap < 10) {
        return;
    }
    const char* rwx = "rwx";
    out[0] = ((mode >> 6) & JVK_FS_R) ? rwx[0] : '-';
    out[1] = ((mode >> 6) & JVK_FS_W) ? rwx[1] : '-';
    out[2] = ((mode >> 6) & JVK_FS_X) ? rwx[2] : '-';
    out[3] = '-';
    out[4] = ((mode >> 3) & JVK_FS_R) ? rwx[0] : '-';
    out[5] = ((mode >> 3) & JVK_FS_W) ? rwx[1] : '-';
    out[6] = ((mode >> 3) & JVK_FS_X) ? rwx[2] : '-';
    out[7] = '-';
    out[8] = (mode & JVK_FS_R) ? rwx[0] : '-';
    out[9] = (mode & JVK_FS_W) ? rwx[1] : '-';
    out[10] = (mode & JVK_FS_X) ? rwx[2] : '-';
    out[11] = '\0';
    if (cap < 12) {
        out[cap - 1] = '\0';
    }
}

int jvk_fs_mode_parse(const char* text, unsigned int* out_mode)
{
    if (text == NULL || out_mode == NULL || text[0] == '\0') {
        return 0;
    }

    if (text[0] >= '0' && text[0] <= '7') {
        unsigned int value = 0;
        for (int i = 0; text[i] != '\0'; i++) {
            if (text[i] < '0' || text[i] > '7') {
                return 0;
            }
            value = value * 8 + (unsigned int)(text[i] - '0');
        }
        *out_mode = value & 0777u;
        return 1;
    }

    if (strlen(text) != 9) {
        return 0;
    }

    unsigned int mode = 0;
    for (int i = 0; i < 9; i++) {
        if (text[i] == '-') {
            continue;
        }
        int shift = 6 - ((i / 3) * 3);
        int bit;
        switch (text[i]) {
        case 'r': bit = JVK_FS_R; break;
        case 'w': bit = JVK_FS_W; break;
        case 'x': bit = JVK_FS_X; break;
        default:  return 0;
        }
        mode |= (unsigned int)(bit << shift);
    }
    *out_mode = mode;
    return 1;
}
