/*
 * kernel/core/kernel.c
 *
 * JARVIS OS — kernel entry point and JSON ABI.
 *
 * Milestone M2 scope: the kernel core now runs on the documented ABI
 * (jvk_init / jvk_command / jvk_tick / jvk_snapshot / jvk_logs /
 * jvk_shutdown / jvk_last_error) and drives a simulated CPU plus virtual
 * clock. Parsing uses the vendored cJSON library (kernel/deps/cJSON).
 *
 * Commands dispatch by action name. The CPU is fed programs as arrays of
 * 16-bit words (kernel/cpu/instruction_set.h); every tick advances the
 * clock, runs one time-slice quantum of instructions, surfaces the timer
 * interrupt at the preemption point, and asks the scheduler for the next
 * process.
 *
 * All returned JSON strings live in static buffers owned by the kernel.
 * The bridge must use ctypes restype=c_void_p + string_at and never free
 * these pointers.
 */

#define JARVIS_KERNEL_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abi.h"
#include "asm/context_switch.h"
#include "cpu/alu.h"
#include "cpu/clock.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "deps/cJSON/cJSON.h"
#include "scheduler/scheduler.h"

/* ---- kernel state --------------------------------------------------- */

#define JVK_LOG_CAP  256
#define JVK_LOG_LEN  256

static int           g_booted    = 0;
static unsigned long g_ticks     = 0;
static int           g_shutdown  = 0;
static char          g_last_error[JVK_LOG_LEN] = "";

static char          g_logs[JVK_LOG_CAP][JVK_LOG_LEN];
static int           g_log_count = 0;

static char          g_init_buf[128];
static char          g_cmd_buf[4096];
static char          g_snap_buf[4096];
static char          g_log_buf[JVK_LOG_CAP * (JVK_LOG_LEN + 32)];

static jvk_cpu_t     g_cpu;
static jvk_clock_t   g_clock;
static jvk_scheduler_t g_sched;
static int           g_cpu_halt_logged = 0;

/* ---- logging --------------------------------------------------------- */

static void jvk_log(const char* msg)
{
    if (g_log_count >= JVK_LOG_CAP) {
        return;
    }
    snprintf(g_logs[g_log_count], JVK_LOG_LEN, "%s", msg);
    g_log_count++;
}

static void jvk_set_error(const char* msg)
{
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
}

/* ---- JSON helpers ---------------------------------------------------- */

static void json_into(char* buf, size_t cap, cJSON* obj)
{
    char* s = cJSON_PrintUnformatted(obj);
    snprintf(buf, cap, "%s", s != NULL ? s : "{}");
    if (s != NULL) {
        free(s);
    }
    cJSON_Delete(obj);
}

static const char* fail(const char* msg)
{
    jvk_set_error(msg);
    cJSON* o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "ok", 0);
    cJSON_AddStringToObject(o, "error", msg);
    json_into(g_cmd_buf, sizeof(g_cmd_buf), o);
    return g_cmd_buf;
}

/* ---- ABI implementation --------------------------------------------- */

const char* jvk_init(const char* config_json)
{
    int speed_hz = 1000;
    int quantum  = 10;

    if (config_json != NULL && config_json[0] != '\0') {
        cJSON* root = cJSON_Parse(config_json);
        if (root == NULL) {
            return fail("invalid config JSON");
        }
        cJSON* clock = cJSON_GetObjectItemCaseSensitive(root, "clock");
        if (cJSON_IsObject(clock)) {
            cJSON* s = cJSON_GetObjectItemCaseSensitive(clock, "speed_hz");
            if (cJSON_IsNumber(s)) {
                speed_hz = (int)s->valuedouble;
            }
            cJSON* q = cJSON_GetObjectItemCaseSensitive(clock, "quantum");
            if (cJSON_IsNumber(q)) {
                quantum = (int)q->valuedouble;
            }
        }
        cJSON_Delete(root);
    }

    g_booted    = 1;
    g_ticks     = 0;
    g_shutdown  = 0;
    g_log_count = 0;
    g_last_error[0] = '\0';
    g_cpu_halt_logged = 0;

    cpu_init(&g_cpu);
    clock_init(&g_clock, speed_hz, quantum);
    scheduler_init(&g_sched);

    jvk_log("kernel booted");
    char msg[JVK_LOG_LEN];
    snprintf(msg, sizeof(msg), "clock: speed_hz=%d quantum=%d",
             g_clock.speed_hz, g_clock.quantum);
    jvk_log(msg);
    if (g_sched.ctx_verified) {
        jvk_log("context_switch: asm stub verified");
    }

    snprintf(g_init_buf, sizeof(g_init_buf), "ok");
    return g_init_buf;
}

