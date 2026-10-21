/*
 * kernel/device/drivers/drivers.h
 *
 * JARVIS OS — exported driver operations. Each driver owns its own
 * implementation state; the device manager only holds the pointers.
 */

#ifndef JARVIS_DRIVERS_H
#define JARVIS_DRIVERS_H

#include "../base_device.h"

const jvk_driver_ops_t* jvk_driver_keyboard_ops(void);
void*                   jvk_driver_keyboard_impl(void);

const jvk_driver_ops_t* jvk_driver_mouse_ops(void);
void*                   jvk_driver_mouse_impl(void);

const jvk_driver_ops_t* jvk_driver_display_ops(void);
void*                   jvk_driver_display_impl(void);

const jvk_driver_ops_t* jvk_driver_printer_ops(void);
void*                   jvk_driver_printer_impl(void);

const jvk_driver_ops_t* jvk_driver_disk_ops(void);
void*                   jvk_driver_disk_impl(void);

const jvk_driver_ops_t* jvk_driver_clock_ops(void);
void*                   jvk_driver_clock_impl(void);

#endif /* JARVIS_DRIVERS_H */
