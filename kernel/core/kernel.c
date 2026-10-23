#include <stddef.h>
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
#include "interrupts/error_manager.h"
#include "interrupts/ic.h"
#include "interrupts/isr.h"
#include "kernel_devices.h"
#include "kernel_agents.h"
#include "kernel_fs.h"
#include "kernel_ipc.h"
#include "kernel_memory.h"
#include "kernel_shell.h"
#include "machine/machine.h"
#include "memory/memory_manager.h"
#include "process/process_manager.h"
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
static char          g_cmd_buf[131072];
static char          g_snap_buf[262144];
static char          g_log_buf[JVK_LOG_CAP * (JVK_LOG_LEN + 32)];

static jvk_cpu_t     g_cpu;
static jvk_clock_t   g_clock;
static jvk_scheduler_t g_sched;
static jvk_process_manager_t g_pm;
static jvk_memory_manager_t  g_mm;
static jvk_ic_t              g_ic;
static jvk_isr_registry_t    g_isr;
static jvk_error_manager_t   g_em;
static jvk_vfs_t             g_fs;
static jvk_device_manager_t  g_dm;
static jvk_machine_t         g_machine;
static jvk_ipc_t             g_ipc;
static kshell_t              g_ksh;
static jvk_agent_registry_t   g_agents;
static int           g_current_pid = -1;
static int           g_cpu_halt_logged = 0;
static unsigned long g_ctx_switches = 0;

/* ---- logging --------------------------------------------------------- */

static void jvk_log(const char* msg)
{
    if (g_log_count >= JVK_LOG_CAP) {
        return;
    }
    snprintf(g_logs[g_log_count], JVK_LOG_LEN, "%s", msg);
    g_log_count++;
}

/* Memory-subsystem events land in the central kernel log.
   Page faults also enqueue a page-fault interrupt so the IC can show them. */
static void mem_event_trampoline(void* user, const char* message)
{
    (void)user;
    jvk_log(message);
    if (strncmp(message, "PAGE_FAULT", 10) == 0) {
        const char* detail = message[10] == ' ' ? message + 11 : message;
        ic_enqueue(&g_ic, JVK_IRQ_PAGE_FAULT, "memory", detail, g_ticks);
    } else if (strncmp(message, "SEGV", 4) == 0) {
        em_record(&g_em, JVK_ERR_MEMORY, message, &g_ic);
    } else if (strncmp(message, "FRAME_RECLAIM", 13) == 0) {
        /* reclaim already logged; no extra interrupt */
    }
}

static void jvk_set_error(const char* msg)
{
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
}

/* Filesystem subsystem events: log them, and escalate permission denials. */
static void fs_event_trampoline(void* user, const char* message)
{
    (void)user;
    jvk_log(message);
    if (strncmp(message, "FS_DENIED", 9) == 0) {
        em_record(&g_em, JVK_ERR_FILESYSTEM, message, &g_ic);
    }
}

static void dev_event_trampoline(void* user, const char* message)
{
    (void)user;
    jvk_log(message);
}

/* IPC subsystem events are already human-readable; deadlocks also raise
   a software interrupt so the Interrupts window shows the detection. */
static void ipc_event_trampoline(void* user, const char* message)
{
    (void)user;
    jvk_log(message);
    if (strncmp(message, "IPC_DEADLOCK", 12) == 0) {
        const char* detail = message[12] == ' ' ? message + 13 : message;
        ic_enqueue(&g_ic, JVK_IRQ_SOFTWARE, "ipc", detail, g_ticks);
    }
}

static void agent_event_trampoline(void* user, const char* message)
{
    (void)user;
    jvk_log(message);
}

/* Every hardware IRQ line is delivered to the devices sitting on it. */
static void dev_isr_bridge(jvk_ic_t* ic, const jvk_irq_entry_t* entry, void* user)
{
    (void)ic;
    if (entry != NULL) {
        kdev_deliver_irq((jvk_device_manager_t*)user, (int)entry->irq);
    }
}

/* ---- kill path (shared by kill_process and IPC recovery) -------------- */

