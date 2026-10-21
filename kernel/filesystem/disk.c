/*
 * kernel/filesystem/disk.c
 *
 * JARVIS OS — RAM-backed virtual disk with a block bitmap.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"

int disk_init(jvk_disk_t* disk, int blocks, int block_size)
{
    if (disk == NULL) {
        return 0;
    }
    memset(disk, 0, sizeof(*disk));

    if (blocks <= 0 || blocks > JVK_DISK_MAX_BLOCKS) {
        blocks = JVK_DISK_DEFAULT_BLOCKS;
    }
    if (block_size < JVK_DISK_MIN_BLOCK_SIZE ||
        block_size > JVK_DISK_DEFAULT_BLOCK_SIZE * 4) {
        block_size = JVK_DISK_DEFAULT_BLOCK_SIZE;
    }

    disk->data = (unsigned char*)calloc((size_t)blocks * (size_t)block_size, 1);
    disk->used = (unsigned char*)calloc((size_t)blocks, 1);
    if (disk->data == NULL || disk->used == NULL) {
        free(disk->data);
        free(disk->used);
        disk->data = NULL;
        disk->used = NULL;
        return 0;
    }
    disk->total_blocks = blocks;
    disk->block_size   = block_size;
    return 1;
}

void disk_shutdown(jvk_disk_t* disk)
{
    if (disk == NULL) {
        return;
    }
    free(disk->data);
    free(disk->used);
    disk->data = NULL;
    disk->used = NULL;
    disk->total_blocks = 0;
    disk->block_size   = 0;
}

int disk_alloc(jvk_disk_t* disk, int count, int* out_blocks)
{
    if (disk == NULL || count <= 0 || out_blocks == NULL || disk->used == NULL) {
        return -1;
    }
    disk->alloc_calls++;

    int run = 0;
    for (int i = 0; i < disk->total_blocks; i++) {
        if (disk->used[i] == 0) {
            run++;
            if (run == count) {
                int start = i - count + 1;
                for (int k = 0; k < count; k++) {
                    disk->used[start + k] = 1;
                    out_blocks[k] = start + k;
                }
                return 0;
            }
        } else {
            run = 0;
        }
    }
    return -1;
}

void disk_free(jvk_disk_t* disk, const int* blocks, int count)
{
    if (disk == NULL || blocks == NULL || disk->used == NULL) {
        return;
    }
    disk->free_calls++;
    for (int i = 0; i < count; i++) {
        int b = blocks[i];
        if (b >= 0 && b < disk->total_blocks) {
            disk->used[b] = 0;
        }
    }
}

int disk_write(jvk_disk_t* disk, const int* blocks, int count,
               const char* data, int len)
{
    if (disk == NULL || blocks == NULL || count <= 0 || disk->data == NULL) {
        return 0;
    }
    int capacity = count * disk->block_size;
    if (len < 0) {
        len = 0;
    }
    if (len > capacity) {
        len = capacity;
    }

    for (int i = 0; i < count; i++) {
        int b = blocks[i];
        if (b < 0 || b >= disk->total_blocks) {
            return 0;
        }
        memset(disk->data + (size_t)b * (size_t)disk->block_size, 0,
               (size_t)disk->block_size);
    }
    if (data != NULL && len > 0) {
        int written = 0;
        for (int i = 0; i < count && written < len; i++) {
            int b = blocks[i];
            int chunk = len - written;
            if (chunk > disk->block_size) {
                chunk = disk->block_size;
            }
            memcpy(disk->data + (size_t)b * (size_t)disk->block_size,
                   data + written, (size_t)chunk);
            written += chunk;
        }
    }
    disk->write_ops++;
    return len;
}

int disk_read(const jvk_disk_t* disk, const int* blocks, int count,
              char* out, int cap, int len)
{
    if (disk == NULL || blocks == NULL || count <= 0 || out == NULL ||
        cap <= 0 || disk->data == NULL) {
        return 0;
    }
    if (len < 0) {
        len = 0;
    }
    int capacity = count * disk->block_size;
    if (len > capacity) {
        len = capacity;
    }
    if (len > cap - 1) {
        len = cap - 1;
    }

    int copied = 0;
    for (int i = 0; i < count && copied < len; i++) {
        int b = blocks[i];
        if (b < 0 || b >= disk->total_blocks) {
            break;
        }
        int chunk = len - copied;
        if (chunk > disk->block_size) {
            chunk = disk->block_size;
        }
        memcpy(out + copied,
               disk->data + (size_t)b * (size_t)disk->block_size,
               (size_t)chunk);
        copied += chunk;
    }
    out[copied] = '\0';
    return copied;
}

int disk_used_blocks(const jvk_disk_t* disk)
{
    if (disk == NULL || disk->used == NULL) {
        return 0;
    }
    int used = 0;
    for (int i = 0; i < disk->total_blocks; i++) {
        used += disk->used[i] ? 1 : 0;
    }
    return used;
}

int disk_free_blocks(const jvk_disk_t* disk)
{
    if (disk == NULL) {
        return 0;
    }
    return disk->total_blocks - disk_used_blocks(disk);
}

int disk_largest_free_run(const jvk_disk_t* disk)
{
    if (disk == NULL || disk->used == NULL) {
        return 0;
    }
    int best = 0;
    int run = 0;
    for (int i = 0; i < disk->total_blocks; i++) {
        if (disk->used[i] == 0) {
            run++;
            if (run > best) {
                best = run;
            }
        } else {
            run = 0;
        }
    }
    return best;
}
