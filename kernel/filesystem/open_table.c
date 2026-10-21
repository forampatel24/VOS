/*
 * kernel/filesystem/open_table.c
 *
 * JARVIS OS — open file descriptor table.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "open_table.h"

void ot_init(jvk_open_table_t* ot)
{
    if (ot == NULL) {
        return;
    }
    memset(ot, 0, sizeof(*ot));
    ot->next_fd = 3; /* 0/1/2 stay reserved for stdio */
}

int ot_open(jvk_open_table_t* ot, int inode_id, int owner,
            const char* mode, unsigned long ticks)
{
    if (ot == NULL) {
        return -1;
    }
    for (int i = 0; i < JVK_FS_MAX_OPEN; i++) {
        if (!ot->entries[i].used) {
            jvk_open_file_t* e = &ot->entries[i];
            e->used         = 1;
            e->fd           = ot->next_fd++;
            e->inode_id     = inode_id;
            e->owner        = owner;
            e->cursor       = 0;
            e->opened_ticks = ticks;
            snprintf(e->mode, sizeof(e->mode), "%s", mode != NULL ? mode : "r");
            ot->count++;
            return e->fd;
        }
    }
    return -1;
}

int ot_find(jvk_open_table_t* ot, int fd)
{
    if (ot == NULL) {
        return -1;
    }
    for (int i = 0; i < JVK_FS_MAX_OPEN; i++) {
        if (ot->entries[i].used && ot->entries[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

int ot_close(jvk_open_table_t* ot, int fd)
{
    int idx = ot_find(ot, fd);
    if (idx < 0) {
        return 0;
    }
    memset(&ot->entries[idx], 0, sizeof(ot->entries[idx]));
    if (ot->count > 0) {
        ot->count--;
    }
    return 1;
}

int ot_close_inode(jvk_open_table_t* ot, int inode_id)
{
    if (ot == NULL) {
        return 0;
    }
    int closed = 0;
    for (int i = 0; i < JVK_FS_MAX_OPEN; i++) {
        if (ot->entries[i].used && ot->entries[i].inode_id == inode_id) {
            memset(&ot->entries[i], 0, sizeof(ot->entries[i]));
            closed++;
        }
    }
    ot->count -= closed;
    if (ot->count < 0) {
        ot->count = 0;
    }
    return closed;
}

int ot_close_owner(jvk_open_table_t* ot, int pid)
{
    if (ot == NULL) {
        return 0;
    }
    int closed = 0;
    for (int i = 0; i < JVK_FS_MAX_OPEN; i++) {
        if (ot->entries[i].used && ot->entries[i].owner == pid) {
            memset(&ot->entries[i], 0, sizeof(ot->entries[i]));
            closed++;
        }
    }
    ot->count -= closed;
    if (ot->count < 0) {
        ot->count = 0;
    }
    return closed;
}
