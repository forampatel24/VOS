/*
 * kernel/device/drivers/keyboard.c
 *
 * JARVIS OS — keyboard driver with an input buffer.
 *
 * Injected keys land in the buffer first (no lost keystrokes); reads
 * drain the buffer and report how many bytes were delivered.
 */

#include <stddef.h>
#include <string.h>

#include "../../deps/cJSON/cJSON.h"
#include "drivers.h"

#define KB_BUFFER 160

typedef struct {
    char buffer[KB_BUFFER];
    int  len;
} kb_impl_t;

static kb_impl_t g_kb;

static int kb_init(jvk_device_t* dev)
{
    (void)dev;
    memset(&g_kb, 0, sizeof(g_kb));
    return 1;
}

static void kb_shutdown(jvk_device_t* dev)
{
    (void)dev;
    g_kb.len = 0;
    g_kb.buffer[0] = '\0';
}

static int kb_read(jvk_device_t* dev, char* out, int cap)
{
    (void)dev;
    if (out == NULL || cap <= 0) {
        return 0;
    }
    int n = g_kb.len;
    if (n > cap - 1) {
        n = cap - 1;
    }
    memcpy(out, g_kb.buffer, (size_t)n);
    out[n] = '\0';
    g_kb.len = 0;
    g_kb.buffer[0] = '\0';
    return n;
}

static int kb_write(jvk_device_t* dev, const char* data, int len)
{
    (void)dev;
    if (data == NULL || len <= 0) {
        return 0;
    }
    int space = KB_BUFFER - 1 - g_kb.len;
    if (len > space) {
        len = space;
    }
    if (len <= 0) {
        return 0;
    }
    memcpy(g_kb.buffer + g_kb.len, data, (size_t)len);
    g_kb.len += len;
    g_kb.buffer[g_kb.len] = '\0';
    return len;
}

static void kb_interrupt(jvk_device_t* dev)
{
    (void)dev;
}

static void kb_error(jvk_device_t* dev, const char* message)
{
    (void)dev;
    (void)message;
}

static void kb_snapshot(const jvk_device_t* dev, cJSON* out)
{
    (void)dev;
    cJSON_AddStringToObject(out, "buffer", g_kb.buffer);
    cJSON_AddNumberToObject(out, "buffer_len", g_kb.len);
}

static const jvk_driver_ops_t g_kb_ops = {
    kb_init, kb_shutdown, kb_read, kb_write, kb_interrupt, kb_error, kb_snapshot,
    NULL,
};

const jvk_driver_ops_t* jvk_driver_keyboard_ops(void)
{
    return &g_kb_ops;
}

void* jvk_driver_keyboard_impl(void)
{
    return &g_kb;
}
