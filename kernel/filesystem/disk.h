/*
 * kernel/filesystem/disk.h
 *
 * JARVIS OS — virtual disk (RAM-backed block device).
 *
 * The disk is the only storage authority below the VFS. It owns a block
 * bitmap and the block bytes. Allocation is contiguous (first run that
 * fits), matching the v1 design; the largest free run is reported as the
 * fragmentation metric instead of a fake percentage.
 */

#ifndef JARVIS_DISK_H
#define JARVIS_DISK_H

#define JVK_DISK_DEFAULT_BLOCKS      1024
#define JVK_DISK_DEFAULT_BLOCK_SIZE  4096
#define JVK_DISK_MIN_BLOCK_SIZE      64
#define JVK_DISK_MAX_BLOCKS          4096

typedef struct {
    int            total_blocks;
    int            block_size;
    unsigned char* data; /* total_blocks * block_size bytes, heap */
    unsigned char* used; /* one byte per block: 0 free, 1 used */
    long           read_ops;
    long           write_ops;
    long           alloc_calls;
    long           free_calls;
} jvk_disk_t;

int  disk_init(jvk_disk_t* disk, int blocks, int block_size);
void disk_shutdown(jvk_disk_t* disk);

/* Contiguous placement. Returns 0 and fills out_blocks, or -1 if no run. */
int  disk_alloc(jvk_disk_t* disk, int count, int* out_blocks);
void disk_free(jvk_disk_t* disk, const int* blocks, int count);

/* Write at most count*block_size bytes; the rest of the run is zeroed. */
int  disk_write(jvk_disk_t* disk, const int* blocks, int count,
                const char* data, int len);
/* Copy `len` bytes out (bounded by count*block_size and cap-1). */
int  disk_read(const jvk_disk_t* disk, const int* blocks, int count,
               char* out, int cap, int len);

int  disk_used_blocks(const jvk_disk_t* disk);
int  disk_free_blocks(const jvk_disk_t* disk);
int  disk_largest_free_run(const jvk_disk_t* disk);

#endif /* JARVIS_DISK_H */
