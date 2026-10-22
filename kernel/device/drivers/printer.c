/*
 * kernel/device/drivers/printer.c
 *
 * JARVIS OS — printer driver with a spool queue.
 *
 * Writes queue jobs; an interrupt prints exactly one job, which is how
 * a real spooler acknowledges completion. The queue is bounded so a
 * flood is rejected instead of overwriting pending work.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../../deps/cJSON/cJSON.h"
#include "drivers.h"

#define SPOOL_CAP 8
#define JOB_LEN   64

typedef struct {
    char jobs[SPOOL_CAP][JOB_LEN];
    int  count;
    int  printed;
} printer_impl_t;

static printer_impl_t g_printer;

static int printer_init(jvk_device_t* dev)
{
    (void)dev;
    memset(&g_printer, 0, sizeof(g_printer));
    return 1;
}

static void printer_shutdown(jvk_device_t* dev)
{
    (void)dev;
    memset(&g_printer, 0, sizeof(g_printer));
}

static void printer_pop(void)
{
    if (g_printer.count <= 0) {
        return;
    }
    for (int i = 1; i < g_printer.count; i++) {
        memcpy(g_printer.jobs[i - 1], g_printer.jobs[i], JOB_LEN);
    }
    g_printer.count--;
    g_printer.printed++;
}

static int printer_read(jvk_device_t* dev, char* out, int cap)
{
    if (out == NULL || cap <= 0 || g_printer.count == 0) {
        if (out != NULL && cap > 0) {
            out[0] = '\0';
        }
        return 0;
    }
    int n = snprintf(out, (size_t)cap, "%s", g_printer.jobs[0]);
    printer_pop();
    if (g_printer.count == 0) {
        dev->state = JVK_DEV_CONNECTED;
    }
    return n;
}

static int printer_write(jvk_device_t* dev, const char* data, int len)
{
    if (data == NULL || len <= 0) {
        return 0;
    }
    if (g_printer.count >= SPOOL_CAP) {
        return -1;
    }
    snprintf(g_printer.jobs[g_printer.count], JOB_LEN, "%.*s",
             len < JOB_LEN - 1 ? len : JOB_LEN - 1, data);
    g_printer.count++;
    dev->state = JVK_DEV_WAITING;
    return len;
}

static void printer_interrupt(jvk_device_t* dev)
{
    printer_pop();
    dev->state = g_printer.count > 0 ? JVK_DEV_WAITING : JVK_DEV_CONNECTED;
}

static void printer_error(jvk_device_t* dev, const char* message)
{
    (void)dev;
    (void)message;
}

static void printer_snapshot(const jvk_device_t* dev, cJSON* out)
{
    (void)dev;
    cJSON* jobs = cJSON_CreateArray();
    for (int i = 0; i < g_printer.count; i++) {
        cJSON_AddItemToArray(jobs, cJSON_CreateString(g_printer.jobs[i]));
    }
    cJSON_AddItemToObject(out, "queue", jobs);
    cJSON_AddNumberToObject(out, "queued", g_printer.count);
    cJSON_AddNumberToObject(out, "printed", g_printer.printed);
}

static const jvk_driver_ops_t g_printer_ops = {
    printer_init, printer_shutdown, printer_read, printer_write,
    printer_interrupt, printer_error, printer_snapshot, NULL,
};

const jvk_driver_ops_t* jvk_driver_printer_ops(void)
{
    return &g_printer_ops;
}

void* jvk_driver_printer_impl(void)
{
    return &g_printer;
}
