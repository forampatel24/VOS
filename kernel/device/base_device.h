/*
 * kernel/device/base_device.h
 *
 * JARVIS OS — common driver interface.
 *
 * Every driver implements the same operations: init, shutdown, read,
 * write, handle_interrupt, report_error and snapshot. The device
 * manager calls them; nothing else may.
 */

#ifndef JARVIS_BASE_DEVICE_H
#define JARVIS_BASE_DEVICE_H

#include "device_types.h"

struct cJSON;

typedef struct jvk_device jvk_device_t;

typedef struct {
    int  (*init)(jvk_device_t* dev);
    void (*shutdown)(jvk_device_t* dev);
    int  (*read)(jvk_device_t* dev, char* out, int cap);
    int  (*write)(jvk_device_t* dev, const char* data, int len);
    void (*handle_interrupt)(jvk_device_t* dev);
    void (*report_error)(jvk_device_t* dev, const char* message);
    void (*snapshot)(const jvk_device_t* dev, struct cJSON* out);
} jvk_driver_ops_t;

struct jvk_device {
    jvk_device_id_t        id;
    char                   name[24];
    char                   driver[24];
    jvk_device_state_t     state;
    int                    irq; /* jvk_irq_t value; -1 when none */
    int                    power_on;
    const jvk_driver_ops_t* ops;
    void*                  impl;
    /* statistics */
    unsigned long          irq_count;
    unsigned long          reads;
    unsigned long          writes;
    unsigned long          errors;
    unsigned long          bytes_in;
    unsigned long          bytes_out;
};

#endif /* JARVIS_BASE_DEVICE_H */