static int kernel_kill_process(int pid, int* freed_out, int* closed_out)
{
    if (!pm_kill(&g_pm, pid)) {
        return 0;
    }
    kagent_on_kill(&g_agents, pid);
    scheduler_unregister(&g_sched, pid);
    int freed = mm_free_pid(&g_mm, pid);
    int closed = kfs_close_owner(&g_fs, pid);
    ipc_release_pid(&g_ipc, pid, g_ticks);
    char msg[JVK_LOG_LEN];
    snprintf(msg, sizeof(msg), "PROCESS_KILLED pid=%d", pid);
    jvk_log(msg);
    if (freed_out != NULL) {
        *freed_out = freed > 0 ? freed : 0;
    }
    if (closed_out != NULL) {
        *closed_out = closed > 0 ? closed : 0;
    }
    return 1;
}

static void ipc_kill_hook(void* user, int pid)
{
    (void)user;
    (void)kernel_kill_process(pid, NULL, NULL);
}

static kipc_ctx_t ipc_context(void)
{
    kipc_ctx_t ctx = {
        .ipc = &g_ipc,
        .pm = &g_pm,
        .sched = &g_sched,
        .em = &g_em,
        .ic = &g_ic,
        .kill = ipc_kill_hook,
        .kill_user = NULL,
    };
    return ctx;
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

/* ---- process helpers -------------------------------------------------- */

static void queue_to_json(const jvk_queue_t* q, cJSON* arr)
{
    for (int i = 0; i < q->count; i++) {
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(q->items[i]));
    }
}

static void queues_to_json(cJSON* root, const char* key,
                           const jvk_process_manager_t* pm)
{
    cJSON* q = cJSON_CreateObject();

    cJSON* ready = cJSON_CreateArray();
    queue_to_json(&pm->ready, ready);
    cJSON_AddItemToObject(q, "ready", ready);

    cJSON* waiting = cJSON_CreateArray();
    queue_to_json(&pm->waiting, waiting);
    cJSON_AddItemToObject(q, "waiting", waiting);

    cJSON* suspended = cJSON_CreateArray();
    queue_to_json(&pm->suspended, suspended);
    cJSON_AddItemToObject(q, "suspended", suspended);

    cJSON* terminated = cJSON_CreateArray();
    queue_to_json(&pm->terminated, terminated);
    cJSON_AddItemToObject(q, "terminated", terminated);

    cJSON_AddItemToObject(root, key, q);
}

static void process_list_to_json(cJSON* arr, const jvk_process_manager_t* pm)
{
    for (int i = 0; i < JVK_MAX_PROCS; i++) {
        const jvk_pcb_t* p = &pm->pcbs[i];
        if (p->pid == 0) {
            continue;
        }
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "pid", p->pid);
        cJSON_AddStringToObject(item, "name", p->name);
        cJSON_AddStringToObject(item, "state", pm_state_name(p->state));
        cJSON_AddNumberToObject(item, "priority", p->priority);
        cJSON_AddNumberToObject(item, "burst_time", p->burst_time);
        cJSON_AddNumberToObject(item, "created_ticks", (double)p->created_ticks);
        cJSON_AddNumberToObject(item, "cpu_used", p->cpu_used);
        cJSON_AddNumberToObject(item, "mem_pages", mm_pages_of(&g_mm, p->pid));
        cJSON_AddNumberToObject(item, "mem_budget_pages", p->mem_budget_pages);
        cJSON_AddItemToArray(arr, item);
    }
}

/* ---- ABI implementation --------------------------------------------- */

