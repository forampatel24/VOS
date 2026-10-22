/*
 * kernel/machine/machine.c
 *
 * JARVIS OS — virtual motherboard (implementation).
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "machine.h"

static void copy_str(char* dst, size_t cap, const char* src)
{
    if (src == NULL) {
        return;
    }
    snprintf(dst, cap, "%s", src);
}

static const char* cfg_string(const cJSON* root, const char* key)
{
    if (!cJSON_IsObject(root)) {
        return NULL;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static int cfg_int(const cJSON* root, const char* key, int fallback)
{
    if (!cJSON_IsObject(root)) {
        return fallback;
    }
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsNumber(item) ? (int)item->valuedouble : fallback;
}

static void machine_add_slot(jvk_machine_t* m, const char* name,
                             const char* kind, const char* detail)
{
    if (m->slot_count >= JVK_MACHINE_MAX_SLOTS) {
        return;
    }
    jvk_machine_slot_t* s = &m->slots[m->slot_count++];
    copy_str(s->name, sizeof(s->name), name);
    copy_str(s->kind, sizeof(s->kind), kind);
    copy_str(s->detail, sizeof(s->detail), detail);
    s->populated = 1;
}

void machine_init(jvk_machine_t* m, const cJSON* config_root)
{
    if (m == NULL) {
        return;
    }
    memset(m, 0, sizeof(*m));
    copy_str(m->board, sizeof(m->board), "JVK-VB1");
    copy_str(m->chipset, sizeof(m->chipset), "JARVIS-VC1");
    copy_str(m->bus, sizeof(m->bus), "VirtIO-Bus");
    copy_str(m->bios, sizeof(m->bios), "JVK-BIOS 1.0");
    copy_str(m->boot_device, sizeof(m->boot_device), "/dev/vdisk0");
    m->bus_speed_mhz = 800;

    const cJSON* cfg = NULL;
    if (cJSON_IsObject(config_root)) {
        cfg = cJSON_GetObjectItemCaseSensitive(config_root, "machine");
    }
    copy_str(m->board, sizeof(m->board), cfg_string(cfg, "board") ? cfg_string(cfg, "board") : m->board);
    copy_str(m->chipset, sizeof(m->chipset), cfg_string(cfg, "chipset") ? cfg_string(cfg, "chipset") : m->chipset);
    copy_str(m->bus, sizeof(m->bus), cfg_string(cfg, "bus") ? cfg_string(cfg, "bus") : m->bus);
    copy_str(m->bios, sizeof(m->bios), cfg_string(cfg, "bios") ? cfg_string(cfg, "bios") : m->bios);
    copy_str(m->boot_device, sizeof(m->boot_device),
             cfg_string(cfg, "bootDevice") ? cfg_string(cfg, "bootDevice") : m->boot_device);
    m->bus_speed_mhz = cfg_int(cfg, "busSpeedMHz", m->bus_speed_mhz);
    m->cpu_cores     = cfg_int(cfg, "cpuCores", 1);
}

void machine_set_resources(jvk_machine_t* m, int cpu_cores,
                           unsigned long ram_frames, int device_slots)
{
    if (m == NULL) {
        return;
    }
    m->cpu_cores    = cpu_cores > 0 ? cpu_cores : 1;
    m->ram_frames   = ram_frames;
    m->device_slots = device_slots;

    m->slot_count = 0;
    char detail[48];
    for (int c = 0; c < m->cpu_cores && c < 2; c++) {
        snprintf(detail, sizeof(detail), "virtual core %d", c);
        char name[24];
        snprintf(name, sizeof(name), "CPU%d", c);
        machine_add_slot(m, name, "cpu", detail);
    }
    snprintf(detail, sizeof(detail), "%lu frames", ram_frames);
    machine_add_slot(m, "RAM0", "ram", detail);

    for (int d = 0; d < device_slots && m->slot_count < JVK_MACHINE_MAX_SLOTS; d++) {
        char name[24];
        snprintf(name, sizeof(name), "PCI%d", d);
        machine_add_slot(m, name, "device", "virtual device");
    }
}

void machine_power_on(jvk_machine_t* m)
{
    if (m == NULL) {
        return;
    }
    m->power_on = 1;
    m->post_ok  = (m->cpu_cores > 0 && m->ram_frames > 0) ? 1 : 0;
}

void machine_power_off(jvk_machine_t* m)
{
    if (m == NULL) {
        return;
    }
    m->power_on = 0;
}

void machine_snapshot(const jvk_machine_t* m, cJSON* root)
{
    if (m == NULL || root == NULL) {
        return;
    }
    cJSON* out = cJSON_CreateObject();
    cJSON_AddStringToObject(out, "board", m->board);
    cJSON_AddStringToObject(out, "chipset", m->chipset);
    cJSON_AddStringToObject(out, "bus", m->bus);
    cJSON_AddStringToObject(out, "bios", m->bios);
    cJSON_AddNumberToObject(out, "bus_speed_mhz", m->bus_speed_mhz);
    cJSON_AddStringToObject(out, "boot_device", m->boot_device);
    cJSON_AddBoolToObject(out, "power_on", m->power_on != 0);
    cJSON_AddBoolToObject(out, "post_ok", m->post_ok != 0);
    cJSON_AddNumberToObject(out, "cpu_cores", m->cpu_cores);
    cJSON_AddNumberToObject(out, "ram_frames", (double)m->ram_frames);
    cJSON_AddNumberToObject(out, "device_slots", m->device_slots);

    cJSON* slots = cJSON_CreateArray();
    for (int i = 0; i < m->slot_count; i++) {
        const jvk_machine_slot_t* s = &m->slots[i];
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", s->name);
        cJSON_AddStringToObject(item, "kind", s->kind);
        cJSON_AddStringToObject(item, "detail", s->detail);
        cJSON_AddBoolToObject(item, "populated", s->populated != 0);
        cJSON_AddItemToArray(slots, item);
    }
    cJSON_AddItemToObject(out, "slots", slots);

    cJSON_AddItemToObject(root, "machine", out);
}
