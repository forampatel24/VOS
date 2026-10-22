/*
 * kernel/core/kernel_devices.c
 *
 * JARVIS OS — device registry, JSON commands and snapshot.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "kernel_devices.h"
#include "../device/drivers/drivers.h"
#include "../interrupts/interrupt_types.h"

/* ---- registry ---------------------------------------------------------- */

typedef struct {
    jvk_device_id_t          id;
    const char*              name;
    const char*              driver;
    jvk_irq_t                irq;
    const jvk_driver_ops_t*  ops;
    void*                    impl;
} kdev_spec_t;

static const kdev_spec_t KDEV_SPECS[JVK_DEV_COUNT] = {
    { JVK_DEV_CPU,      "CPU",      "core",     JVK_IRQ_SOFTWARE,   NULL, NULL },
    { JVK_DEV_RAM,      "RAM",      "core",     JVK_IRQ_PAGE_FAULT, NULL, NULL },
    { JVK_DEV_DISK,     "Disk",     "disk",     JVK_IRQ_DISK,       NULL, NULL },
    { JVK_DEV_KEYBOARD, "Keyboard", "keyboard", JVK_IRQ_KEYBOARD,   NULL, NULL },
    { JVK_DEV_MOUSE,    "Mouse",    "mouse",    JVK_IRQ_KEYBOARD,   NULL, NULL },
    { JVK_DEV_DISPLAY,  "Display",  "display",  JVK_IRQ_SOFTWARE,   NULL, NULL },
    { JVK_DEV_PRINTER,  "Printer",  "printer",  JVK_IRQ_DISK,       NULL, NULL },
    { JVK_DEV_CLOCK,    "Clock",    "clock",    JVK_IRQ_TIMER,      NULL, NULL },
};

static const jvk_driver_ops_t* spec_ops(jvk_device_id_t id)
{
    switch (id) {
    case JVK_DEV_DISK:     return jvk_driver_disk_ops();
    case JVK_DEV_KEYBOARD: return jvk_driver_keyboard_ops();
    case JVK_DEV_MOUSE:    return jvk_driver_mouse_ops();
    case JVK_DEV_DISPLAY:  return jvk_driver_display_ops();
    case JVK_DEV_PRINTER:  return jvk_driver_printer_ops();
    case JVK_DEV_CLOCK:    return jvk_driver_clock_ops();
    default:               return NULL;
    }
}

static void* spec_impl(jvk_device_id_t id)
{
    switch (id) {
    case JVK_DEV_DISK:     return jvk_driver_disk_impl();
    case JVK_DEV_KEYBOARD: return jvk_driver_keyboard_impl();
    case JVK_DEV_MOUSE:    return jvk_driver_mouse_impl();
    case JVK_DEV_DISPLAY:  return jvk_driver_display_impl();
    case JVK_DEV_PRINTER:  return jvk_driver_printer_impl();
    case JVK_DEV_CLOCK:    return jvk_driver_clock_impl();
    default:               return NULL;
    }
}

/* config/device.json: {"devices": {"keyboard": {"enabled": false,
   "irq": "keyboard"}, ...}} — enables/disables a device and may rewire
   its IRQ line. Unknown keys are ignored, never invented. */
static void apply_device_config(jvk_device_manager_t* dm, const cJSON* cfg)
{
    if (!cJSON_IsObject(cfg)) {
        return;
    }
    for (int i = 0; i < dm->count; i++) {
        jvk_device_t* dev = &dm->devices[i];
        const char* key = jvk_device_key(dev->id);
        const cJSON* item = cJSON_GetObjectItemCaseSensitive(cfg, key);
        if (!cJSON_IsObject(item)) {
            continue;
        }
        const cJSON* irq_item = cJSON_GetObjectItemCaseSensitive(item, "irq");
        jvk_irq_t irq;
        if (cJSON_IsString(irq_item) && jvk_irq_parse(irq_item->valuestring, &irq)) {
            dev->irq = (int)irq;
        }
        const cJSON* enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
        if (cJSON_IsBool(enabled) && !cJSON_IsTrue(enabled)) {
            dm_set_power(dm, dev->id, 0);
            dm_set_state(dm, dev->id, JVK_DEV_DISABLED);
        }
    }
}

int kdev_boot(jvk_device_manager_t* dm, const cJSON* config_root)
{
    if (dm == NULL || !dm_init(dm)) {
        return 0;
    }
    for (int i = 0; i < JVK_DEV_COUNT; i++) {
        const kdev_spec_t* s = &KDEV_SPECS[i];
        (void)dm_register(dm, s->id, s->name, s->driver, s->irq,
                          spec_ops(s->id), spec_impl(s->id));
    }
    if (cJSON_IsObject(config_root)) {
        apply_device_config(dm, cJSON_GetObjectItemCaseSensitive(config_root, "devices"));
    }
    return dm->count == JVK_DEV_COUNT;
}

void kdev_deliver_irq(jvk_device_manager_t* dm, int irq)
{
    (void)dm_handle_irq(dm, irq);
}

