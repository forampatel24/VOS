#include <stddef.h>
/*
 * kernel/memory/frame_table.c
 *
 * JARVIS OS — global physical frame table operations.
 */

#include <string.h>

#include "frame_table.h"

void ft_init(mm_frame_table_t* ft, int total_frames)
{
    if (ft == NULL) {
        return;
    }
    if (total_frames < 1) {
        total_frames = 1;
    }
    if (total_frames > JVK_MM_MAX_FRAMES) {
        total_frames = JVK_MM_MAX_FRAMES;
    }
    memset(ft, 0, sizeof(*ft));
    ft->total = total_frames;
}

int ft_free_count(const mm_frame_table_t* ft)
{
    if (ft == NULL) {
        return 0;
    }
    int free_count = 0;
    for (int i = 0; i < ft->total; i++) {
        if (ft->frames[i].state == MM_FRAME_FREE) {
            free_count++;
        }
    }
    return free_count;
}

mm_frame_t* ft_at(mm_frame_table_t* ft, int frame)
{
    if (ft == NULL || frame < 0 || frame >= ft->total) {
        return NULL;
    }
    return &ft->frames[frame];
}

void ft_mark_used(mm_frame_table_t* ft, int frame, int pid,
                  int vpage, unsigned long seq)
{
    mm_frame_t* f = ft_at(ft, frame);
    if (f == NULL) {
        return;
    }
    f->state      = MM_FRAME_USED;
    f->owner_pid  = pid;
    f->vpage      = vpage;
    f->loaded_seq = seq;
    f->access_seq = seq;
    f->ref_bit    = 1;
}

void ft_mark_free(mm_frame_table_t* ft, int frame)
{
    mm_frame_t* f = ft_at(ft, frame);
    if (f == NULL) {
        return;
    }
    memset(f, 0, sizeof(*f));
}

int ft_scan_free(const mm_frame_table_t* ft, int from)
{
    if (ft == NULL || ft->total <= 0) {
        return -1;
    }
    for (int off = 0; off < ft->total; off++) {
        int i = (from + off) % ft->total;
        if (ft->frames[i].state == MM_FRAME_FREE) {
            return i;
        }
    }
    return -1;
}

int ft_run_length(const mm_frame_table_t* ft, int start)
{
    if (ft == NULL || start < 0 || start >= ft->total) {
        return 0;
    }
    if (ft->frames[start].state != MM_FRAME_FREE) {
        return 0;
    }
    int run = 0;
    while (start + run < ft->total &&
           ft->frames[start + run].state == MM_FRAME_FREE) {
        run++;
    }
    return run;
}
