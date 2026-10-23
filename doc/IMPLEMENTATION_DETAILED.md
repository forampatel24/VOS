# JARVIS OS — Detailed Implementation Handover

**Version:** 1.4 · **Date:** 2026-10-23 · **Status:** M0 → M9b complete, 13 fixed windows + dynamic agent consoles, 90 pytest + 10 C smokes
**Purpose:** Give a teammate (or an LLM) *everything* implemented, in minute detail, with no prior context. Hand this file to an LLM and it can answer any question about what exists, how it works, and how to run/demo it.

---

## 1. Project Goal in One Paragraph

JARVIS OS is a **software-only educational OS simulator** where the “programs” are **AI agents**. The user sees a full desktop (boot → windows → taskbar) that *looks* like an OS, but underneath a **C kernel** (`jarvis_kernel.dll`) simulates CPU, processes, scheduling, virtual memory, and interrupts — all deterministically, on any laptop, with no real LLM calls. The Python FastAPI bridge loads that DLL via `ctypes` and exposes it as REST + WebSocket; the Electron+React desktop renders *only* what the kernel reports — no fake numbers.

**Golden rule:** Kernel is the only authority. Every click → `POST /api/command` → `jvk_command` → subsystem → JSON back → `WebSocket /ws` pushes new snapshot → React re-renders. React never mutates kernel state, subsystems never talk to each other (Kernel ↓ Subsystem only).

---

## 2. Tech Stack (exact versions on this machine)

| Layer | Technology |
|---|---|
| Kernel | C17, x86-64 NASM, cJSON (vendored), MinGW-w64 gcc 15.2.0 + NASM via `.toolchain/` |
| Bridge | Python 3.12.6, FastAPI 0.104.1, Pydantic 2.5, Uvicorn 0.24, httpx 0.24, websockets 12, SQLAlchemy/SQLite (for future FS) |
| Frontend | Electron 31, React 18.2, TypeScript 5.3, Vite 5.4, Tailwind 3.4, Zustand 4.5, Framer Motion 11, Axios 1.7 |
| Build/Test | `mingw32-make` (kernel Makefile) + CMake, `pytest`, Google Test style C smokes |

---

## 3. Repository Layout (what lives where)

```
VOS/
├── AGENTS.md, README.md, doc/ (TEAM_OVERVIEW, FRONTEND_SPEC, ARCHITECTURE, etc.)
├── kernel/
│   ├── abi.h (jvk_init, jvk_command, jvk_tick, jvk_snapshot, jvk_logs, jvk_shutdown, jvk_last_error)
│   ├── core/kernel.c (dispatcher, 80+ actions, snapshot builder, 65KB buffer)
│   ├── core/kernel_memory.c (mem_* glue, snapshot)
│   ├── core/kernel_ipc*.c (IPC dispatcher, sync commands, snapshot)
│   ├── core/kernel_shell.c (shell ↔ kernel re-entry)
│   ├── cpu/ (alu, clock, cpu, registers, instruction_set)
│   ├── asm/context_switch.S + .h (NASM save/restore, verified at boot)
│   ├── process/ (pcb, pid_gen, queues, context_switch, process_manager)
│   ├── scheduler/ (RR/FCFS/SJF/Priority strategy)
│   ├── machine/ (jvk_machine_t: board/chipset/bus/BIOS/POST/slots, machine_snapshot)
│   ├── memory/ (mm_types, frame_table, page_table, alloc_strategy, replace_policy, swap, memory_manager)
│   ├── filesystem/ (disk, fs_types, open_table, vfs, vfs_ops)
│   ├── device/ (device_types, device_manager, drivers/{keyboard,mouse,display,printer,disk_driver,clock_driver})
│   ├── interrupts/ (interrupt_types, ic, isr, error_manager)
│   ├── ipc/ (ipc_types, ipc_manager, ipc_queue, ipc_shm, ipc_pipe, ipc_sync, ipc_deadlock)
│   ├── shell/ (shell_parse, shell_cmds, shell_proc_cmds, shell_fs_cmds, shell_format)
│   ├── agent/ (agent_types, agent_registry, agent_tasks — registry + Plan→Think→Act→Report engine)
│   ├── core/kernel_agents*.c (launch/config/effects/snapshot glue, observer logging)
│   ├── deps/cJSON/
│   └── tests/test_cpu.c, test_process.c, test_memory.c, test_interrupts.c, test_filesystem.c, test_devices.c, test_ipc.c, test_shell.c, test_system.c, test_agents.c
├── backend/main.py (FastAPI + /ws) + services/kernel_loader.py (ctypes) + services/config_loader.py + services/agent_store.py (SQLite profiles) + routers/agents.py
├── frontend/
│   ├── electron/main.js (spawns uvicorn, waits /health, frameless window)
│   ├── electron/preload.js (window.jarvis)
│   ├── vite.config.ts, tailwind.config.js, tsconfig.json, index.html
│   └── src/
│       ├── api/types.ts + client.ts (typed REST + WS)
│       ├── store/kernelStore.ts (Zustand, WS deltas) + windowStore.ts (WM)
│       ├── components/BootScreen, desktop/Desktop, desktop/Taskbar, desktop/Window
│       └── components/windows/ (SystemMonitor, ProcessManager, MemoryViewer, CpuScheduler, EventLog, InterruptCenter, FileExplorer, DeviceManager, IpcLab, Terminal, AgentHub, AgentStudio, AgentConsole, Placeholder)
├── config/machine.json, clock.json, memory.json, paging.json, scheduler.json, fs.json, device.json, ipc.json, agents.json, theme.json, voice.json
├── tests/python/test_abi.py + test_agents_api.py + other subsystem tests (90 tests)
└── .venv/ + frontend/node_modules/ (not committed)
```

