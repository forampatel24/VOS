/*
 * kernel/device/drivers/display.c
 *
 * JARVIS OS — display driver with a line output buffer.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../../deps/cJSON/cJSON.h"
#include "drivers.h"

#define DISP_LINES 10
#define DISP_COLS  72

typedef struct {
    char lines[DISP_LINES][DISP_COLS];
    int  count;
} display_impl_t;

static display_impl_t g_display;

static void display_append(display_impl_t* d, const char* text, int len)
{
    if (len <= 0) {
        return;
    }
    if (d->count == DISP_LINES) {
        memmove(&d->lines[0][0], &d->lines[1][0],
                (size_t)(DISP_LINES - 1) * DISP_COLS);
        d->count--;
    }
    if (len > DISP_COLS - 1) {
        len = DISP_COLS - 1;
    }
    memcpy(d->lines[d->count], text, (size_t)len);
    d->lines[d->count][len] = '\0';
    d->count++;
}

static int display_init(jvk_device_t* dev)
{
    (void)dev;
    memset(&g_display, 0, sizeof(g_display));
    return 1;
}

static void display_shutdown(jvk_device_t* dev)
{
    (void)dev;
    memset(&g_display, 0, sizeof(g_display));
}

static int display_read(jvk_device_t* dev, char* out, int cap)
{
    (void)dev;
    if (out == NULL || cap <= 0) {
        return 0;
    }
    out[0] = '\0';
    int used = 0;
    for (int i = 0; i < g_display.count; i++) {
        int n = snprintf(out + used, (size_t)(cap - used), "%s%s",
                         i == 0 ? "" : "\n", g_display.lines[i]);
        if (n < 0 || used + n >= cap) {
            break;
        }
        used += n;
    }
    return used;
}

static int display_write(jvk_device_t* dev, const char* data, int len)
{
    (void)dev;
    if (data == NULL || len <= 0) {
        return 0;
    }
    int start = 0;
    for (int i = 0; i <= len; i++) {
        if (i == len || data[i] == '\n') {
            display_append(&g_display, data + start, i - start);
            start = i + 1;
        }
    }
    return len;
}

static void display_interrupt(jvk_device_t* dev)
{
    (void)dev;
}

static void display_error(jvk_device_t* dev, const char* message)
{
    (void)dev;
    if (message != NULL) {
        display_append(&g_display, message, (int)strlen(message));
    }
}

static void display_snapshot(const jvk_device_t* dev, cJSON* out)
{
    (void)dev;
    cJSON* lines = cJSON_CreateArray();
    for (int i = 0; i < g_display.count; i++) {
        cJSON_AddItemToArray(lines, cJSON_CreateString(g_display.lines[i]));
    }
    cJSON_AddItemToObject(out, "lines", lines);
    cJSON_AddNumberToObject(out, "line_count", g_display.count);
}

static const jvk_driver_ops_t g_display_ops = {
    display_init, display_shutdown, display_read, display_write,
    display_interrupt, display_error, display_snapshot,
};

const jvk_driver_ops_t* jvk_driver_display_ops(void)
{
    return &g_display_ops;
}

void* jvk_driver_display_impl(void)
{
    return &g_display;
}