/* ---- JSON helpers ------------------------------------------------------ */

static void device_json(const jvk_device_manager_t* dm, const jvk_device_t* dev,
                        cJSON* item)
{
    (void)dm;
    cJSON_AddNumberToObject(item, "id", dev->id);
    cJSON_AddStringToObject(item, "key", jvk_device_key(dev->id));
    cJSON_AddStringToObject(item, "name", dev->name);
    cJSON_AddStringToObject(item, "driver", dev->driver);
    cJSON_AddStringToObject(item, "state",
                            jvk_device_state_name(dev->state));
    cJSON_AddStringToObject(item, "irq",
                            dev->irq >= 0 ? jvk_irq_name((jvk_irq_t)dev->irq)
                                          : "none");
    cJSON_AddBoolToObject(item, "power", dev->power_on != 0);
    cJSON_AddNumberToObject(item, "irq_count", (double)dev->irq_count);
    cJSON_AddNumberToObject(item, "reads", (double)dev->reads);
    cJSON_AddNumberToObject(item, "writes", (double)dev->writes);
    cJSON_AddNumberToObject(item, "errors", (double)dev->errors);
    cJSON_AddNumberToObject(item, "bytes_in", (double)dev->bytes_in);
    cJSON_AddNumberToObject(item, "bytes_out", (double)dev->bytes_out);

    if (dev->ops != NULL && dev->ops->snapshot != NULL) {
        cJSON* info = cJSON_CreateObject();
        dev->ops->snapshot(dev, info);
        cJSON_AddItemToObject(item, "info", info);
    }
}

void kdev_snapshot(const jvk_device_manager_t* dm, cJSON* root)
{
    if (dm == NULL || root == NULL) {
        return;
    }
    cJSON* out = cJSON_CreateObject();
    cJSON_AddNumberToObject(out, "count", dm->count);

    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "interrupts", (double)dm->interrupts);
    cJSON_AddNumberToObject(stats, "errors", (double)dm->errors);
    cJSON_AddNumberToObject(stats, "reads", (double)dm->reads);
    cJSON_AddNumberToObject(stats, "writes", (double)dm->writes);
    cJSON_AddItemToObject(out, "stats", stats);

    cJSON* list = cJSON_CreateArray();
    for (int i = 0; i < dm->count; i++) {
        cJSON* item = cJSON_CreateObject();
        device_json(dm, &dm->devices[i], item);
        cJSON_AddItemToArray(list, item);
    }
    cJSON_AddItemToObject(out, "list", list);

    cJSON_AddItemToObject(root, "devices", out);
}

/* ---- command handlers -------------------------------------------------- */

static jvk_device_t* find_device(jvk_device_manager_t* dm, const cJSON* req)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(req, "device");
    if (cJSON_IsString(item)) {
        return dm_find(dm, item->valuestring);
    }
    if (cJSON_IsNumber(item)) {
        return dm_get(dm, (jvk_device_id_t)(int)item->valuedouble);
    }
    return NULL;
}

static int fail_device(cJSON* result, const char* message)
{
    cJSON_AddBoolToObject(result, "ok", 0);
    cJSON_AddStringToObject(result, "error", message);
    return 1;
}

static int handle_set(jvk_device_manager_t* dm, const cJSON* req, cJSON* result)
{
    jvk_device_t* dev = find_device(dm, req);
    if (dev == NULL) {
        return fail_device(result, "unknown device");
    }
    const cJSON* state_item = cJSON_GetObjectItemCaseSensitive(req, "state");
    if (cJSON_IsString(state_item)) {
        jvk_device_state_t state;
        if (!jvk_device_state_parse(state_item->valuestring, &state)) {
            return fail_device(result, "unknown state");
        }
        dm_set_state(dm, dev->id, state);
    }
    const cJSON* power_item = cJSON_GetObjectItemCaseSensitive(req, "power");
    if (cJSON_IsBool(power_item)) {
        dm_set_power(dm, dev->id, cJSON_IsTrue(power_item) ? 1 : 0);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "device", jvk_device_key(dev->id));
    cJSON_AddStringToObject(result, "state",
                            jvk_device_state_name(dev->state));
    cJSON_AddBoolToObject(result, "power", dev->power_on != 0);
    return 1;
}

static int handle_write(jvk_device_manager_t* dm, const cJSON* req,
                        cJSON* result)
{
    jvk_device_t* dev = find_device(dm, req);
    if (dev == NULL) {
        return fail_device(result, "unknown device");
    }
    const cJSON* data_item = cJSON_GetObjectItemCaseSensitive(req, "data");
    if (!cJSON_IsString(data_item)) {
        return fail_device(result, "data must be a string");
    }
    int n = dm_write(dm, dev->id, data_item->valuestring,
                     (int)strlen(data_item->valuestring));
    if (n < 0) {
        return fail_device(result, "device unavailable or rejected request");
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "device", jvk_device_key(dev->id));
    cJSON_AddNumberToObject(result, "bytes", n);
    cJSON_AddStringToObject(result, "state",
                            jvk_device_state_name(dev->state));
    return 1;
}

