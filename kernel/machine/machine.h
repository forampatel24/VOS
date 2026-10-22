/*
 * kernel/machine/machine.h
 *
 * JARVIS OS — virtual machine / motherboard model (M2).
 *
 * The machine is the board the kernel boots on: its identity (board,
 * chipset, bus, BIOS), its power state and the POST result, plus the
 * physical inventory (CPU cores, RAM frames, occupied device slots).
 *
 * The machine never reads another subsystem: the kernel glue calls
 * machine_set_resources() with the counts it already owns, so the board
 * stays a passive description of the virtual hardware.
 */

#ifndef JARVIS_MACHINE_H
#define JARVIS_MACHINE_H

#include <stdint.h>

#include "../deps/cJSON/cJSON.h"

#define JVK_MACHINE_MAX_SLOTS 8

typedef struct {
    char name[24];     /* slot label, e.g. "CPU0", "RAM0", "PCI0" */
    char kind[16];     /* cpu | ram | device | bus */
    char detail[48];   /* human-readable occupancy */
    int  populated;
} jvk_machine_slot_t;

typedef struct {
    char             board[32];
    char             chipset[32];
    char             bus[32];
    char             bios[32];
    int              bus_speed_mhz;
    char             boot_device[16];
    int              cpu_cores;
    unsigned long    ram_frames;
    int              device_slots;
    int              power_on;
    int              post_ok;
    jvk_machine_slot_t slots[JVK_MACHINE_MAX_SLOTS];
    int              slot_count;
} jvk_machine_t;

/* Configure and power the board. `config_root` may be NULL. */
void machine_init(jvk_machine_t* m, const cJSON* config_root);
void machine_power_on(jvk_machine_t* m);
void machine_power_off(jvk_machine_t* m);

/* Kernel glue reports the resources it owns; the machine fills its slots. */
void machine_set_resources(jvk_machine_t* m, int cpu_cores,
                           unsigned long ram_frames, int device_slots);

/* Add the live "machine" object to a snapshot root. */
void machine_snapshot(const jvk_machine_t* m, cJSON* root);

#endif /* JARVIS_MACHINE_H */