const char* jvk_command(const char* action_json)
{
    if (action_json == NULL) {
        return fail("missing action JSON");
    }

    cJSON* req = cJSON_Parse(action_json);
    if (req == NULL) {
        return fail("invalid action JSON");
    }

    cJSON* act = cJSON_GetObjectItemCaseSensitive(req, "action");
    if (!cJSON_IsString(act)) {
        cJSON_Delete(req);
        return fail("missing action field");
    }
    const char* action = act->valuestring;

    cJSON* result = cJSON_CreateObject();

    if (strcmp(action, "ping") == 0) {
        jvk_log("command: ping");
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddStringToObject(result, "kernel", "c-native");
        cJSON_AddBoolToObject(result, "pong", 1);
    } else if (strcmp(action, "echo") == 0) {
        cJSON* msg = cJSON_GetObjectItemCaseSensitive(req, "message");
        jvk_log("command: echo");
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddStringToObject(result, "echo",
                                cJSON_IsString(msg) ? msg->valuestring : "");
    } else if (strcmp(action, "cpu_load_program") == 0) {
        cJSON* prog = cJSON_GetObjectItemCaseSensitive(req, "program");
        if (!cJSON_IsArray(prog)) {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error",
                                    "program must be an array of words");
        } else {
            uint16_t words[JVK_PROG_CAP];
            int n = cJSON_GetArraySize(prog);
            if (n > JVK_PROG_CAP) {
                n = JVK_PROG_CAP;
            }
            for (int i = 0; i < n; i++) {
                cJSON* item = cJSON_GetArrayItem(prog, i);
                words[i] = (uint16_t)(cJSON_IsNumber(item) ? item->valuedouble : 0);
            }
            int ok = cpu_load_program(&g_cpu, words, (uint32_t)n);
            cJSON_AddBoolToObject(result, "ok", ok ? 1 : 0);
            cJSON_AddNumberToObject(result, "words", n);
            if (ok) {
                g_cpu_halt_logged = 0;
                jvk_log("CPU_PROGRAM_LOADED");
            }
        }
    } else if (strcmp(action, "cpu_reset") == 0) {
        cpu_reset(&g_cpu);
        g_cpu_halt_logged = 0;
        jvk_log("CPU_RESET");
        cJSON_AddBoolToObject(result, "ok", 1);
    } else if (strcmp(action, "cpu_step") == 0) {
        cpu_step(&g_cpu);
        jvk_log("CPU_STEP");
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddBoolToObject(result, "halted", g_cpu.regs.halted);
        cJSON_AddNumberToObject(result, "pc", g_cpu.regs.pc);
        cJSON_AddNumberToObject(result, "ir", g_cpu.regs.ir);
        for (int i = 0; i < 8; i++) {
            char key[8];
            snprintf(key, sizeof(key), "R%d", i);
            cJSON_AddNumberToObject(result, key, g_cpu.regs.r[i]);
        }
    } else {
        jvk_set_error("unknown action");
        jvk_log("command rejected: unknown action");
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "unknown action");
    }

    json_into(g_cmd_buf, sizeof(g_cmd_buf), result);
    cJSON_Delete(req);
    return g_cmd_buf;
}

