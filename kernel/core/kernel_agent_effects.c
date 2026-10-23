#include <stdio.h>
#include <string.h>

#include "kernel_agents_internal.h"

static int store_file(jvk_vfs_t* fs, const char* path, int pid,
                      const char* text, unsigned long ticks)
{
    int id = 0;
    if (fs == NULL || text == NULL) {
        return 0;
    }
    if (!vfs_resolve(fs, path, &id)) {
        jvk_fs_status_t st = vfs_create(fs, path, pid, 0644u, text,
                                        (int)strlen(text), ticks, &id);
        return st == JVK_FS_OK;
    }
    int written = 0;
    jvk_fs_status_t st = vfs_write_path(fs, path, pid, text,
                                        (int)strlen(text), 0, ticks,
                                        &written);
    return st == JVK_FS_OK;
}

static void report_path(char* out, int cap, const char* key, int task_id)
{
    snprintf(out, (size_t)cap, "/Agents/%s/report-%d.md", key, task_id);
}

static void draft_path(char* out, int cap, const char* key, int task_id)
{
    snprintf(out, (size_t)cap, "/Agents/%s/task-%d-draft.md", key, task_id);
}

static int first_int(const char* text, int fallback)
{
    int sign = 1;
    for (int i = 0; text != NULL && text[i] != '\0'; i++) {
        if (text[i] == '-') {
            sign = -1;
            continue;
        }
        if (text[i] >= '0' && text[i] <= '9') {
            int value = 0;
            while (text[i] >= '0' && text[i] <= '9') {
                value = value * 10 + (text[i] - '0');
                i++;
            }
            return sign * value;
        }
        sign = 1;
    }
    return fallback;
}

static int sum_ints(const char* text, int fallback)
{
    int total = 0;
    int found = 0;
    for (int i = 0; text != NULL && text[i] != '\0'; i++) {
        if (text[i] < '0' || text[i] > '9') {
            continue;
        }
        int value = 0;
        while (text[i] >= '0' && text[i] <= '9') {
            value = value * 10 + (text[i] - '0');
            i++;
        }
        total += value;
        found = 1;
        if (text[i] == '\0') {
            break;
        }
    }
    return found ? total : fallback;
}

static void read_tool(jvk_vfs_t* fs, int pid, const char* path,
                      char* result, int cap, int* reads)
{
    char buf[JVK_AGENT_TEXT_LEN];
    int len = 0;
    jvk_fs_status_t st = vfs_read_path(fs, path, pid, buf, sizeof(buf), &len);
    if (st != JVK_FS_OK) {
        snprintf(result, (size_t)cap, "read %s failed (%s)", path,
                 jvk_fs_status_name(st));
        return;
    }
    *reads += 1;
    snprintf(result, (size_t)cap, "%s: %.90s", path, buf);
}

static void search_tool(jvk_vfs_t* fs, int pid, const char* pattern,
                        char* result, int cap, int* reads)
{
    int ids[16];
    int count = vfs_search(fs, pattern, ids, 16);
    if (count <= 0) {
        snprintf(result, (size_t)cap, "no files matched \"%s\"", pattern);
        return;
    }
    *reads += count;
    char path[JVK_FS_PATH_LEN];
    vfs_build_path(fs, ids[0], path, sizeof(path));
    snprintf(result, (size_t)cap, "%d match(es); first=%s", count, path);
}

