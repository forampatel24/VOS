/*
 * kernel/tests/test_devices.c
 *
 * JARVIS OS — device manager & driver smoke test (M6).
 *
 * Exercises registration, lookup, driver dispatch, keyboard buffering,
 * mouse coordinates, display output, printer spooling, clock ticks,
 * power/state control, error reporting and IRQ fan-out.
 * Run from kernel/:  make test
 */

#include <stdio.h>
#include <string.h>

#include "device/device_manager.h"
#include "device/drivers/drivers.h"
#include "interrupts/interrupt_types.h"

static int g_failures = 0;

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static void register_all(jvk_device_manager_t* dm)
{
    dm_init(dm);
    dm_register(dm, JVK_DEV_KEYBOARD, "Keyboard", "keyboard",
                JVK_IRQ_KEYBOARD, jvk_driver_keyboard_ops(),
                jvk_driver_keyboard_impl());
    dm_register(dm, JVK_DEV_MOUSE, "Mouse", "mouse",
                JVK_IRQ_KEYBOARD, jvk_driver_mouse_ops(),
                jvk_driver_mouse_impl());
    dm_register(dm, JVK_DEV_DISPLAY, "Display", "display",
                JVK_IRQ_SOFTWARE, jvk_driver_display_ops(),
                jvk_driver_display_impl());
    dm_register(dm, JVK_DEV_PRINTER, "Printer", "printer",
                JVK_IRQ_DISK, jvk_driver_printer_ops(),
                jvk_driver_printer_impl());
    dm_register(dm, JVK_DEV_DISK, "Disk", "disk",
                JVK_IRQ_DISK, jvk_driver_disk_ops(),
                jvk_driver_disk_impl());
    dm_register(dm, JVK_DEV_CLOCK, "Clock", "clock",
                JVK_IRQ_TIMER, jvk_driver_clock_ops(),
                jvk_driver_clock_impl());
}

static void test_registry(void)
{
    jvk_device_manager_t dm;
    register_all(&dm);
    expect(dm.count == 6, "six devices registered");
    expect(dm_get(&dm, JVK_DEV_KEYBOARD) != NULL, "get keyboard");
    expect(dm_find(&dm, "keyboard") != NULL, "find by key");
    expect(dm_find(&dm, "disk") != NULL, "find by driver name");
    expect(dm_find(&dm, "nope") == NULL, "unknown device is null");
    expect(strcmp(jvk_device_key(JVK_DEV_DISPLAY), "display") == 0,
           "stable device key");
    dm_shutdown(&dm);
}

static void test_keyboard(void)
{
    jvk_device_manager_t dm;
    dm_init(&dm);
    dm_register(&dm, JVK_DEV_KEYBOARD, "Keyboard", "keyboard",
                JVK_IRQ_KEYBOARD, jvk_driver_keyboard_ops(),
                jvk_driver_keyboard_impl());
    char out[32];
    expect(dm_write(&dm, JVK_DEV_KEYBOARD, "abc", 3) == 3, "keyboard write");
    expect(dm_write(&dm, JVK_DEV_KEYBOARD, "de", 2) == 2, "keyboard append");
    int n = dm_read(&dm, JVK_DEV_KEYBOARD, out, (int)sizeof(out));
    expect(n == 5 && strcmp(out, "abcde") == 0, "keyboard read drains buffer");
    n = dm_read(&dm, JVK_DEV_KEYBOARD, out, (int)sizeof(out));
    expect(n == 0 && out[0] == '\0', "second read is empty");
    dm_shutdown(&dm);
}

static void test_mouse(void)
{
    jvk_device_manager_t dm;
    dm_init(&dm);
    dm_register(&dm, JVK_DEV_MOUSE, "Mouse", "mouse",
                JVK_IRQ_KEYBOARD, jvk_driver_mouse_ops(),
                jvk_driver_mouse_impl());
    char out[32];
    expect(dm_write(&dm, JVK_DEV_MOUSE, "640,480", 7) == 7, "mouse move");
    dm_read(&dm, JVK_DEV_MOUSE, out, (int)sizeof(out));
    expect(strcmp(out, "640,480") == 0, "mouse position read");
    dm_write(&dm, JVK_DEV_MOUSE, "left", 4);
    dm_read(&dm, JVK_DEV_MOUSE, out, (int)sizeof(out));
    expect(strcmp(out, "639,480") == 0, "relative mouse left");
    dm_shutdown(&dm);
}

