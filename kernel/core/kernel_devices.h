/*
 * kernel/core/kernel_devices.h
 *
 * JARVIS OS — kernel-layer glue for the device manager.
 *
 * Registers the built-in virtual hardware, dispatches device_* actions,
 * routes device IRQs into the interrupt controller and serializes the
 * "devices" snapshot object.
 */

#ifndef JARVIS_KERNEL_DEVICES_H
#define JARVIS_KERNEL_DEVICES_H

#include "../deps/cJSON/cJSON.h"
#include "../device/device_manager.h"
#include "../interrupts/error_manager.h"
#include "../interrupts/ic.h"

int  kdev_boot(jvk_device_manager_t* dm, const cJSON* config_root);

int  kdev_handle(jvk_device_manager_t* dm, const char* action,
                 const cJSON* req, cJSON* result,
                 jvk_ic_t* ic, jvk_error_manager_t* em,
                 unsigned long ticks);

void kdev_snapshot(const jvk_device_manager_t* dm, cJSON* root);

/* ISR bridge: deliver one IRQ line to every powered device on it. */
void kdev_deliver_irq(jvk_device_manager_t* dm, int irq);

#endif /* JARVIS_KERNEL_DEVICES_H */