static void execute_tool(jvk_agent_registry_t* reg, jvk_vfs_t* fs, int pid,
                         const jvk_agent_effect_t* effect,
                         unsigned long ticks, char* result, int cap,
                         int* writes, int* reads)
{
    const char* tool = effect->tool;
    const char* input = effect->input;
    int value = first_int(input, 0);
    int total = sum_ints(input, value);
    if (strcmp(tool, "query_budget") == 0) {
        snprintf(result, (size_t)cap,
                 "budget envelope %d credits; status within_limit", value);
    } else if (strcmp(tool, "compute_total") == 0) {
        snprintf(result, (size_t)cap, "computed total: %d", total);
    } else if (strcmp(tool, "read_spec") == 0) {
        read_tool(fs, pid, "/System/README.txt", result, cap, reads);
    } else if (strcmp(tool, "read_file") == 0) {
        const char* path = (input[0] == '/') ? input
                                             : "/Users/Admin/Documents/welcome.txt";
        read_tool(fs, pid, path, result, cap, reads);
    } else if (strcmp(tool, "search_files") == 0) {
        search_tool(fs, pid, input[0] != '\0' ? input : "txt", result, cap,
                    reads);
    } else if (strcmp(tool, "summarize") == 0) {
        int words = input[0] == '\0' ? 0 : 1;
        for (const char* p = input; *p != '\0'; p++) {
            if (*p == ' ') words++;
        }
        snprintf(result, (size_t)cap,
                 "summary: %d word(s), %d char(s); key theme = %s",
                 words, (int)strlen(input), input);
    } else if (strcmp(tool, "generate_code") == 0) {
        char path[JVK_FS_PATH_LEN];
        draft_path(path, sizeof(path), effect->key, effect->task_id);
        int ok = store_file(fs, path, pid,
                            "generated stub\nint main(void) { return 0; }\n",
                            ticks);
        if (ok) *writes += 1;
        snprintf(result, (size_t)cap, "generated 3 line(s) into %s", path);
    } else if (strcmp(tool, "run_tests") == 0) {
        snprintf(result, (size_t)cap, "tests: 6 passed, 0 failed");
    } else if (strcmp(tool, "write_draft") == 0 ||
               strcmp(tool, "rewrite") == 0) {
        char path[JVK_FS_PATH_LEN];
        draft_path(path, sizeof(path), effect->key, effect->task_id);
        int ok = store_file(fs, path, pid, input, ticks);
        if (ok) *writes += 1;
        snprintf(result, (size_t)cap, "%s draft at %s (%d chars)",
                 strcmp(tool, "rewrite") == 0 ? "rewrote" : "stored",
                 path, (int)strlen(input));
    } else if (strcmp(tool, "campaign_report") == 0) {
        char path[JVK_FS_PATH_LEN];
        draft_path(path, sizeof(path), effect->key, effect->task_id);
        int ok = store_file(fs, path, pid, input, ticks);
        if (ok) *writes += 1;
        snprintf(result, (size_t)cap, "campaign brief saved to %s", path);
    } else if (strcmp(tool, "extract_clauses") == 0) {
        snprintf(result, (size_t)cap,
                 "extracted %d clause(s) for compliance review",
                 input[0] == '\0' ? 0 : total % 7 + 1);
    } else if (strcmp(tool, "compliance_check") == 0) {
        snprintf(result, (size_t)cap,
                 "compliance scan complete; risk=%s",
                 (total % 2) == 0 ? "low" : "medium");
    } else if (strcmp(tool, "policy_lookup") == 0) {
        snprintf(result, (size_t)cap,
                 "policy lookup for \"%s\": section %d applies",
                 input, total % 12 + 1);
    } else if (strcmp(tool, "onboarding_checklist") == 0) {
        snprintf(result, (size_t)cap,
                 "onboarding checklist generated with %d steps",
                 total % 5 + 3);
    } else if (strcmp(tool, "brainstorm") == 0) {
        snprintf(result, (size_t)cap,
                 "brainstorm produced %d campaign idea(s)", total % 4 + 2);
    } else if (strcmp(tool, "plan_itinerary") == 0) {
        snprintf(result, (size_t)cap,
                 "itinerary planned with %d stop(s)", total % 4 + 2);
    } else if (strcmp(tool, "booking_check") == 0) {
        snprintf(result, (size_t)cap, "booking check: simulated availability confirmed");
    } else if (strcmp(tool, "wellness_checklist") == 0) {
        snprintf(result, (size_t)cap, "wellness checklist: hydration, stretch, rest");
    } else if (strcmp(tool, "reminder") == 0) {
        snprintf(result, (size_t)cap, "reminder scheduled for the next tick window");
    } else {
        snprintf(result, (size_t)cap, "simulated %s completed", tool);
    }
    (void)reg;
}

static void apply_tool_effect(jvk_agent_registry_t* reg, jvk_vfs_t* fs,
                              jvk_memory_manager_t* mm,
                              const jvk_agent_effect_t* effect,
                              unsigned long ticks)
{
    if (fs == NULL) {
        agent_apply_tool_result(reg, effect->pid, effect->task_id,
                                "filesystem offline", 0, 0);
        return;
    }
    char result[JVK_AGENT_TEXT_LEN];
    int writes = 0;
    int reads = 0;
    execute_tool(reg, fs, effect->pid, effect, ticks, result, sizeof(result),
                 &writes, &reads);
    jvk_agent_instance_t* inst = agent_instance_by_pid(reg, effect->pid);
    if (inst != NULL && inst->memory_growth < 2 && mm != NULL) {
        mm_alloc_result_t alloc;
        if (mm_alloc(mm, inst->pid, 1, &alloc) == MM_OK) {
            agent_add_memory(reg, inst->pid, 1);
        }
    }
    agent_apply_tool_result(reg, effect->pid, effect->task_id, result,
                            writes, reads);
}

void kagent_tick(jvk_agent_registry_t* reg, int current_pid,
                 unsigned long ticks, jvk_vfs_t* fs,
                 jvk_memory_manager_t* mm)
{
    if (reg == NULL || current_pid <= 0) {
        return;
    }
    jvk_agent_effect_t effect;
    if (!agent_step(reg, current_pid, ticks, &effect)) {
        return;
    }
    if (effect.kind == JVK_AGENT_EFFECT_TOOL) {
        apply_tool_effect(reg, fs, mm, &effect, ticks);
    } else if (effect.kind == JVK_AGENT_EFFECT_REPORT && fs != NULL) {
        char path[JVK_FS_PATH_LEN];
        report_path(path, sizeof(path), effect.key, effect.task_id);
        if (store_file(fs, path, effect.pid, effect.report, ticks)) {
            char msg[192];
            snprintf(msg, sizeof(msg), "AGENT_REPORT pid=%d task=%d path=%s",
                     effect.pid, effect.task_id, path);
            agent_emit(reg, msg);
        }
    }
}

void kagent_on_kill(jvk_agent_registry_t* reg, int pid)
{
    agent_on_kill(reg, pid);
}