static void test_display(void)
{
    jvk_device_manager_t dm;
    dm_init(&dm);
    dm_register(&dm, JVK_DEV_DISPLAY, "Display", "display",
                JVK_IRQ_SOFTWARE, jvk_driver_display_ops(),
                jvk_driver_display_impl());
    char out[256];
    dm_write(&dm, JVK_DEV_DISPLAY, "line one\nline two", 17);
    dm_read(&dm, JVK_DEV_DISPLAY, out, (int)sizeof(out));
    expect(strstr(out, "line one") != NULL, "display first line");
    expect(strstr(out, "line two") != NULL, "display second line");
    dm_shutdown(&dm);
}

static void test_printer(void)
{
    jvk_device_manager_t dm;
    dm_init(&dm);
    dm_register(&dm, JVK_DEV_PRINTER, "Printer", "printer",
                JVK_IRQ_DISK, jvk_driver_printer_ops(),
                jvk_driver_printer_impl());
    jvk_device_t* dev = dm_get(&dm, JVK_DEV_PRINTER);
    expect(dm_write(&dm, JVK_DEV_PRINTER, "job-a", 5) == 5, "queue job a");
    expect(dm_write(&dm, JVK_DEV_PRINTER, "job-b", 5) == 5, "queue job b");
    expect(dev->state == JVK_DEV_WAITING, "printer waiting while spooled");
    expect(dm_raise_interrupt(&dm, JVK_DEV_PRINTER) == 1, "printer irq");
    char out[32];
    int n = dm_read(&dm, JVK_DEV_PRINTER, out, (int)sizeof(out));
    expect(n == 5 && strcmp(out, "job-b") == 0, "irq printed first job");
    expect(dm_read(&dm, JVK_DEV_PRINTER, out, (int)sizeof(out)) == 0,
           "spool empty after irq + read");
    expect(dev->state == JVK_DEV_CONNECTED, "printer returns to connected");
    dm_shutdown(&dm);
}

static void test_clock_and_irq(void)
{
    jvk_device_manager_t dm;
    register_all(&dm);
    jvk_device_t* clock = dm_get(&dm, JVK_DEV_CLOCK);
    jvk_device_t* kbd = dm_get(&dm, JVK_DEV_KEYBOARD);
    jvk_device_t* mouse = dm_get(&dm, JVK_DEV_MOUSE);
    expect(dm_handle_irq(&dm, JVK_IRQ_TIMER) == 1, "timer irq reaches clock");
    expect(clock->irq_count == 1, "clock irq counted");
    expect(dm_handle_irq(&dm, JVK_IRQ_KEYBOARD) == 2,
           "keyboard irq fans out to keyboard+mouse");
    expect(kbd->irq_count == 1 && mouse->irq_count == 1,
           "both keyboard-line devices counted");

    char out[32];
    dm_read(&dm, JVK_DEV_CLOCK, out, (int)sizeof(out));
    expect(strcmp(out, "1") == 0, "clock device ticked");
    expect(dm_read(&dm, JVK_DEV_DISK, out, (int)sizeof(out)) > 0,
           "disk read path reports status");
    dm_shutdown(&dm);
}

static void test_power_and_errors(void)
{
    jvk_device_manager_t dm;
    register_all(&dm);
    jvk_device_t* kbd = dm_get(&dm, JVK_DEV_KEYBOARD);

    expect(dm_set_power(&dm, JVK_DEV_KEYBOARD, 0) == 1, "power off keyboard");
    expect(kbd->state == JVK_DEV_DISABLED, "powered off is disabled");
    expect(dm_write(&dm, JVK_DEV_KEYBOARD, "x", 1) == -1,
           "offline device rejects write");
    expect(dm_set_power(&dm, JVK_DEV_KEYBOARD, 1) == 1, "power back on");
    expect(kbd->state == JVK_DEV_CONNECTED, "powered on is connected");

    dm_set_state(&dm, JVK_DEV_KEYBOARD, JVK_DEV_FAULT);
    char tmp[8];
    expect(dm_read(&dm, JVK_DEV_KEYBOARD, tmp, (int)sizeof(tmp)) == -1,
           "faulted device rejects read");

    dm_error(&dm, JVK_DEV_MOUSE, "cable unplugged");
    jvk_device_t* mouse = dm_get(&dm, JVK_DEV_MOUSE);
    expect(mouse->errors == 1 && mouse->state == JVK_DEV_FAULT,
           "dm_error marks fault and counts");
    expect(dm.errors == 1, "manager error count");
    dm_shutdown(&dm);
}

int main(void)
{
    test_registry();
    test_keyboard();
    test_mouse();
    test_display();
    test_printer();
    test_clock_and_irq();
    test_power_and_errors();

    if (g_failures == 0) {
        printf("test_devices: all checks passed\n");
        return 0;
    }
    printf("test_devices: %d failure(s)\n", g_failures);
    return 1;
}
