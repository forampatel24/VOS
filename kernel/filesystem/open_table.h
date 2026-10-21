/*
 * kernel/filesystem/open_table.h
 *
 * JARVIS OS — per-system open file table.
 *
 * Every open file has an entry: descriptor, inode, owner process,
 * cursor and access mode. The kernel owns the table; processes only
 * receive the descriptor number.
 */

#ifndef JARVIS_OPEN_TABLE_H
#define JARVIS_OPEN_TABLE_H

#include "fs_types.h"

typedef struct {
    int           used;
    int           fd;
    int           inode_id;
    int           owner;
    int           cursor;
    char          mode[4]; /* "r" | "w" | "rw" */
    unsigned long opened_ticks;
} jvk_open_file_t;

typedef struct {
    jvk_open_file_t entries[JVK_FS_MAX_OPEN];
    int             count;
    int             next_fd;
} jvk_open_table_t;

void ot_init(jvk_open_table_t* ot);

/* Returns a fresh descriptor (>= 3), or -1 when the table is full. */
int  ot_open(jvk_open_table_t* ot, int inode_id, int owner,
             const char* mode, unsigned long ticks);
/* Returns the entry index, or -1 when fd is closed. */
int  ot_find(jvk_open_table_t* ot, int fd);
int  ot_close(jvk_open_table_t* ot, int fd);
int  ot_close_inode(jvk_open_table_t* ot, int inode_id);
int  ot_close_owner(jvk_open_table_t* ot, int pid);

#endif /* JARVIS_OPEN_TABLE_H */
