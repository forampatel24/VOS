/*
 * kernel/device/drivers/mouse.c
 *
 * JARVIS OS — mouse driver holding a virtual pointer position.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../../deps/cJSON/cJSON.h"
#include "drivers.h"

typedef struct {
    int x;
    int y;
} mouse_impl_t;

static mouse_impl_t g_mouse;

static int mouse_init(jvk_device_t* dev)
{
    (void)dev;
    g_mouse.x = 0;
    g_mouse.y = 0;
    return 1;
}

static void mouse_shutdown(jvk_device_t* dev)
{
    (void)dev;
    g_mouse.x = 0;
    g_mouse.y = 0;
}

static int mouse_read(jvk_device_t* dev, char* out, int cap)
{
    (void)dev;
    if (out == NULL || cap <= 0) {
        return 0;
    }
    return snprintf(out, (size_t)cap, "%d,%d", g_mouse.x, g_mouse.y);
}

static int mouse_write(jvk_device_t* dev, const char* data, int len)
{
    (void)dev;
    if (data == NULL || len <= 0) {
        return 0;
    }
    int x = 0;
    int y = 0;
    if (sscanf(data, "%d,%d", &x, &y) == 2) {
        g_mouse.x = x;
        g_mouse.y = y;
        return len;
    }
    if (strncmp(data, "left", 4) == 0) {
        g_mouse.x--;
    } else if (strncmp(data, "right", 5) == 0) {
        g_mouse.x++;
    } else if (strncmp(data, "up", 2) == 0) {
        g_mouse.y--;
    } else if (strncmp(data, "down", 4) == 0) {
        g_mouse.y++;
    } else {
        return 0;
    }
    return len;
}

static void mouse_interrupt(jvk_device_t* dev)
{
    (void)dev;
}

static void mouse_error(jvk_device_t* dev, const char* message)
{
    (void)dev;
    (void)message;
}

static void mouse_snapshot(const jvk_device_t* dev, cJSON* out)
{
    (void)dev;
    cJSON_AddNumberToObject(out, "x", g_mouse.x);
    cJSON_AddNumberToObject(out, "y", g_mouse.y);
}

static const jvk_driver_ops_t g_mouse_ops = {
    mouse_init, mouse_shutdown, mouse_read, mouse_write,
    mouse_interrupt, mouse_error, mouse_snapshot,
};

const jvk_driver_ops_t* jvk_driver_mouse_ops(void)
{
    return &g_mouse_ops;
}

void* jvk_driver_mouse_impl(void)
{
    return &g_mouse;
}
