/*
 * kernel/device/device_types.h
 *
 * JARVIS OS — virtual hardware identities and states.
 *
 * The device layer models real hardware bookkeeping (status, IRQ line,
 * power state, statistics). Drivers sit below the device manager; no
 * agent or subsystem talks to a driver directly.
 */

#ifndef JARVIS_DEVICE_TYPES_H
#define JARVIS_DEVICE_TYPES_H

typedef enum {
    JVK_DEV_CPU = 0,
    JVK_DEV_RAM,
    JVK_DEV_DISK,
    JVK_DEV_KEYBOARD,
    JVK_DEV_MOUSE,
    JVK_DEV_DISPLAY,
    JVK_DEV_PRINTER,
    JVK_DEV_CLOCK,
    JVK_DEV_COUNT,
} jvk_device_id_t;

typedef enum {
    JVK_DEV_CONNECTED = 0,
    JVK_DEV_DISCONNECTED,
    JVK_DEV_BUSY,
    JVK_DEV_WAITING,
    JVK_DEV_FAULT,
    JVK_DEV_DISABLED,
} jvk_device_state_t;

const char* jvk_device_name(jvk_device_id_t id); /* display name "Keyboard" */
const char* jvk_device_key(jvk_device_id_t id);  /* stable key "keyboard" */
const char* jvk_device_state_name(jvk_device_state_t state);
int         jvk_device_state_parse(const char* name, jvk_device_state_t* out);
int         jvk_device_parse(const char* name, jvk_device_id_t* out);

#endif /* JARVIS_DEVICE_TYPES_H */