static int handle_read(jvk_device_manager_t* dm, const cJSON* req,
                       cJSON* result)
{
    jvk_device_t* dev = find_device(dm, req);
    if (dev == NULL) {
        return fail_device(result, "unknown device");
    }
    char buf[256];
    int n = dm_read(dm, dev->id, buf, (int)sizeof(buf));
    if (n < 0) {
        return fail_device(result, "device unavailable or has no read path");
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "device", jvk_device_key(dev->id));
    cJSON_AddStringToObject(result, "data", buf);
    cJSON_AddNumberToObject(result, "bytes", n);
    return 1;
}

static int handle_flush(jvk_device_manager_t* dm, const cJSON* req,
                        cJSON* result)
{
    jvk_device_t* dev = find_device(dm, req);
    if (dev == NULL) {
        return fail_device(result, "unknown device");
    }
    int n = dm_flush(dm, dev->id);
    if (n < 0) {
        return fail_device(result, "device has no write buffer");
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "device", jvk_device_key(dev->id));
    cJSON_AddNumberToObject(result, "flushed", n);
    return 1;
}

static int handle_interrupt(jvk_device_manager_t* dm, const cJSON* req,
                            cJSON* result, jvk_ic_t* ic, unsigned long ticks)
{
    jvk_device_t* dev = find_device(dm, req);
    if (dev == NULL) {
        return fail_device(result, "unknown device");
    }
    if (!dm_raise_interrupt(dm, dev->id)) {
        return fail_device(result, "device cannot raise interrupts");
    }
    if (ic != NULL && dev->irq >= 0) {
        char detail[64];
        snprintf(detail, sizeof(detail), "%s raised", jvk_device_key(dev->id));
        ic_enqueue(ic, (jvk_irq_t)dev->irq, dev->name, detail, ticks);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "device", jvk_device_key(dev->id));
    cJSON_AddStringToObject(result, "irq",
                            dev->irq >= 0 ? jvk_irq_name((jvk_irq_t)dev->irq)
                                          : "none");
    cJSON_AddNumberToObject(result, "irq_count", (double)dev->irq_count);
    return 1;
}

static int handle_fault(jvk_device_manager_t* dm, const cJSON* req,
                        cJSON* result, jvk_error_manager_t* em,
                        jvk_ic_t* ic)
{
    jvk_device_t* dev = find_device(dm, req);
    if (dev == NULL) {
        return fail_device(result, "unknown device");
    }
    const cJSON* reason_item = cJSON_GetObjectItemCaseSensitive(req, "reason");
    const char* reason = cJSON_IsString(reason_item)
                             ? reason_item->valuestring : "simulated fault";
    dm_error(dm, dev->id, reason);
    if (em != NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s: %s", jvk_device_key(dev->id), reason);
        em_record(em, JVK_ERR_DEVICE, msg, ic);
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "device", jvk_device_key(dev->id));
    cJSON_AddStringToObject(result, "state",
                            jvk_device_state_name(dev->state));
    return 1;
}

static int handle_repair(jvk_device_manager_t* dm, const cJSON* req,
                         cJSON* result)
{
    jvk_device_t* dev = find_device(dm, req);
    if (dev == NULL) {
        return fail_device(result, "unknown device");
    }
    dm_set_power(dm, dev->id, 1);
    dm_set_state(dm, dev->id, JVK_DEV_CONNECTED);
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddStringToObject(result, "device", jvk_device_key(dev->id));
    cJSON_AddStringToObject(result, "state", "CONNECTED");
    return 1;
}

int kdev_handle(jvk_device_manager_t* dm, const char* action,
                const cJSON* req, cJSON* result,
                jvk_ic_t* ic, jvk_error_manager_t* em,
                unsigned long ticks)
{
    if (dm == NULL || action == NULL || req == NULL || result == NULL) {
        return 0;
    }
    if (strcmp(action, "list_devices") == 0) {
        kdev_snapshot(dm, result);
        cJSON_AddBoolToObject(result, "ok", 1);
        return 1;
    }
    if (strcmp(action, "device_set") == 0) {
        return handle_set(dm, req, result);
    }
    if (strcmp(action, "device_write") == 0) {
        return handle_write(dm, req, result);
    }
    if (strcmp(action, "device_read") == 0) {
        return handle_read(dm, req, result);
    }
    if (strcmp(action, "device_flush") == 0) {
        return handle_flush(dm, req, result);
    }
    if (strcmp(action, "device_interrupt") == 0) {
        return handle_interrupt(dm, req, result, ic, ticks);
    }
    if (strcmp(action, "device_fault") == 0) {
        return handle_fault(dm, req, result, em, ic);
    }
    if (strcmp(action, "device_repair") == 0) {
        return handle_repair(dm, req, result);
    }
    return 0;
}
