/*
 * kernel/device/drivers/clock_driver.c
 *
 * JARVIS OS — programmable clock device.
 *
 * Timer interrupts advance the hardware tick counter; the kernel's own
 * virtual clock is a separate authority (cpu/clock.c).
 */

#include <stddef.h>
#include <stdio.h>

#include "../../deps/cJSON/cJSON.h"
#include "drivers.h"

typedef struct {
    unsigned long ticks;
} clock_impl_t;

static clock_impl_t g_clock;

static int clock_dev_init(jvk_device_t* dev)
{
    (void)dev;
    g_clock.ticks = 0;
    return 1;
}

static void clock_dev_shutdown(jvk_device_t* dev)
{
    (void)dev;
    g_clock.ticks = 0;
}

static int clock_dev_read(jvk_device_t* dev, char* out, int cap)
{
    (void)dev;
    if (out == NULL || cap <= 0) {
        return 0;
    }
    return snprintf(out, (size_t)cap, "%lu", g_clock.ticks);
}

static int clock_dev_write(jvk_device_t* dev, const char* data, int len)
{
    (void)dev;
    (void)data;
    return len > 0 ? len : 0;
}

static void clock_dev_interrupt(jvk_device_t* dev)
{
    (void)dev;
    g_clock.ticks++;
}

static void clock_dev_error(jvk_device_t* dev, const char* message)
{
    (void)dev;
    (void)message;
}

static void clock_dev_snapshot(const jvk_device_t* dev, cJSON* out)
{
    (void)dev;
    cJSON_AddNumberToObject(out, "ticks", (double)g_clock.ticks);
    cJSON_AddStringToObject(out, "mode", "periodic");
}

static const jvk_driver_ops_t g_clock_ops = {
    clock_dev_init, clock_dev_shutdown, clock_dev_read, clock_dev_write,
    clock_dev_interrupt, clock_dev_error, clock_dev_snapshot,
};

const jvk_driver_ops_t* jvk_driver_clock_ops(void)
{
    return &g_clock_ops;
}

void* jvk_driver_clock_impl(void)
{
    return &g_clock;
}