---

## 4. Kernel ABI (the only boundary)

All cross-boundary objects are **JSON strings** in static buffers owned by the kernel. Python uses `ctypes c_void_p + string_at` and never frees.

```c
const char* jvk_init(const char* config_json)       // "ok" or error
const char* jvk_command(const char* action_json)    // {"action":"..."} → {"ok":bool, ...}
void        jvk_tick(void)                          // advance clock, run CPU quantum, handle one IRQ batch, schedule
const char* jvk_snapshot(void)                      // full state for UI
const char* jvk_logs(int since)                     // incremental logs
void        jvk_shutdown(void)
int         jvk_last_error(char* buf, size_t n)
```

**`jvk_command` actions implemented (M0-M9b):**
* `ping`, `echo`
* `cpu_load_program {program:[u16]}`, `cpu_reset`, `cpu_step`
* `create_process {name, priority, burst_time|burst|bt, mem_budget|mem_budget_mb}`, `kill_process {pid}`, `suspend_process {pid}`, `resume_process {pid}`, `list_processes`
* `mem_alloc {pid, pages}` (denied with "memory budget exceeded" when the agent's cap would be crossed), `mem_free {pid}`, `mem_read {pid, addr}`, `mem_write {pid, addr, value}`, `mem_config {allocator, replacement, agentBudgetPages, referencedBit}`
* `trigger_interrupt {irq, source, detail}`, `list_interrupts`, `panic {reason}`, `clear_panic`
* `scheduler_config {algo|algorithm: rr|round_robin|fcfs|sjf|priority}`, `list_scheduler` (now includes metrics)
* `fs_*` (15 actions, M5), `list_devices` / `device_set` / `device_write` / `device_read` / `device_flush` / `device_interrupt` / `device_fault` / `device_repair` (M6)
* `ipc_queue_{create,send,recv,destroy}`, `ipc_shm_{create,attach,detach,write,read,destroy}`, `ipc_pipe_{create,open,close,write,read,destroy}`, `ipc_mutex_{create,lock,unlock,destroy}`, `ipc_sem_{create,wait,post,destroy}`, `ipc_deadlock`, `ipc_recover {pid}`, `list_ipc` (M7)
* `shell_exec {line}` → `{ok, output, prompt, cwd, clear, exit, command}`, `list_memory`, `list_filesystem`, `tick {count}`, `shutdown` (M8)
* `list_agents`, `agent_list`, `agent_tasks [pid]`, `agent_define {key,name,role,personality,color,icon,priority,mem_pages,tools}`, `agent_remove {key|id}`, `agent_launch {key|id}`, `agent_task {pid,title,input}`, `agent_cancel {pid,task_id}`, `agent_kill {pid}` (M9a)

**`jvk_snapshot` top-level keys (all live, no hardcode):**
```json
{
  "booted": true, "shutdown": false, "uptime_ticks": 123, "processes": 3,
  "machine": {"board":"JVK-VB1","chipset":"JARVIS-VC1","bus":"VirtIO-Bus","bios":"JVK-BIOS 1.0","bus_speed_mhz":800,"boot_device":"/dev/vdisk0","power_on":true,"post_ok":true,"cpu_cores":1,"ram_frames":1024,"device_slots":8,"slots":[{"name":"CPU0","kind":"cpu","detail":"virtual core 0","populated":true},...]},
  "memory": {
    "config": {"total_frames":1024,"page_words":16,"allocator":"first_fit","replacement":"clock","swap_enabled":true,"swap_slots":128,"agent_budget_pages":64,"page_table_strategy":"per-agent","present_bit":true,"dirty_bit":true,"referenced_bit":true},
    "stats": {"frames_used":6,"frames_free":1018,"pages_mapped":6,"allocs":3,"frees":0,"page_faults":1,"swap_ins":1,"swap_outs":2,"segfaults":0,"fragmented_allocs":0},
    "swap_used": 1,
    "frame_map": [1,1,2,0,0,...],
    "page_tables": [{"pid":1,"used":2,"budget":64,"pages":[{"vpage":0,"frame":0,"present":true,"referenced":true,"dirty":false},...]}]
  },
  "interrupts": {"pending":1,"handled":5,"dropped":0,"panic":false,"panic_reason":"","queue":[{"irq":"context_switch","priority":6,"source":"scheduler","detail":"pid 1 -> 2","enqueued_ticks":42,"status":"PENDING"}]},
  "error_manager": {"last_category":"none","last_message":"","counts":{"none":0,"memory":1,"process":0,"cpu":0,"system":0,"filesystem":0,"device":0,"ipc":0}},
  "scheduler": {"algo":"round_robin","current":2,"next":3,"switches":12,"cpu_utilization":0.83,"avg_wait":4.0,"avg_turnaround":9.5,"completed":1,"context_switch_irqs":7},
  "process_list": [{"pid":1,"name":"alpha","state":"READY","priority":5,"burst_time":7,"created_ticks":8,"cpu_used":0,"mem_pages":2,"mem_budget_pages":64},...],
  "queues": {"ready":[1,2,3],"waiting":[],"suspended":[],"terminated":[]},
  "filesystem": {"config":{...},"stats":{...},"disk":{...},"entries":[...],"open_files":[...]},
  "devices": {"count":8,"stats":{...},"list":[{"key":"disk","info":{"transfers":2,"writes":2,"buffered":0,"buffer_blocks":8,"flushes":1}},...]},
  "ipc": {"config":{"queue_capacity":8,"pipe_capacity":256,"shm_bytes":128},"stats":{...},"queues":[...],"segments":[...],"pipes":[...],"mutexes":[...],"semaphores":[...],"deliveries":[...],"wait_graph":{"edges":[],"cycles":[],"active":false}},
  "agents": {"config":{"max_definitions":24,"max_live":8,"max_tasks":8,"stage_plan":1,"stage_think":2,"stage_act":2,"stage_report":1},"stats":{"definitions":9,"live":1,"launched":1,"tasks_queued":1,"tasks_done":0,"tasks_failed":0,"reports":0},"definitions":[{"id":1,"key":"finance","name":"Finance Agent","role":"...","personality":"...","color":"#22d3ee","icon":"wallet","builtin":true,"priority":6,"mem_pages":4,"running_pid":1,"tools":["query_budget","compute_total"]}],"instances":[{"pid":1,"def_id":1,"key":"finance","name":"Finance Agent","state":"READY","activity":"THINKING","priority":6,"mem_pages":4,"memory_growth":0,"cpu_ticks":3,"launched_ticks":8,"tasks_queued":1,"tasks_done":0,"tasks_failed":0,"active_tasks":1,"current_task":1,"current":{"id":1,"title":"...","stage":"THINKING","progress":45,"tool":"compute_total"}}],"tasks":[{"id":1,"pid":1,"def_id":1,"agent_key":"finance","agent":"Finance Agent","title":"...","input":"...","stage":"THINKING","progress":45,"tool":"compute_total","plan":"...","thought":"...","action":"...","result":"...","report":"","writes":1,"reads":0,"created_ticks":9,"started_ticks":9,"completed_ticks":0}]},
  "shell": {"cwd":"/","prompt":"jarvis@vos:/$ ","last_status":0,"last_output":""},
  "cpu": {"pc":4,"sp":0,"ir":8208,"halted":false,"program_size":13,"registers":{"R0":10,"R1":5,...},"flags":{"Z":false,"N":false,"C":false}},
  "clock": {"speed_hz":1000,"quantum":10,"ticks":123}
}
```

**Logs:** `jvk_logs(since)` → `{"logs":[{"index":0,"message":"kernel booted"},...]}` — cap 256, incremental.

---

## 5. CPU & Clock (M2)

* **Registers:** 8× `R0-R7` + `PC` + `SP` + `IR` + flags `Z/N/C`. Saved per PCB.
* **ALU:** `ADD/SUB/MOV/CMP/JMP/JZ/HALT` on 16-bit words (`kernel/cpu/instruction_set.h`). Example sum-loop program is 13 words `[4096,0,4352,10,8208,4608,1,12544,24576,12,20480,4,32768]` → `R0` ends `55`.
* **Clock:** `speed_hz` (ticks/sec) + `quantum` (instructions per tick) from `config/clock.json` or `jvk_init({clock:{speed_hz,quantum}})`. `clock_tick()` increments, `clock_timer_fired()` signals quantum expiry.
* **Execution:** `jvk_tick` → `cpu_run(quantum)` → if still running and quantum hit → `TIMER_INTERRUPT` + `ic_enqueue(timer)` → handle IRQs → `scheduler_schedule`.
* **Halted:** `true` = idle, no program. `cpu_reset` or `cpu_load_program` clears it; `HALT` sets it. At boot with no agents, `halted YES` is correct idle.
* **Context switch stub:** `asm/context_switch.S` swaps 12 qwords via `jvk_context_switch` — verified at boot by swapping `[1,2,3,4]` ↔ `[100,200,300,400]` and checking, log `context_switch: asm stub verified`.

---

## 6. Process Manager (M3a)

* **PCB (`pcb.h`):** `pid (>0, 0=free), name[32], state (READY/RUNNING/WAITING/SUSPENDED/TERMINATED), priority, burst_time, regs, created_ticks (AT), cpu_used`.
* **PID gen (`pid_gen`):** monotonic from 1, never reuse (killed PID stays in `terminated` queue as record).
* **Queues (`queues`):** `JVK_QUEUE_CAP 16` each: `ready, waiting, suspended, terminated`. `JVK_MAX_PROCS 8`.
* **Lifecycle:** `create → READY (ready queue)`, `READY → SUSPENDED (suspend)`, `SUSPENDED → READY (resume, pushes to back)`, any → `TERMINATED (kill, record, slot freed)`. `pm_next_ready` rotates `ready[0]→back` (round-robin) and bumps `switches`.
* **API:** `create_process` → `pm_create` + `scheduler_register_ex`, `kill` → `pm_kill` + `scheduler_unregister` + `mm_free_pid`, `suspend/resume` → `scheduler_set_ready`.
* **Strict memory:** `mem_alloc` now checks `pm_get(pid)` exists in `kernel.c` — `PID 99 → error "no such process"` for any unknown PID.

---

## 7. Scheduler (M2 stub → M4.5 strategy)

* **Algorithms (strategy pattern):**
  * `RR` — `next` cursor, picks `(next + i) % count` first `ready`, then `next = (idx+1)%count`. Fair, ignores prio/BT.
  * `FCFS` — smallest `arrival` (`created_ticks`).
  * `SJF` — smallest `burst` (0 treated as ∞), tie → earliest arrival.
  * `Priority` — highest `priority`, tie → earliest arrival. (Starves low prio — demo it.)
* **State:** `count, next, switches, algo, pids[], ready[], priority[], burst[], arrival[], names[]`.
* **API:** `scheduler_config {algo: rr|fcfs|sjf|priority}` → `SCHEDULER_ALGO <algo>` log, `list_scheduler`. Default `RR` (from config or `jvk_init({scheduler:{algorithm}})`).
* **Snapshot:** `scheduler{algo, current (last picked), next (who will be picked on copy), switches}` — `current` is `g_current_pid` set on each schedule, `next` is peek without side effect.
* **Tick:** `jvk_tick` → `scheduler_schedule` → `g_current_pid = pid` → log `SCHEDULE pid=X algo=Y` → if `RR` also `pm_next_ready` to keep PM's `ready` visibly rotating; otherwise PM stays sorted — `Priority` will stick on highest.

---

## 8. Memory Manager (M3b)

* **Concepts:** Virtual pages (16 words each, `JVK_MM_PAGE_WORDS`) → physical frames. One frame = one page. `PAGE_SIZE` for display is 4096 bytes, but kernel reports `page_words`.
* **Structures:** Global `frame_table` (per-frame `state, owner_pid, vpage, loaded_seq, access_seq, ref_bit`), per-process `page_table` (64 entries, `present, frame, swap_slot, referenced, dirty`), heap `data[frames][16]` + swap `data[slots][16]` (`JVK_MAX_FRAMES 1024, JVK_MAX_SWAP 512`).
* **Placement (first/best/worst-fit):** Scan free-frame runs. `first` = lowest run ≥ need, `best` = smallest run ≥ need, `worst` = largest. If no contiguous run, collect scattered (marks `fragmented_allocs` + `EXTERNAL_FRAGMENTATION`).
* **Replacement (FIFO/LRU/Clock):** Global. `FIFO` = smallest `loaded_seq`, `LRU` = smallest `access_seq`, `Clock` = circular hand clearing `ref_bit`.
* **Swap:** `swap_put` saves evicted page to free slot, `swap_take` restores on fault.
* **Flow alloc:** `validate → gather contiguous run else scattered → if still short & swap on → reclaim via replacement until `free ≥ need` → claim frames → map PTEs (present 1) → zero-fill → `MEMORY_ALLOCATED`.
* **Flow access:** `addr → vpage=addr/16 offset=addr%16` → if `!present` → `PAGE_FAULT` → `FRAME_RECLAIM` → `SWAP_OUT` → `swap_take` → `SWAP_IN` → update `ref/dirty`.
* **API:** `mem_alloc` (strict PID check), `mem_free` (per PID), `mem_read/write` (via `resolve` → fault handling), `mem_config {allocator, replacement}`.
* **Snapshot:** `frame_map[frame]=owner or 0`, `page_tables[{pid, pages[{vpage,frame,present,referenced,dirty}]}]`, `stats`.

---

## 9. Interrupt Controller & Error Manager (M4)

* **Priorities:** `0 shutdown >1 page_fault >2 disk >3 keyboard >4 timer >5 software` (lower = more urgent).
* **Queue:** Cap 32, sorted insert by `(priority, enqueued_ticks)` — FIFO within same prio. `pending, handled, dropped` counters.
* **ISR registry:** One handler per IRQ, `isr_register_defaults` sets 6 no-op handlers except `shutdown` → `ic_set_panic`.
* **Wiring:** `mem_event_trampoline` → `PAGE_FAULT → ic_enqueue(page_fault, "memory", detail)`, `jvk_tick` timer expiry → `ic_enqueue(timer, "timer", "quantum expired")`. Each `jvk_tick` **drains all pending** in priority order before scheduling (process doesn't advance until queue empty) and logs `IRQ_HANDLED`. `shutdown` sets `panic` and freezes scheduling (`SCHEDULER_PAUSED`) until `clear_panic`.
* **Error manager:** 4 categories `memory/process/cpu/system`, counts + `last_message`, `system → panic`.
* **Snapshot:** `interrupts{pending,handled,dropped,panic,panic_reason,queue[]}` + `error_manager{last_category,last_message,counts}`.
* **ABI:** `trigger_interrupt {irq, source, detail}`, `list_interrupts`, `panic {reason}`, `clear_panic`.
* **Tick interval:** `backend/main.py` WS loop `send snapshot → wait 0.5s for client → jvk_tick → sleep 2.0` = ~2.5s per update so queue stays visible.

## 9.5 IPC & Synchronization (M7)

* **Subsystem:** `kernel/ipc/` is pure logic — no JSON, no process-manager or scheduler calls. It returns `JVK_IPC_BLOCKED` and buffers woken pids (`ipc_take_wakeups`); the kernel glue performs the actual `pm_block`/`pm_wake` and scheduler updates.
* **Objects:** bounded message queues (default 8 messages), shared-memory segments (dedicated 512-byte-per-segment arena, `attach` required before read/write), named pipes (bounded byte ring, per-writer pending chunk), recursive mutexes with unlock handoff, counting semaphores with permit handoff.
* **Blocking:** a sender blocks when full (message is held in the wait record and committed when a slot frees); a receiver blocks when empty and receives the next message through a per-pid delivery slot; pipe readers/writers, mutex waiters and semaphore waiters wake in FIFO order. `pid 0` is the system caller and never blocks (returns `would block`).
* **Deadlock:** wait-for edges are built from mutex waiter→owner and semaphore waiter→holders; cycles are found with DFS and deduplicated by sorted pid set (signature guard prevents log spam). The kernel raises the `IPC_DEADLOCK` observer event and a software IRQ, logs the cycle, and `ipc_recover {pid}` releases that pid's objects, kills the process through the kernel kill hook, then re-runs detection.
* **Seeding:** `config/ipc.json` (`seed`, capacities) creates `agent-bus` queue, `shared-board` segment, `console-pipe`, `log-lock` mutex and `worker-slots` semaphore at boot; boot log reports the configuration.
* **Snapshot:** `ipc.config`, full `stats`, object arrays with waiters/owners/data, pending deliveries, and `wait_graph {edges,cycles,active}` (recomputed on every snapshot).
* **Errors:** new `JVK_ERR_IPC` category (`"ipc"`) records failed IPC calls in the error manager.

## 9.6 Shell & CLI (M8)

* **Subsystem:** `kernel/shell/` owns the tokenizer (quotes), path normalisation (`.`/`..`, root clamp), output builder and command implementations. It does not access subsystems directly: a `sh_exec_fn` callback re-enters `jvk_command`, so every shell command is a real kernel request and reply.
* **Commands:** `help, echo, clear, pwd, cd, version, whoami, exit, shutdown, ps, kill, suspend, resume, memory/mem, alloc, free, write, read, sched, tick, devices, ipc, send, recv, files, ls, mkdir, touch, cat, write-file, rm, mv, cp, chmod`. `kill`/`suspend`/`resume` accept a pid or a process name.
* **Glue:** `kernel/core/kernel_shell.c` implements `shell_exec {line}` → `{ok, output, prompt, cwd, clear, exit, command}`; prompt is derived from cwd (`jarvis@vos:<cwd>$ `), `clear`/`exit` are flags for the terminal, and the last output/status live in the `shell` snapshot object.
* **Kernel additions:** `list_memory`, `list_filesystem`, `tick {count}` and `shutdown` actions; `kill_process` and IPC recovery share one kill helper that frees process, scheduler, memory, files and IPC state.

## 9.7 Agent System (M9a) & Consoles (M9b)

* **Subsystems:** `kernel/agent/agent_registry.c` owns definitions (24 slots, one live instance per definition) and `agent_tasks.c` owns per-agent task slots (8 each), the deterministic stage machine and generated plan/thought/action/result/report text. Neither touches process/memory/FS directly; `kernel/core/kernel_agents*.c` is the glue.
* **Built-ins:** nine definitions seeded from `config/agents.json` (Finance, Coding, Research, Writing, HR, Legal, Marketing, Travel, Health) with role, personality, colour, icon label, tool list, priority and memory pages. `agent_define` adds/updates custom definitions; the backend mirrors Agent Studio profiles into them after boot.
* **Process integration:** `agent_launch` calls `pm_create`, `scheduler_register_ex`, `pm_set_budget` and `mm_alloc` (budget = `mem_pages + 2` so the ACT stage can grow by at most 2 pages), creates `/Agents/<key>` in the VFS, and returns the real pid. `agent_kill`/`kill_process` share the kernel kill hook so the registry entry and process slot stay consistent.
* **Task lifecycle:** `agent_task {pid,title,input}` queues a task; only when the scheduler selects that pid does `kagent_tick` advance it. Defaults: PLAN 1 tick, THINK 2, ACT 2, REPORT 1. Stages are `QUEUED, PLANNING, THINKING, ACTING, REPORTING, DONE` (plus `FAILED`/`CANCELLED`). Tasks are recycled only when terminal.
* **Tool effects:** deterministic per-tool simulation — budget arithmetic, VFS search/read, spec-read/code-generation/tests, draft writing/rewriting, onboarding/policy/compliance/brainstorm/campaign/itinerary/booking/wellness/reminder. Writes update the VFS and a report is stored at `/Agents/<key>/report-<task>.md`.
* **Snapshot:** `agents {config, stats, definitions, instances, tasks}`; each instance carries pid, state/activity, cpu ticks, memory pages/growth, task counts and the current task; each task carries the full trace and progress.
* **Backend persistence (M9b):** `backend/services/agent_store.py` stores custom profiles in SQLite (`JARVIS_AGENT_DB`, default `data/jarvis_agents.db`, table `agent_profiles`). `backend/routers/agents.py` exposes `GET/POST /api/agents` and `PUT/DELETE /api/agents/{id}` with validation/clamping and a `409` when a running profile is deleted. `register_stored_agents()` re-registers all profiles after kernel boot.
* **UI (M9b):** Agent Hub (stats, installed definitions, launch/stop, open console, queue/pipeline), Agent Studio (create/edit/delete custom profiles + save-and-launch), and dynamic Agent Consoles keyed by pid (task dispatch, pipeline bar, trace, report, queue/history, cancel, +1/+10 ticks, terminate). `windowStore` now supports arbitrary `agent-<pid>` ids; taskbar lists live consoles.

---

## 10. Bridge (Backend)

* `backend/main.py` — `lifespan` boots `jvk_init({"boot":True})`, `CORSMiddleware` *, endpoints:
  * `GET /health` → `{ok, kernel: snapshot}`
  * `GET /api/snapshot` → snapshot
  * `POST /api/command` → `jvk_command`
  * `GET /api/tick` → `jvk_tick` + snapshot
  * `GET /api/logs?since=n` → `jvk_logs`
  * `GET|POST /api/agents`, `PUT|DELETE /api/agents/{id}` → SQLite agent profiles + kernel registry registration
  * `WS /ws` → loop as above, handles client `{"action":"tick"}` and `{"action":"command","payload":{}}`
* `backend/services/kernel_loader.py` — `ctypes CDLL(kernel/jarvis_kernel.dll)`, `c_void_p + string_at` (kernel owns buffers), wrappers `jvk_init, jvk_command, jvk_tick, jvk_snapshot, jvk_logs, jvk_shutdown`.

---

## 11. Frontend (Desktop)

* **Shell:** `electron/main.js` spawns `python -m uvicorn backend.main:app --host 127.0.0.1 --port 8000`, polls `/health` 12s, then `BrowserWindow` (frameless, `backgroundColor #060a14`, `preload.js`), dev loads `http://127.0.0.1:5173` else `dist/index.html`.
* **Build:** `Vite 5 + React 18 + TS 5 + Tailwind 3 + Zustand 4 + Framer Motion 11 + Axios 1.7`. `vite.config.ts` proxies `/api, /ws, /health` to `127.0.0.1:8000`. `tailwind.config.js` defines `jarvis.*` palette.
* **State:**
  * `store/kernelStore.ts` — `snapshot, logs, connected, error, logCursor` + `init` (REST initial + `createKernelSocket` WS push, merges logs, keeps 400), `command`, `doTick`.
  * `store/windowStore.ts` — `windows: Record<string, WindowState>` with `x,y,w,h,z,open,minimized`, fixed ids (`system-monitor … agent-hub, agent-studio, settings`) plus dynamic ``agent-${number}`` console ids; `open(id,title?)` creates missing dynamic windows, actions `open/close/focus/move/resize/toggleMinimize`. Defaults: System Monitor + Event Log open.
* **API:** `api/types.ts` mirrors `jvk_snapshot` exactly (including `agents`), `api/client.ts` `fetchSnapshot, fetchHealth, fetchLogs, sendCommand, tick, createKernelSocket` plus `fetchAgentProfiles, createAgentProfile, updateAgentProfile, deleteAgentProfile` (auto-reconnect, fallback `ws://127.0.0.1:8000/ws` when Vite on 5173).
* **Windows (all read `snapshot`/`logs` only, no fake):**
  * `SystemMonitor` — `processes, memory %, CPU PC/SP/IR/halted, uptime_ticks, queues, memory policies + swap + faults, clock`.
  * `ProcessManager` — table `PID/NAME/STATE/PRIO/BT/AT` with `PRIO/BT` inputs on CREATE (sends `priority, burst_time`), `PAUSE/RESUME/KILL`, queues footer.
  * `MemoryViewer` — `frames_used/total`, policies, `frame_map` grid, `PID pages → ALLOC / FREE`, `addr val → WRITE/READ`, per-PID `page_tables` cards.
  * `CpuScheduler` — `PC/SP/IR/halted, R0-7, flags Z/N/C, program_size`, `STEP/RESET`, `SCHEDULER RR/FCFS/SJF/Priority` switch + `RUNNING: PID X / NEXT: PID Y / switches` + `ready` queue with `● running` / `← next` highlights.
  * `EventLog` — filter `all/PROCESS/MEMORY/PAGE_FAULT/SWAP/SCHEDULE/TIMER/CPU`, search, `follow` toggle, `index# message` stream.
  * `InterruptCenter` — `pending/handled/dropped/panic` stats, panic banner + `CLEAR`, queue table `IRQ/PRIO/SOURCE/DETAIL/TICKS/STATUS`, trigger buttons for 6 IRQs + `PANIC`, error-manager `last_category` + counts.
  * `FileExplorer` — recursive tree, create file/folder, editor + save (`fs_write`), rename, delete, chmod, search, open-file table, disk/inode stats.
  * `DeviceManager` — 8 devices, per-driver I/O panel (keyboard inject/read, mouse move, display lines, printer spool, clock IRQ, disk status), power/fault/repair, counters straight from the snapshot.
  * `IpcLab` — live queues/segments/pipes/mutexes/semaphores, blocked sender/receiver lists, wait-for graph with cycles, DETECT + RECOVER actions, and an operations console (`IpcOps`) for every IPC action with pid selection.
  * `Terminal` — kernel shell: prompt/cwd from the `shell` snapshot, command history (↑/↓), ctrl+l clear, quick buttons, executes `shell_exec` for every line.
  * `AgentHub` — live agent stats, nine built-in + custom definitions (colour/role/tools/priority/memory), launch/stop, open console, aggregate task queue with stage/progress, STEP ticks.
  * `AgentStudio` — form for name, role/system prompt, personality, colour, icon, ≤4 simulated tools, priority and memory pages; saves through `/api/agents` (SQLite), edits and deletes custom profiles, Save & Launch opens the new console.
  * `AgentConsole` — one per running pid: task title/input dispatch, queue/history with cancel, `QUEUED→PLANNING→THINKING→ACTING→REPORTING→DONE` pipeline and progress, plan/thought/action/result/report trace, CPU ticks/memory/growth, +1/+10 tick controls and terminate.
  * `Placeholder` — honest `Subsystem offline — arrives in Mx` for Settings until M10b.
* **Chrome:** `Desktop` (wallpaper + 12 desktop icons in two columns, double-click to open, fixed windows + dynamic consoles rendered from `windowStore`) + `Taskbar` (launcher, window buttons, live agent-console buttons, `kernel live` dot + `procs/frames/agents`, `TICK` button, clock, Electron window controls) + `BootScreen` (2.2s phases + click to skip, failsafe 3.5s) + `Window` (drag header, resize handle, Framer Motion enter/exit, focus → `z`).

---

## 12. Build & Run (exact commands, PowerShell)

```powershell
# 0. project root
Set-Location D:\Foram_TP\VOS

# 1. venv (once)
python -m venv .venv
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r backend\requirements.txt

# 2. kernel
Set-Location kernel
mingw32-make          # → jarvis_kernel.dll
mingw32-make test     # 10/10 PASS
Set-Location ..

# 3. frontend
Set-Location frontend
npm install           # 505 packages
npx tsc --noEmit      # no output = pass
npx vite build        # → dist/index.html + 401KB JS
Set-Location ..

# 4. tests
python -m pytest tests -q  # 90 passed

# 5. run (two terminals)
# Terminal A:
$env:PYTHONPATH="D:\Foram_TP\VOS"
python -m uvicorn backend.main:app --host 127.0.0.1 --port 8000 --reload
# Terminal B:
Set-Location frontend; npm run dev  # → http://localhost:5173/
```

Health: `http://127.0.0.1:8000/health`, WS: `ws://127.0.0.1:8000/ws`, Frontend: `http://localhost:5173`.

---

## 13. Current Demo Script (what works today)

1. Boot → desktop. `System Monitor` shows `0 procs, 0 frames, halted YES, pending 0`.
2. `Process Manager → CREATE low prio1 bt5 → high prio10 bt2` → `ready [1,2]`. Switch `CPU → Priority` → `NEXT` jumps to `2` (starves `1`).
3. `Memory Viewer → PID 1 pages 2 → ALLOC` → frame map `[1,1]`, `PID 1 — 2 pages`. `addr 5 val 42 → WRITE → READ =42`. `addr 999 → READ → segfault` in log.
4. `CPU → STEP` or `RESET`, load demo program via API to see `PC/R0` move and `TIMER` appear.
5. `Interrupt Center → software → page_fault` → queue `[page_fault, software]` → 2.5s later both `IRQ_HANDLED` (priority order) then `SCHEDULE` resumes; `shutdown → PANIC` freezes `ready` until `CLEAR`.
6. `File Explorer` → open `/Users/Admin/Documents/welcome.txt` → edit + SAVE (kernel `fs_write`), create a folder/file, chmod read-only and watch the write fail, search "welcome"; the disk/inode counters update from `filesystem` snapshot.
7. `Device Manager` → keyboard: inject + READ; display: write two lines; printer: queue 2 jobs then PRINT ONE (raises the disk IRQ and pops one job); clock: RAISE TIMER IRQ; inject a fault then repair; all counters are kernel statistics.
8. `Event Log` filter `PROCESS/MEMORY/PAGE_FAULT/FS_/DEVICE_` to prove every number came from kernel.
9. `System Monitor` → MACHINE panel shows the board/chipset/bus/BIOS/POST read from `config/machine.json`, plus live CPU utilisation / avg wait / avg turnaround; `Process Manager` MEM column shows each agent's `mem_pages / budget`; `Device Manager` disk → two writes stay `buffered`, FLUSH commits them.
10. `IPC Lab` → queue `agent-bus` already seeded; create processes, send until a sender blocks (amber waiter), recv to wake it; write a shared segment and attach/read it; lock `log-lock` from two pids and watch handoff; create two mutexes and cross-lock them → `DETECT` shows the cycle and `RECOVER` kills one holder. Every stat comes from `ipc` in the snapshot.
11. `Terminal` → `help`, `ps`, `ipc`, `files`, `mkdir /demo`, `write-file /demo/a.txt hello`, `cat`, then `send agent-bus hi` + `recv agent-bus`; `kill <name>`, `tick 3`, `shutdown` all run through the kernel.
12. `Agent Hub` → LAUNCH Finance → its console opens with a real pid; queue "Quarterly budget summary" → watch `PLANNING → THINKING → ACTING → REPORTING → DONE` as the scheduler gives the pid CPU; click `+10` to finish quickly, read the generated report and inspect `/Agents/finance/report-*.md` in File Explorer. `Process Manager` shows the same pid's PCB/memory; `Agent Studio` → create a custom agent → Save & Launch → it appears in Agent Hub and runs the same pipeline; restart the backend and it is still registered.

---

## 14. What Is Still Offline (honest placeholders)

* AI (Gemini reasoning/voice) and Settings persistence — Settings shows `offline — arrives in M10b`; the M9 agent system is fully live. Future modules follow the same vertical-slice pattern (kernel + bridge + window goes live together).

---

## 15. Verification Evidence (as of this doc)

* Ubuntu 24.04 WSL: `cd kernel && make clean && make && make test` → `libjarvis_kernel.so`, `PASS: cpu / process / memory / interrupts / filesystem / devices / ipc / shell / system / agents` (10/10)
* `python -m pytest tests -q` → `90 passed` (M5 filesystem + M6 devices + machine/metrics/budget/config + M7 IPC + M8 shell + M9 agent ABI + Agent Studio persistence/API + bridge/ABI)
* `cd frontend && npm run typecheck && npm run build` → clean (477 modules, 426KB JS / 25.2KB CSS)
* Live kernel round-trip (TestClient, same ctypes path): launch `finance` → `agent_task` → ticks → `DONE 100%`, tool `query_budget`, 355-char report at `/Agents/finance/report-1.md`; create `custom-*` profile → it appears in `agents.definitions`; agent kill succeeds. Existing FS/device/IPC/shell/machine checks remain green.

---

*End of handover — this doc + `TEAM_OVERVIEW.md` + `FRONTEND_SPEC.md` is the complete picture.*