void jvk_tick(void)
{
    if (!g_booted || g_shutdown) {
        return;
    }
    g_ticks++;
    clock_tick(&g_clock);

    int executed = cpu_run(&g_cpu, g_clock.quantum);

    if (g_cpu.regs.halted && !g_cpu_halt_logged) {
        g_cpu_halt_logged = 1;
        jvk_log("CPU_HALT");
    }

    if (clock_timer_fired(&g_clock)) {
        clock_clear_timer(&g_clock);
        /* The quantum expired while the CPU was still running: that is
           the preemption point a real OS would switch on. */
        if (!g_cpu.regs.halted && executed >= g_clock.quantum) {
            jvk_log("TIMER_INTERRUPT");
        }
    }

    int pid = scheduler_schedule(&g_sched);
    if (pid >= 0) {
        char msg[JVK_LOG_LEN];
        snprintf(msg, sizeof(msg), "SCHEDULE pid=%d", pid);
        jvk_log(msg);
    }

    if (g_ticks % 10 == 0) {
        char msg[JVK_LOG_LEN];
        snprintf(msg, sizeof(msg), "tick %lu", g_ticks);
        jvk_log(msg);
    }
}

const char* jvk_snapshot(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "booted", g_booted);
    cJSON_AddBoolToObject(root, "shutdown", g_shutdown);
    cJSON_AddNumberToObject(root, "uptime_ticks", g_ticks);
    cJSON_AddNumberToObject(root, "processes", g_sched.count);
    cJSON_AddNumberToObject(root, "memory_pages", 0);

    cJSON* cpu = cJSON_CreateObject();
    cJSON_AddNumberToObject(cpu, "pc", g_cpu.regs.pc);
    cJSON_AddNumberToObject(cpu, "sp", g_cpu.regs.sp);
    cJSON_AddNumberToObject(cpu, "ir", g_cpu.regs.ir);
    cJSON_AddBoolToObject(cpu, "halted", g_cpu.regs.halted);
    cJSON_AddNumberToObject(cpu, "program_size", g_cpu.program_size);

    cJSON* regs = cJSON_CreateObject();
    for (int i = 0; i < 8; i++) {
        char key[8];
        snprintf(key, sizeof(key), "R%d", i);
        cJSON_AddNumberToObject(regs, key, g_cpu.regs.r[i]);
    }
    cJSON_AddItemToObject(cpu, "registers", regs);

    cJSON* flags = cJSON_CreateObject();
    cJSON_AddBoolToObject(flags, "Z", (g_cpu.regs.flags & FLAG_Z) != 0);
    cJSON_AddBoolToObject(flags, "N", (g_cpu.regs.flags & FLAG_N) != 0);
    cJSON_AddBoolToObject(flags, "C", (g_cpu.regs.flags & FLAG_C) != 0);
    cJSON_AddItemToObject(cpu, "flags", flags);

    cJSON_AddItemToObject(root, "cpu", cpu);

    cJSON* clock = cJSON_CreateObject();
    cJSON_AddNumberToObject(clock, "speed_hz", g_clock.speed_hz);
    cJSON_AddNumberToObject(clock, "quantum", g_clock.quantum);
    cJSON_AddNumberToObject(clock, "ticks", g_clock.ticks);
    cJSON_AddItemToObject(root, "clock", clock);

    json_into(g_snap_buf, sizeof(g_snap_buf), root);
    return g_snap_buf;
}

const char* jvk_logs(int since)
{
    if (since < 0) {
        since = 0;
    }
    cJSON* root = cJSON_CreateObject();
    cJSON* arr  = cJSON_CreateArray();
    for (int i = since; i < g_log_count; i++) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "index", i);
        cJSON_AddStringToObject(item, "message", g_logs[i]);
        cJSON_AddItemToArray(arr, item);
    }
    cJSON_AddItemToObject(root, "logs", arr);
    json_into(g_log_buf, sizeof(g_log_buf), root);
    return g_log_buf;
}

void jvk_shutdown(void)
{
    if (g_booted && !g_shutdown) {
        jvk_log("kernel shutdown");
        g_shutdown = 1;
    }
}

int jvk_last_error(char* buf, size_t n)
{
    if (buf == NULL || n == 0) {
        return 0;
    }
    snprintf(buf, n, "%s", g_last_error);
    return (int)strlen(buf);
}