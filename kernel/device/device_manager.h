/*
 * kernel/device/device_manager.h
 *
 * JARVIS OS — device manager authority.
 *
 * Owns the device table, common driver interface dispatch, power and
 * status control, IRQ delivery and statistics. Every request passes
 * through here; drivers are never called directly.
 */

#ifndef JARVIS_DEVICE_MANAGER_H
#define JARVIS_DEVICE_MANAGER_H

#include "base_device.h"

#define JVK_DM_MAX_DEVICES 16

typedef void (*dm_event_fn)(void* user, const char* message);

typedef struct {
    jvk_device_t devices[JVK_DM_MAX_DEVICES];
    int          count;
    dm_event_fn  on_event;
    void*        event_user;
    /* statistics */
    unsigned long reads;
    unsigned long writes;
    unsigned long interrupts;
    unsigned long errors;
} jvk_device_manager_t;

int  dm_init(jvk_device_manager_t* dm);
void dm_shutdown(jvk_device_manager_t* dm);
void dm_set_observer(jvk_device_manager_t* dm, dm_event_fn fn, void* user);

int  dm_register(jvk_device_manager_t* dm, jvk_device_id_t id,
                 const char* name, const char* driver, int irq,
                 const jvk_driver_ops_t* ops, void* impl);
jvk_device_t* dm_get(jvk_device_manager_t* dm, jvk_device_id_t id);
jvk_device_t* dm_find(jvk_device_manager_t* dm, const char* name);

int  dm_set_state(jvk_device_manager_t* dm, jvk_device_id_t id,
                  jvk_device_state_t state);
int  dm_set_power(jvk_device_manager_t* dm, jvk_device_id_t id, int on);

/* Driver dispatch. Return bytes moved, or -1 on error. */
int  dm_read(jvk_device_manager_t* dm, jvk_device_id_t id, char* out, int cap);
int  dm_write(jvk_device_manager_t* dm, jvk_device_id_t id,
              const char* data, int len);
/* Commit a driver's buffered writes. Returns bytes flushed, or -1 when the
   device is unavailable or has no buffering. */
int  dm_flush(jvk_device_manager_t* dm, jvk_device_id_t id);

/* Device raises its own IRQ (counted + processed). Returns 1 if raised. */
int  dm_raise_interrupt(jvk_device_manager_t* dm, jvk_device_id_t id);
/* Kernel ISR path: deliver one IRQ to every powered device on that line. */
int  dm_handle_irq(jvk_device_manager_t* dm, int irq);

void dm_error(jvk_device_manager_t* dm, jvk_device_id_t id,
              const char* message);

#endif /* JARVIS_DEVICE_MANAGER_H */
