/*
 * kernel/device/drivers/disk_driver.c
 *
 * JARVIS OS — disk device driver.
 *
 * The block storage itself belongs to the VFS; this driver models the
 * hardware side: transfer counters and completion interrupts.
 */

#include <stddef.h>
#include <stdio.h>

#include "../../deps/cJSON/cJSON.h"
#include "drivers.h"

typedef struct {
    unsigned long transfers;
} disk_impl_t;

static disk_impl_t g_disk;

static int disk_init(jvk_device_t* dev)
{
    (void)dev;
    g_disk.transfers = 0;
    return 1;
}

static void disk_shutdown(jvk_device_t* dev)
{
    (void)dev;
    g_disk.transfers = 0;
}

static int disk_read(jvk_device_t* dev, char* out, int cap)
{
    (void)dev;
    if (out == NULL || cap <= 0) {
        return 0;
    }
    return snprintf(out, (size_t)cap, "virtual disk ready (%lu transfers)",
                    g_disk.transfers);
}

static int disk_write(jvk_device_t* dev, const char* data, int len)
{
    (void)dev;
    (void)data;
    if (len < 0) {
        return 0;
    }
    g_disk.transfers++;
    return len;
}

static void disk_interrupt(jvk_device_t* dev)
{
    (void)dev;
    g_disk.transfers++;
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
    cJSON_AddStringToObject(out, "backend", "virtual-blocks");
}

static const jvk_driver_ops_t g_disk_ops = {
    disk_init, disk_shutdown, disk_read, disk_write,
    disk_interrupt, disk_error, disk_snapshot,
};

const jvk_driver_ops_t* jvk_driver_disk_ops(void)
{
    return &g_disk_ops;
}

void* jvk_driver_disk_impl(void)
{
    return &g_disk;
}
