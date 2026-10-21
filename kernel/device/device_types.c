/*
 * kernel/device/device_types.c
 *
 * JARVIS OS — device names and state strings.
 */

#include <stddef.h>
#include <string.h>

#include "device_types.h"

static const char* const k_names[JVK_DEV_COUNT] = {
    "CPU", "RAM", "Disk", "Keyboard", "Mouse", "Display", "Printer", "Clock",
};

static const char* const k_keys[JVK_DEV_COUNT] = {
    "cpu", "ram", "disk", "keyboard", "mouse", "display", "printer", "clock",
};

static const char* const k_states[] = {
    "CONNECTED", "DISCONNECTED", "BUSY", "WAITING", "FAULT", "DISABLED",
};

const char* jvk_device_name(jvk_device_id_t id)
{
    if (id < 0 || id >= JVK_DEV_COUNT) {
        return "Unknown";
    }
    return k_names[id];
}

const char* jvk_device_key(jvk_device_id_t id)
{
    if (id < 0 || id >= JVK_DEV_COUNT) {
        return "unknown";
    }
    return k_keys[id];
}

const char* jvk_device_state_name(jvk_device_state_t state)
{
    if (state < 0 || state > JVK_DEV_DISABLED) {
        return "UNKNOWN";
    }
    return k_states[state];
}

/* Case-insensitive compare so JSON/UI may send "busy" or "BUSY". */
static int state_eq(const char* a, const char* b)
{
    while (*a != '\0' && *b != '\0') {
        char ca = *a >= 'A' && *a <= 'Z' ? (char)(*a - 'A' + 'a') : *a;
        char cb = *b >= 'A' && *b <= 'Z' ? (char)(*b - 'A' + 'a') : *b;
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

int jvk_device_state_parse(const char* name, jvk_device_state_t* out)
{
    if (name == NULL || out == NULL) {
        return 0;
    }
    for (int i = 0; i <= JVK_DEV_DISABLED; i++) {
        if (state_eq(name, k_states[i])) {
            *out = (jvk_device_state_t)i;
            return 1;
        }
    }
    return 0;
}

int jvk_device_parse(const char* name, jvk_device_id_t* out)
{
    if (name == NULL || out == NULL) {
        return 0;
    }
    for (int i = 0; i < JVK_DEV_COUNT; i++) {
        if (state_eq(name, k_keys[i]) || state_eq(name, k_names[i])) {
            *out = (jvk_device_id_t)i;
            return 1;
        }
    }
    return 0;
}
