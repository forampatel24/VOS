/*
 * kernel/device/device_manager.c
 *
 * JARVIS OS — device table, driver dispatch and IRQ delivery.
 */

#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "device_manager.h"

static void dm_event(jvk_device_manager_t* dm, const char* fmt, ...)
{
    if (dm == NULL || dm->on_event == NULL || fmt == NULL) {
        return;
    }
    char msg[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    dm->on_event(dm->event_user, msg);
}

int dm_init(jvk_device_manager_t* dm)
{
    if (dm == NULL) {
        return 0;
    }
    memset(dm, 0, sizeof(*dm));
    return 1;
}

void dm_shutdown(jvk_device_manager_t* dm)
{
    if (dm == NULL) {
        return;
    }
    for (int i = 0; i < dm->count; i++) {
        if (dm->devices[i].ops != NULL && dm->devices[i].ops->shutdown) {
            dm->devices[i].ops->shutdown(&dm->devices[i]);
        }
    }
    dm->count = 0;
}

void dm_set_observer(jvk_device_manager_t* dm, dm_event_fn fn, void* user)
{
    if (dm != NULL) {
        dm->on_event   = fn;
        dm->event_user = user;
    }
}

int dm_register(jvk_device_manager_t* dm, jvk_device_id_t id,
                const char* name, const char* driver, int irq,
                const jvk_driver_ops_t* ops, void* impl)
{
    if (dm == NULL || dm->count >= JVK_DM_MAX_DEVICES) {
        return 0;
    }
    jvk_device_t* dev = &dm->devices[dm->count++];
    memset(dev, 0, sizeof(*dev));
    dev->id       = id;
    dev->state    = JVK_DEV_CONNECTED;
    dev->irq      = irq;
    dev->power_on = 1;
    dev->ops      = ops;
    dev->impl     = impl;
    snprintf(dev->name, sizeof(dev->name), "%s", name != NULL ? name : "?");
    snprintf(dev->driver, sizeof(dev->driver), "%s",
             driver != NULL ? driver : "none");
    if (ops != NULL && ops->init != NULL) {
        ops->init(dev);
    }
    return 1;
}

jvk_device_t* dm_get(jvk_device_manager_t* dm, jvk_device_id_t id)
{
    if (dm == NULL) {
        return NULL;
    }
    for (int i = 0; i < dm->count; i++) {
        if (dm->devices[i].id == id) {
            return &dm->devices[i];
        }
    }
    return NULL;
}

jvk_device_t* dm_find(jvk_device_manager_t* dm, const char* name)
{
    if (dm == NULL || name == NULL) {
        return NULL;
    }
    jvk_device_id_t id;
    if (jvk_device_parse(name, &id)) {
        return dm_get(dm, id);
    }
    for (int i = 0; i < dm->count; i++) {
        jvk_device_t* dev = &dm->devices[i];
        if (strcmp(dev->name, name) == 0 || strcmp(dev->driver, name) == 0) {
            return dev;
        }
    }
    return NULL;
}

int dm_set_state(jvk_device_manager_t* dm, jvk_device_id_t id,
                 jvk_device_state_t state)
{
    jvk_device_t* dev = dm_get(dm, id);
    if (dev == NULL) {
        return 0;
    }
    dev->state = state;
    dm_event(dm, "DEVICE_STATE device=%s state=%s",
             jvk_device_key(dev->id), jvk_device_state_name(state));
    return 1;
}

int dm_set_power(jvk_device_manager_t* dm, jvk_device_id_t id, int on)
{
    jvk_device_t* dev = dm_get(dm, id);
    if (dev == NULL) {
        return 0;
    }
    dev->power_on = on ? 1 : 0;
    if (!dev->power_on) {
        dev->state = JVK_DEV_DISABLED;
    } else if (dev->state == JVK_DEV_DISABLED) {
        dev->state = JVK_DEV_CONNECTED;
    }
    dm_event(dm, "DEVICE_POWER device=%s power=%s",
             jvk_device_key(dev->id), dev->power_on ? "on" : "off");
    return 1;
}

static int device_usable(const jvk_device_t* dev, int for_write)
{
    (void)for_write;
    if (dev == NULL || dev->ops == NULL) {
        return 0;
    }
    if (!dev->power_on) {
        return 0;
    }
    if (dev->state == JVK_DEV_DISABLED || dev->state == JVK_DEV_DISCONNECTED ||
        dev->state == JVK_DEV_FAULT) {
        return 0;
    }
    return 1;
}

int dm_read(jvk_device_manager_t* dm, jvk_device_id_t id, char* out, int cap)
{
    jvk_device_t* dev = dm_get(dm, id);
    if (!device_usable(dev, 0) || dev->ops->read == NULL || out == NULL) {
        return -1;
    }
    int n = dev->ops->read(dev, out, cap);
    if (n < 0) {
        return -1;
    }
    dev->reads++;
    dev->bytes_in += (unsigned long)n;
    dm->reads++;
    dm_event(dm, "DEVICE_READ device=%s bytes=%d", jvk_device_key(dev->id), n);
    return n;
}

int dm_write(jvk_device_manager_t* dm, jvk_device_id_t id,
             const char* data, int len)
{
    jvk_device_t* dev = dm_get(dm, id);
    if (!device_usable(dev, 1) || dev->ops->write == NULL || data == NULL) {
        return -1;
    }
    int n = dev->ops->write(dev, data, len);
    if (n < 0) {
        dev->errors++;
        return -1;
    }
    dev->writes++;
    dev->bytes_out += (unsigned long)n;
    dm->writes++;
    dm_event(dm, "DEVICE_WRITE device=%s bytes=%d", jvk_device_key(dev->id), n);
    return n;
}

int dm_raise_interrupt(jvk_device_manager_t* dm, jvk_device_id_t id)
{
    jvk_device_t* dev = dm_get(dm, id);
    if (dev == NULL || !dev->power_on) {
        return 0;
    }
    dev->irq_count++;
    dm->interrupts++;
    if (dev->ops != NULL && dev->ops->handle_interrupt != NULL) {
        dev->ops->handle_interrupt(dev);
    }
    dm_event(dm, "DEVICE_IRQ device=%s irq=%d", jvk_device_key(dev->id),
             dev->irq);
    return 1;
}

int dm_handle_irq(jvk_device_manager_t* dm, int irq)
{
    if (dm == NULL) {
        return 0;
    }
    int handled = 0;
    for (int i = 0; i < dm->count; i++) {
        jvk_device_t* dev = &dm->devices[i];
        if (dev->irq != irq || !dev->power_on) {
            continue;
        }
        dev->irq_count++;
        dm->interrupts++;
        if (dev->ops != NULL && dev->ops->handle_interrupt != NULL) {
            dev->ops->handle_interrupt(dev);
        }
        handled++;
    }
    return handled;
}

void dm_error(jvk_device_manager_t* dm, jvk_device_id_t id,
              const char* message)
{
    jvk_device_t* dev = dm_get(dm, id);
    if (dev == NULL) {
        return;
    }
    dev->errors++;
    dm->errors++;
    dev->state = JVK_DEV_FAULT;
    if (dev->ops != NULL && dev->ops->report_error != NULL) {
        dev->ops->report_error(dev, message);
    }
    dm_event(dm, "DEVICE_ERROR device=%s msg=%s", jvk_device_key(dev->id),
             message != NULL ? message : "unknown");
}