const char* jvk_init(const char* config_json)
{
    int speed_hz = 1000;
    int quantum  = 10;
    char sched_algo_buf[32] = "";
    cJSON* root  = NULL;

    if (config_json != NULL && config_json[0] != '\0') {
        root = cJSON_Parse(config_json);
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
        cJSON* sched = cJSON_GetObjectItemCaseSensitive(root, "scheduler");
        if (cJSON_IsObject(sched)) {
            cJSON* a = cJSON_GetObjectItemCaseSensitive(sched, "algorithm");
            if (!cJSON_IsString(a)) a = cJSON_GetObjectItemCaseSensitive(sched, "algo");
            if (cJSON_IsString(a)) {
                snprintf(sched_algo_buf, sizeof(sched_algo_buf), "%s", a->valuestring);
            }
        }
    }

    if (g_booted) {
        vfs_shutdown(&g_fs);
        dm_shutdown(&g_dm);
        kipc_shutdown(&g_ipc);
    }

    g_booted    = 1;
    g_ticks     = 0;
    g_shutdown  = 0;
    g_log_count = 0;
    g_last_error[0] = '\0';
    g_current_pid = -1;
    g_cpu_halt_logged = 0;
    g_ctx_switches = 0;

    cpu_init(&g_cpu);
    clock_init(&g_clock, speed_hz, quantum);
    scheduler_init(&g_sched);
    if (sched_algo_buf[0] != '\0') {
        scheduler_set_algo(&g_sched, sched_algo_buf);
    }
    pm_init(&g_pm);
    if (!kmem_boot(&g_mm, root)) {
        cJSON_Delete(root);
        return fail("memory manager init failed");
    }
    mm_set_observer(&g_mm, mem_event_trampoline, NULL);
    ic_init(&g_ic);
    isr_init(&g_isr);
    isr_register_defaults(&g_isr);
    em_init(&g_em);

    if (!kfs_boot(&g_fs, root)) {
        cJSON_Delete(root);
        g_booted = 0;
        return fail("filesystem init failed");
    }
    if (!kdev_boot(&g_dm, root)) {
        cJSON_Delete(root);
        g_booted = 0;
        return fail("device manager init failed");
    }
    if (!kipc_boot(&g_ipc, root)) {
        cJSON_Delete(root);
        g_booted = 0;
        return fail("ipc manager init failed");
    }
    kipc_set_observer(&g_ipc, ipc_event_trampoline, NULL);
    ksh_boot(&g_ksh);
    int agent_defs = kagent_boot(&g_agents, root);
    if (agent_defs > 0) {
        agent_set_observer(&g_agents, agent_event_trampoline, NULL);
    }

    /* M2 machine: the board the kernel just came up on. */
    machine_init(&g_machine, root);
    machine_set_resources(&g_machine, 1, (unsigned long)g_mm.cfg.total_frames,
                          g_dm.count);
    machine_power_on(&g_machine);

    kfs_seed(&g_fs, 0);
    vfs_set_observer(&g_fs, fs_event_trampoline, NULL);
    dm_set_observer(&g_dm, dev_event_trampoline, NULL);

    /* Hardware IRQ lines are forwarded to the device layer. */
    isr_register(&g_isr, JVK_IRQ_TIMER,      dev_isr_bridge, &g_dm);
    isr_register(&g_isr, JVK_IRQ_DISK,       dev_isr_bridge, &g_dm);
    isr_register(&g_isr, JVK_IRQ_KEYBOARD,   dev_isr_bridge, &g_dm);
    isr_register(&g_isr, JVK_IRQ_SOFTWARE,   dev_isr_bridge, &g_dm);
    isr_register(&g_isr, JVK_IRQ_PAGE_FAULT, dev_isr_bridge, &g_dm);
    isr_register(&g_isr, JVK_IRQ_CONTEXT_SWITCH, dev_isr_bridge, &g_dm);

    cJSON_Delete(root);

    jvk_log("kernel booted");
    char msg[JVK_LOG_LEN];
    snprintf(msg, sizeof(msg), "clock: speed_hz=%d quantum=%d",
             g_clock.speed_hz, g_clock.quantum);
    jvk_log(msg);
    if (g_sched.ctx_verified) {
        jvk_log("context_switch: asm stub verified");
    }
    jvk_log("process_manager: ready");
    jvk_log("memory_manager: ready");
    jvk_log("interrupt_controller: ready");
    snprintf(msg, sizeof(msg), "machine: %s chipset=%s bus=%s@%dMHz boot=%s",
             g_machine.board, g_machine.chipset, g_machine.bus,
             g_machine.bus_speed_mhz, g_machine.boot_device);
    jvk_log(msg);
    snprintf(msg, sizeof(msg), "POST: %s cores=%d ram_frames=%lu slots=%d",
             g_machine.post_ok ? "ok" : "failed", g_machine.cpu_cores,
             g_machine.ram_frames, g_machine.device_slots);
    jvk_log(msg);
    snprintf(msg, sizeof(msg), "paging: tables=%s present_bit=%d dirty_bit=%d referenced_bit=%d",
             g_mm.cfg.page_table_strategy, g_mm.cfg.present_bit,
             g_mm.cfg.dirty_bit, g_mm.cfg.referenced_bit);
    jvk_log(msg);
    snprintf(msg, sizeof(msg), "memory budget: %d pages per agent%s",
             g_mm.cfg.agent_budget_pages,
             g_mm.cfg.agent_budget_pages > 0 ? "" : " (unlimited)");
    snprintf(msg, sizeof(msg),
             "memory: frames=%d allocator=%s replacement=%s swap=%s",
             g_mm.cfg.total_frames, mm_alloc_name(g_mm.cfg.allocator),
             mm_replace_name(g_mm.cfg.replacement),
             g_mm.cfg.swap_enabled ? "on" : "off");
    jvk_log(msg);

    snprintf(msg, sizeof(msg),
             "filesystem: blocks=%d block_size=%d inodes=%d",
             g_fs.disk.total_blocks, g_fs.disk.block_size, g_fs.inode_count);
    jvk_log(msg);
    snprintf(msg, sizeof(msg), "devices: registered=%d", g_dm.count);
    jvk_log(msg);
    int ipc_objects = 0;
    for (int i = 0; i < JVK_IPC_MAX_QUEUES; i++) ipc_objects += g_ipc.queues[i].used;
    for (int i = 0; i < JVK_IPC_MAX_SEGMENTS; i++) ipc_objects += g_ipc.segments[i].used;
    for (int i = 0; i < JVK_IPC_MAX_PIPES; i++) ipc_objects += g_ipc.pipes[i].used;
    for (int i = 0; i < JVK_IPC_MAX_MUTEXES; i++) ipc_objects += g_ipc.mutexes[i].used;
    for (int i = 0; i < JVK_IPC_MAX_SEMS; i++) ipc_objects += g_ipc.sems[i].used;
    snprintf(msg, sizeof(msg),
             "ipc: objects=%d queue_cap=%d pipe_cap=%d shm_bytes=%d",
             ipc_objects, g_ipc.cfg_queue_capacity, g_ipc.cfg_pipe_capacity,
             g_ipc.cfg_shm_bytes);
    jvk_log(msg);
    jvk_log("shell: ready");
    snprintf(msg, sizeof(msg),
             "agents: definitions=%d live_cap=%d builtins=%d",
             g_agents.def_count, JVK_AGENT_MAX_LIVE,
             g_agents.def_count >= 9 ? 1 : 0);
    jvk_log(msg);

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
    } else if (strcmp(action, "create_process") == 0) {
        cJSON* name_item = cJSON_GetObjectItemCaseSensitive(req, "name");
        cJSON* prio_item = cJSON_GetObjectItemCaseSensitive(req, "priority");
        cJSON* burst_item = cJSON_GetObjectItemCaseSensitive(req, "burst_time");
        if (!cJSON_IsNumber(burst_item)) burst_item = cJSON_GetObjectItemCaseSensitive(req, "burst");
        if (!cJSON_IsNumber(burst_item)) burst_item = cJSON_GetObjectItemCaseSensitive(req, "bt");
        const char* name = cJSON_IsString(name_item) ? name_item->valuestring
                                                      : "process";
        int priority = cJSON_IsNumber(prio_item) ? (int)prio_item->valuedouble : 0;
        int burst    = cJSON_IsNumber(burst_item) ? (int)burst_item->valuedouble : 0;
        if (burst < 0) burst = 0;
        /* M3b: an agent may declare its own memory budget (pages or MB). */
        cJSON* pages_item = cJSON_GetObjectItemCaseSensitive(req, "mem_budget");
        if (!cJSON_IsNumber(pages_item)) pages_item = cJSON_GetObjectItemCaseSensitive(req, "memBudget");
        cJSON* mb_item = cJSON_GetObjectItemCaseSensitive(req, "mem_budget_mb");
        if (!cJSON_IsNumber(mb_item)) mb_item = cJSON_GetObjectItemCaseSensitive(req, "memBudgetMB");
        int budget_pages = 0;
        if (cJSON_IsNumber(pages_item) && pages_item->valuedouble >= 0) {
            budget_pages = (int)pages_item->valuedouble;
        } else if (cJSON_IsNumber(mb_item) && mb_item->valuedouble > 0) {
            budget_pages = (int)(mb_item->valuedouble * 256.0);
        }
        int pid = 0;
        if (pm_create(&g_pm, name, priority, burst, g_ticks, &pid)) {
            scheduler_register_ex(&g_sched, pid, name, priority, burst, g_ticks);
            if (budget_pages > 0) {
                pm_set_budget(&g_pm, pid, budget_pages);
                mm_set_budget(&g_mm, pid, budget_pages);
            }
            char msg[JVK_LOG_LEN];
            snprintf(msg, sizeof(msg), "PROCESS_CREATED pid=%d name=%s", pid, name);
            jvk_log(msg);
            if (budget_pages > 0) {
                snprintf(msg, sizeof(msg), "MEM_BUDGET_SET pid=%d pages=%d", pid, budget_pages);
                jvk_log(msg);
            }
            cJSON_AddBoolToObject(result, "ok", 1);
            cJSON_AddNumberToObject(result, "pid", pid);
            cJSON_AddStringToObject(result, "name", name);
            cJSON_AddNumberToObject(result, "mem_budget_pages",
                                    mm_budget_of(&g_mm, pid));
        } else {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "process table full");
        }
    } else if (strcmp(action, "kill_process") == 0) {
        cJSON* pid_item = cJSON_GetObjectItemCaseSensitive(req, "pid");
        int pid = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
        int freed = 0;
        int closed = 0;
        if (kernel_kill_process(pid, &freed, &closed)) {
            cJSON_AddBoolToObject(result, "ok", 1);
            cJSON_AddNumberToObject(result, "pid", pid);
            cJSON_AddNumberToObject(result, "memory_pages_freed", freed);
            cJSON_AddNumberToObject(result, "files_closed", closed);
        } else {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "process not found");
        }
    } else if (strcmp(action, "suspend_process") == 0) {
        cJSON* pid_item = cJSON_GetObjectItemCaseSensitive(req, "pid");
        int pid = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
        if (pm_suspend(&g_pm, pid)) {
            scheduler_set_ready(&g_sched, pid, 0);
            char msg[JVK_LOG_LEN];
            snprintf(msg, sizeof(msg), "PROCESS_SUSPENDED pid=%d", pid);
            jvk_log(msg);
            cJSON_AddBoolToObject(result, "ok", 1);
            cJSON_AddNumberToObject(result, "pid", pid);
        } else {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "cannot suspend process");
        }
    } else if (strcmp(action, "resume_process") == 0) {
        cJSON* pid_item = cJSON_GetObjectItemCaseSensitive(req, "pid");
        int pid = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
        if (pm_resume(&g_pm, pid)) {
            scheduler_set_ready(&g_sched, pid, 1);
            char msg[JVK_LOG_LEN];
            snprintf(msg, sizeof(msg), "PROCESS_RESUMED pid=%d", pid);
            jvk_log(msg);
            cJSON_AddBoolToObject(result, "ok", 1);
            cJSON_AddNumberToObject(result, "pid", pid);
        } else {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "cannot resume process");
        }
    } else if (strcmp(action, "list_processes") == 0) {
        cJSON* arr = cJSON_CreateArray();
        process_list_to_json(arr, &g_pm);
        cJSON_AddItemToObject(result, "processes", arr);
        queues_to_json(result, "queues", &g_pm);
        cJSON_AddBoolToObject(result, "ok", 1);
    } else if (strncmp(action, "mem_", 4) == 0) {
        /* M4 fix: only an existing process may own memory — otherwise
           the UI would create address spaces for ghost PIDs. */
        if (strcmp(action, "mem_alloc") == 0) {
            cJSON* pid_item = cJSON_GetObjectItemCaseSensitive(req, "pid");
            int pid = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
            if (pm_get(&g_pm, pid) == NULL) {
                em_record(&g_em, JVK_ERR_PROCESS, "alloc for unknown pid", &g_ic);
                cJSON_AddBoolToObject(result, "ok", 0);
                cJSON_AddStringToObject(result, "error", "no such process");
            } else if (!kmem_handle(&g_mm, action, req, result)) {
                jvk_set_error("unknown action");
                cJSON_AddBoolToObject(result, "ok", 0);
                cJSON_AddStringToObject(result, "error", "unknown action");
            }
        } else if (!kmem_handle(&g_mm, action, req, result)) {
            jvk_set_error("unknown action");
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown action");
        }
    } else if (strcmp(action, "trigger_interrupt") == 0) {
        cJSON* irq_item = cJSON_GetObjectItemCaseSensitive(req, "irq");
        const char* irq_str = cJSON_IsString(irq_item) ? irq_item->valuestring : NULL;
        jvk_irq_t irq;
        if (!jvk_irq_parse(irq_str, &irq)) {
            em_record(&g_em, JVK_ERR_CPU, "unknown irq", &g_ic);
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown irq");
        } else {
            cJSON* src_item = cJSON_GetObjectItemCaseSensitive(req, "source");
            cJSON* det_item = cJSON_GetObjectItemCaseSensitive(req, "detail");
            const char* src = cJSON_IsString(src_item) ? src_item->valuestring : jvk_irq_name(irq);
            const char* det = cJSON_IsString(det_item) ? det_item->valuestring : "";
            if (ic_enqueue(&g_ic, irq, src, det, g_ticks)) {
                char lmsg[JVK_LOG_LEN];
                snprintf(lmsg, sizeof(lmsg), "IRQ_ENQUEUED irq=%s source=%s", jvk_irq_name(irq), src);
                jvk_log(lmsg);
                cJSON_AddBoolToObject(result, "ok", 1);
                cJSON_AddStringToObject(result, "irq", jvk_irq_name(irq));
                cJSON_AddNumberToObject(result, "pending", ic_pending(&g_ic));
            } else {
                em_record(&g_em, JVK_ERR_SYSTEM, "interrupt queue full", &g_ic);
                cJSON_AddBoolToObject(result, "ok", 0);
                cJSON_AddStringToObject(result, "error", "interrupt queue full");
            }
        }
    } else if (strcmp(action, "list_interrupts") == 0) {
        ic_snapshot(&g_ic, result);
        em_snapshot(&g_em, result);
        cJSON_AddBoolToObject(result, "ok", 1);
    } else if (strcmp(action, "panic") == 0) {
        cJSON* reason_item = cJSON_GetObjectItemCaseSensitive(req, "reason");
        const char* reason = cJSON_IsString(reason_item) ? reason_item->valuestring : "manual panic";
        ic_set_panic(&g_ic, reason);
        em_record(&g_em, JVK_ERR_SYSTEM, reason, &g_ic);
        jvk_log("KERNEL_PANIC");
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddStringToObject(result, "panic", reason);
    } else if (strcmp(action, "clear_panic") == 0) {
        ic_clear_panic(&g_ic);
        cJSON_AddBoolToObject(result, "ok", 1);
    } else if (strcmp(action, "scheduler_config") == 0) {
        cJSON* algo_item = cJSON_GetObjectItemCaseSensitive(req, "algo");
        if (!cJSON_IsString(algo_item)) algo_item = cJSON_GetObjectItemCaseSensitive(req, "algorithm");
        const char* algo = cJSON_IsString(algo_item) ? algo_item->valuestring : NULL;
        if (!scheduler_set_algo(&g_sched, algo)) {
            em_record(&g_em, JVK_ERR_CPU, "unknown scheduler algo", &g_ic);
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown algo (rr/fcfs/sjf/priority)");
        } else {
            char lmsg[JVK_LOG_LEN];
            snprintf(lmsg, sizeof(lmsg), "SCHEDULER_ALGO %s", scheduler_algo_name(g_sched.algo));
            jvk_log(lmsg);
            cJSON_AddBoolToObject(result, "ok", 1);
            cJSON_AddStringToObject(result, "algo", scheduler_algo_name(g_sched.algo));
        }
    } else if (strcmp(action, "list_scheduler") == 0) {
        cJSON_AddStringToObject(result, "algo", scheduler_algo_name(g_sched.algo));
        cJSON_AddNumberToObject(result, "switches", g_sched.switches);
        cJSON_AddNumberToObject(result, "current", g_current_pid);
        cJSON_AddNumberToObject(result, "context_switch_irqs", (double)g_ctx_switches);
        double util = 0.0, avg_wait = 0.0, avg_turn = 0.0;
        scheduler_metrics(&g_sched, &util, &avg_wait, &avg_turn);
        cJSON_AddNumberToObject(result, "cpu_utilization", util);
        cJSON_AddNumberToObject(result, "avg_wait", avg_wait);
        cJSON_AddNumberToObject(result, "avg_turnaround", avg_turn);
        cJSON_AddNumberToObject(result, "completed", (double)g_sched.completed);
        cJSON_AddBoolToObject(result, "ok", 1);
    } else if (strcmp(action, "list_agents") == 0 ||
               strncmp(action, "agent_", 6) == 0) {
        kagent_env_t env = {
            .pm = &g_pm,
            .sched = &g_sched,
            .mm = &g_mm,
            .fs = &g_fs,
            .kill = ipc_kill_hook,
            .kill_user = NULL,
        };
        (void)kagent_handle(&g_agents, &env, action, req, result, g_ticks);
    } else if (strncmp(action, "fs_", 3) == 0) {
        if (!kfs_handle(&g_fs, action, req, result, g_ticks)) {
            em_record(&g_em, JVK_ERR_FILESYSTEM, "unknown fs action", &g_ic);
            jvk_set_error("unknown action");
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown action");
        }
    } else if (strcmp(action, "list_devices") == 0 ||
               strncmp(action, "device_", 7) == 0) {
        if (!kdev_handle(&g_dm, action, req, result, &g_ic, &g_em, g_ticks)) {
            em_record(&g_em, JVK_ERR_DEVICE, "unknown device action", &g_ic);
            jvk_set_error("unknown action");
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown action");
        }
    } else if (strncmp(action, "ipc_", 4) == 0 ||
               strcmp(action, "list_ipc") == 0) {
        kipc_ctx_t ictx = ipc_context();
        if (!kipc_handle(&ictx, action, req, result, g_ticks)) {
            jvk_set_error("unknown action");
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown action");
        }
    } else if (strcmp(action, "shell_exec") == 0) {
        (void)ksh_handle(&g_ksh, action, req, result);
    } else if (strcmp(action, "list_memory") == 0) {
        kmem_snapshot(&g_mm, result);
        cJSON_AddBoolToObject(result, "ok", 1);
    } else if (strcmp(action, "list_filesystem") == 0) {
        kfs_snapshot(&g_fs, result);
        cJSON_AddBoolToObject(result, "ok", 1);
    } else if (strcmp(action, "tick") == 0) {
        cJSON* count_item = cJSON_GetObjectItemCaseSensitive(req, "count");
        int count = cJSON_IsNumber(count_item) ? (int)count_item->valuedouble : 1;
        if (count < 1) count = 1;
        if (count > 1000) count = 1000;
        for (int i = 0; i < count && !g_shutdown; i++) {
            jvk_tick();
        }
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddNumberToObject(result, "advanced", count);
        cJSON_AddNumberToObject(result, "uptime_ticks", g_ticks);
    } else if (strcmp(action, "shutdown") == 0) {
        jvk_shutdown();
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddBoolToObject(result, "shutdown", 1);
    } else {
        em_record(&g_em, JVK_ERR_CPU, "unknown action", &g_ic);
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

    int prev_pid = g_current_pid;
    int executed = cpu_run(&g_cpu, g_clock.quantum);
    /* M3a: charge this tick to whoever held the CPU. */
    scheduler_account(&g_sched, prev_pid);

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
            ic_enqueue(&g_ic, JVK_IRQ_TIMER, "timer", "quantum expired", g_ticks);
        }
    }

    /* Drain all pending interrupts in priority order before scheduling —
       process does not advance until queue is empty. */
    while (ic_pending(&g_ic) > 0) {
        jvk_irq_entry_t irq;
        if (!ic_dequeue_next(&g_ic, &irq)) {
            break;
        }
        char imsg[JVK_LOG_LEN];
        snprintf(imsg, sizeof(imsg), "IRQ_HANDLED irq=%s source=%s", jvk_irq_name(irq.irq), irq.source);
        jvk_log(imsg);
        isr_handle(&g_isr, &g_ic, &irq);
        if (g_ic.panic) {
            em_record(&g_em, JVK_ERR_SYSTEM, g_ic.panic_reason, &g_ic);
            break; /* panic freezes the rest until CLEAR */
        }
    }

    if (g_ic.panic) {
        /* Shutdown panic freezes scheduling for visuals — ready queue
           stays stuck and no SCHEDULE logs until CLEAR. */
        jvk_log("SCHEDULER_PAUSED panic active");
    } else {
        int pid = scheduler_schedule(&g_sched);
        if (pid >= 0) {
            /* M4: a switch between two live processes raises the
               lowest-priority interrupt, exactly like a real kernel. */
            if (prev_pid >= 0 && prev_pid != pid) {
                char detail[48];
                snprintf(detail, sizeof(detail), "pid %d -> %d", prev_pid, pid);
                ic_enqueue(&g_ic, JVK_IRQ_CONTEXT_SWITCH, "scheduler", detail,
                           g_ticks);
                g_ctx_switches++;
            }
            g_current_pid = pid;
            char msg[JVK_LOG_LEN];
            snprintf(msg, sizeof(msg), "SCHEDULE pid=%d algo=%s", pid, scheduler_algo_name(g_sched.algo));
            jvk_log(msg);
            /* Keep PM's ready queue visibly rotating only for RR — other
               algos pick by priority/burst, not rotation. */
            if (g_sched.algo == SCHED_RR) {
                pm_next_ready(&g_pm);
            }
        }
    }

    if (!g_ic.panic && g_current_pid > 0) {
        kagent_tick(&g_agents, g_current_pid, g_ticks, &g_fs, &g_mm);
    }

    if (g_ticks % 10 == 0) {
        char msg[JVK_LOG_LEN];
        snprintf(msg, sizeof(msg), "tick %lu", g_ticks);
        jvk_log(msg);
    }

    /* M7: re-evaluate the wait-for graph every tick; the subsystem only
       logs and counts when the deadlock signature actually changes. */
    (void)kipc_check_deadlock(&g_ipc);
}

