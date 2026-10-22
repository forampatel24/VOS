/*
 * kernel/device/drivers/disk_driver.c
 *
 * JARVIS OS — disk device driver.
 *
 * The block storage itself belongs to the VFS; this driver models the
 * hardware side: a small write-back buffer (the disk caches incoming
 * block writes and commits them in a batch), transfer counters and
 * completion interrupts.
 */

#include <stddef.h>
#include <stdio.h>

#include "../../deps/cJSON/cJSON.h"
#include "drivers.h"

/* Writes are cached until the buffer fills, or until an explicit flush. */
#define DISK_BUFFER_BLOCKS 8

typedef struct {
    unsigned long transfers;  /* committed block writes */
    unsigned long buffered;   /* writes sitting in the write-back buffer */
    unsigned long flushes;    /* flush operations performed */
    unsigned long writes;     /* every write the driver has accepted */
} disk_impl_t;

static disk_impl_t g_disk;

static int disk_flush(jvk_device_t* dev);

static int disk_init(jvk_device_t* dev)
{
    (void)dev;
    g_disk.transfers = 0;
    g_disk.buffered  = 0;
    g_disk.flushes   = 0;
    g_disk.writes    = 0;
    return 1;
}

static void disk_shutdown(jvk_device_t* dev)
{
    (void)dev;
    disk_flush(dev);
    g_disk.transfers = 0;
    g_disk.buffered  = 0;
    g_disk.flushes   = 0;
    g_disk.writes    = 0;
}

static int disk_flush(jvk_device_t* dev)
{
    (void)dev;
    if (g_disk.buffered == 0) {
        return 0;
    }
    unsigned long committed = g_disk.buffered;
    g_disk.transfers += committed;
    g_disk.buffered   = 0;
    g_disk.flushes++;
    return (int)committed;
}

static int disk_read(jvk_device_t* dev, char* out, int cap)
{
    (void)dev;
    if (out == NULL || cap <= 0) {
        return 0;
    }
    return snprintf(out, (size_t)cap,
                    "virtual disk ready (%lu transfers, %lu buffered)",
                    g_disk.transfers, g_disk.buffered);
}

static int disk_write(jvk_device_t* dev, const char* data, int len)
{
    if (data == NULL) {
        return 0;
    }
    if (len < 0) {
        return 0;
    }
    g_disk.writes++;
    g_disk.buffered++;
    if (g_disk.buffered >= DISK_BUFFER_BLOCKS) {
        disk_flush(dev); /* buffer full: hardware commits the batch */
    }
    return len;
}

static void disk_interrupt(jvk_device_t* dev)
{
    (void)dev;
    /* A completion interrupt means the buffered batch reached the platter. */
    if (g_disk.buffered > 0) {
        disk_flush(dev);
    } else {
        g_disk.transfers++;
    }
}

static void disk_error(jvk_device_t* dev, const char* message)
{
    (void)dev;
    (void)message;
}

static void disk_snapshot(const jvk_device_t* dev, cJSON* out)
{
    (void)dev;
    cJSON_AddNumberToObject(out, "transfers", (double)g_disk.transfers);
    cJSON_AddNumberToObject(out, "writes", (double)g_disk.writes);
    cJSON_AddNumberToObject(out, "buffered", (double)g_disk.buffered);
    cJSON_AddNumberToObject(out, "buffer_blocks", DISK_BUFFER_BLOCKS);
    cJSON_AddNumberToObject(out, "flushes", (double)g_disk.flushes);
    cJSON_AddStringToObject(out, "backend", "virtual-blocks");
}

static const jvk_driver_ops_t g_disk_ops = {
    disk_init, disk_shutdown, disk_read, disk_write,
    disk_interrupt, disk_error, disk_snapshot, disk_flush,
};

const jvk_driver_ops_t* jvk_driver_disk_ops(void)
{
    return &g_disk_ops;
}

void* jvk_driver_disk_impl(void)
{
    return &g_disk;
}
