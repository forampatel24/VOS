/*
 * kernel/filesystem/fs_types.h
 *
 * JARVIS OS — shared virtual-filesystem types.
 *
 * The filesystem is completely virtual: inodes, directory entries and
 * file data live in RAM blocks owned by the kernel. No host path is ever
 * opened or stat()ed. Storage is contiguous block allocation inside the
 * virtual disk (version 1, per design doc §79).
 */

#ifndef JARVIS_FS_TYPES_H
#define JARVIS_FS_TYPES_H

#define JVK_FS_MAX_INODES      128 /* inode table capacity */
#define JVK_FS_NAME_LEN        32
#define JVK_FS_PATH_LEN        160
#define JVK_FS_MAX_BLOCKS      8   /* contiguous blocks per file */
#define JVK_FS_MAX_OPEN        64  /* file descriptor table capacity */
#define JVK_FS_MODE_LEN        12

typedef enum {
    JVK_FS_FILE = 0,
    JVK_FS_DIR  = 1,
} jvk_fs_type_t;

typedef enum {
    JVK_FS_OK = 0,
    JVK_FS_ERR_NOENT,
    JVK_FS_ERR_EXIST,
    JVK_FS_ERR_NOTDIR,
    JVK_FS_ERR_ISDIR,
    JVK_FS_ERR_NOTEMPTY,
    JVK_FS_ERR_NOSPC,
    JVK_FS_ERR_PERM,
    JVK_FS_ERR_FULL,
    JVK_FS_ERR_INVAL,
    JVK_FS_ERR_READONLY,
    JVK_FS_ERR_FD,
    JVK_FS_ERR_COUNT,
} jvk_fs_status_t;

/* permission bits (UNIX-style, group bits unused) */
#define JVK_FS_R 4
#define JVK_FS_W 2
#define JVK_FS_X 1

const char* jvk_fs_status_name(jvk_fs_status_t st);
const char* jvk_fs_type_name(jvk_fs_type_t type);

/* "rwxr-xr-x" <-> bit mask; also accepts octal ("644" / "0755"). */
void jvk_fs_mode_string(unsigned int mode, char* out, int cap);
int  jvk_fs_mode_parse(const char* text, unsigned int* out_mode);

#endif /* JARVIS_FS_TYPES_H */