const char* jvk_snapshot(void)
{
    cJSON* root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "booted", g_booted);
    cJSON_AddBoolToObject(root, "shutdown", g_shutdown);
    cJSON_AddNumberToObject(root, "uptime_ticks", g_ticks);
    cJSON_AddNumberToObject(root, "processes", g_pm.count);
    kmem_snapshot(&g_mm, root);
    ic_snapshot(&g_ic, root);
    em_snapshot(&g_em, root);
    kfs_snapshot(&g_fs, root);
    kdev_snapshot(&g_dm, root);
    machine_snapshot(&g_machine, root);
    kipc_snapshot(&g_ipc, root);
    ksh_snapshot(&g_ksh, root);
    kagent_snapshot(&g_agents, &g_pm, g_current_pid, root);

    cJSON* sched = cJSON_CreateObject();
    cJSON_AddStringToObject(sched, "algo", scheduler_algo_name(g_sched.algo));
    cJSON_AddNumberToObject(sched, "switches", g_sched.switches);
    cJSON_AddNumberToObject(sched, "current", g_current_pid);
    int next_pid = -1;
    if (g_sched.algo == SCHED_RR) {
        for (int i = 0; i < g_sched.count; i++) {
            int idx = (g_sched.next + i) % g_sched.count;
            if (g_sched.ready[idx]) {
                next_pid = g_sched.pids[idx];
                break;
            }
        }
    } else {
        /* For priority/sjf/fcfs, ask the scheduler who it would pick */
        jvk_scheduler_t copy = g_sched;
        next_pid = scheduler_schedule(&copy);
    }
    cJSON_AddNumberToObject(sched, "next", next_pid);
    double util = 0.0, avg_wait = 0.0, avg_turn = 0.0;
    scheduler_metrics(&g_sched, &util, &avg_wait, &avg_turn);
    cJSON_AddNumberToObject(sched, "cpu_utilization", util);
    cJSON_AddNumberToObject(sched, "avg_wait", avg_wait);
    cJSON_AddNumberToObject(sched, "avg_turnaround", avg_turn);
    cJSON_AddNumberToObject(sched, "completed", (double)g_sched.completed);
    cJSON_AddNumberToObject(sched, "context_switch_irqs", (double)g_ctx_switches);
    cJSON_AddItemToObject(root, "scheduler", sched);

    cJSON* proc_list = cJSON_CreateArray();
    process_list_to_json(proc_list, &g_pm);
    cJSON_AddItemToObject(root, "process_list", proc_list);
    queues_to_json(root, "queues", &g_pm);

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
        machine_power_off(&g_machine);
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