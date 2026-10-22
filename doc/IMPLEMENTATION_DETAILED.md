# JARVIS OS — Detailed Implementation Handover

**Version:** 1.3 · **Date:** 2026-10-22 · **Status:** M0 → M8 complete, 10 live windows, 82 pytest + 9 C smokes — ready for midsem
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
│   ├── deps/cJSON/
│   └── tests/test_cpu.c, test_process.c, test_memory.c, test_interrupts.c, test_filesystem.c, test_devices.c, test_ipc.c, test_shell.c, test_system.c
├── backend/main.py (FastAPI + /ws) + services/kernel_loader.py (ctypes) + services/config_loader.py
├── frontend/
│   ├── electron/main.js (spawns uvicorn, waits /health, frameless window)
│   ├── electron/preload.js (window.jarvis)
│   ├── vite.config.ts, tailwind.config.js, tsconfig.json, index.html
│   └── src/
│       ├── api/types.ts + client.ts (typed REST + WS)
│       ├── store/kernelStore.ts (Zustand, WS deltas) + windowStore.ts (WM)
│       ├── components/BootScreen, desktop/Desktop, desktop/Taskbar, desktop/Window
│       └── components/windows/ (SystemMonitor, ProcessManager, MemoryViewer, CpuScheduler, EventLog, InterruptCenter, FileExplorer, DeviceManager, IpcLab, Terminal, Placeholder)
├── config/machine.json, clock.json, memory.json, paging.json, scheduler.json, fs.json, device.json, ipc.json, theme.json, voice.json
├── tests/python/test_abi.py (82 tests)
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

**`jvk_command` actions implemented (M0-M8):**
* `ping`, `echo`
* `cpu_load_program {program:[u16]}`, `cpu_reset`, `cpu_step`
* `create_process {name, priority, burst_time|burst|bt, mem_budget|mem_budget_mb}`, `kill_process {pid}`, `suspend_process {pid}`, `resume_process {pid}`, `list_processes`
* `mem_alloc {pid, pages}` (denied with "memory budget exceeded" when the agent's cap would be crossed), `mem_free {pid}`, `mem_read {pid, addr}`, `mem_write {pid, addr, value}`, `mem_config {allocator, replacement, agentBudgetPages, referencedBit}`
* `trigger_interrupt {irq, source, detail}`, `list_interrupts`, `panic {reason}`, `clear_panic`
* `scheduler_config {algo|algorithm: rr|round_robin|fcfs|sjf|priority}`, `list_scheduler` (now includes metrics)
* `fs_*` (15 actions, M5), `list_devices` / `device_set` / `device_write` / `device_read` / `device_flush` / `device_interrupt` / `device_fault` / `device_repair` (M6)
* `ipc_queue_{create,send,recv,destroy}`, `ipc_shm_{create,attach,detach,write,read,destroy}`, `ipc_pipe_{create,open,close,write,read,destroy}`, `ipc_mutex_{create,lock,unlock,destroy}`, `ipc_sem_{create,wait,post,destroy}`, `ipc_deadlock`, `ipc_recover {pid}`, `list_ipc` (M7)
* `shell_exec {line}` → `{ok, output, prompt, cwd, clear, exit, command}`, `list_memory`, `list_filesystem`, `tick {count}`, `shutdown` (M8)

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

---

## 10. Bridge (Backend)

* `backend/main.py` — `lifespan` boots `jvk_init({"boot":True})`, `CORSMiddleware` *, endpoints:
  * `GET /health` → `{ok, kernel: snapshot}`
  * `GET /api/snapshot` → snapshot
  * `POST /api/command` → `jvk_command`
  * `GET /api/tick` → `jvk_tick` + snapshot
  * `GET /api/logs?since=n` → `jvk_logs`
  * `WS /ws` → loop as above, handles client `{"action":"tick"}` and `{"action":"command","payload":{}}`
* `backend/services/kernel_loader.py` — `ctypes CDLL(kernel/jarvis_kernel.dll)`, `c_void_p + string_at` (kernel owns buffers), wrappers `jvk_init, jvk_command, jvk_tick, jvk_snapshot, jvk_logs, jvk_shutdown`.

---

## 11. Frontend (Desktop)

* **Shell:** `electron/main.js` spawns `python -m uvicorn backend.main:app --host 127.0.0.1 --port 8000`, polls `/health` 12s, then `BrowserWindow` (frameless, `backgroundColor #060a14`, `preload.js`), dev loads `http://127.0.0.1:5173` else `dist/index.html`.
* **Build:** `Vite 5 + React 18 + TS 5 + Tailwind 3 + Zustand 4 + Framer Motion 11 + Axios 1.7`. `vite.config.ts` proxies `/api, /ws, /health` to `127.0.0.1:8000`. `tailwind.config.js` defines `jarvis.*` palette.
* **State:**
  * `store/kernelStore.ts` — `snapshot, logs, connected, error, logCursor` + `init` (REST initial + `createKernelSocket` WS push, merges logs, keeps 400), `command`, `doTick`.
  * `store/windowStore.ts` — `windows[11]` with `x,y,w,h,z,open,minimized`, `nextZ`, actions `open/close/focus/move/resize/toggleMinimize`. Defaults: System Monitor + Event Log open.
* **API:** `api/types.ts` mirrors `jvk_snapshot` exactly, `api/client.ts` `fetchSnapshot, fetchHealth, fetchLogs, sendCommand, tick, createKernelSocket` (auto-reconnect, fallback `ws://127.0.0.1:8000/ws` when Vite on 5173).
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
  * `Placeholder` — honest `Subsystem offline — arrives in Mx` for Settings until M10b.
* **Chrome:** `Desktop` (wallpaper + 10 desktop icons, double-click to open) + `Taskbar` (launcher, window buttons, `kernel live` dot + `procs/frames`, `TICK` button, clock, Electron window controls) + `BootScreen` (2.2s phases + click to skip, failsafe 3.5s) + `Window` (drag header, resize handle, Framer Motion enter/exit, focus → `z`).

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
mingw32-make test     # 9/9 PASS
Set-Location ..

# 3. frontend
Set-Location frontend
npm install           # 505 packages
npx tsc --noEmit      # no output = pass
npx vite build        # → dist/index.html + 401KB JS
Set-Location ..

# 4. tests
python -m pytest tests -q  # 82 passed

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

---

## 14. What Is Still Offline (honest placeholders)

* Agent Hub/Studio, AI and Settings — windows show `offline — arrives in M9/M10b/...`. Their kernel modules and APIs will follow the same vertical-slice pattern (kernel + bridge + window goes live together).

---

## 15. Verification Evidence (as of this doc)

* Ubuntu 24.04 WSL: `cd kernel && make clean && make && make test` → `libjarvis_kernel.so`, `PASS: cpu / process / memory / interrupts / filesystem / devices / ipc / shell / system` (9/9)
* `python -m pytest tests -q` → `82 passed` (19 M5 filesystem + 19 M6 devices + 9 machine/metrics/budget/config tests + 12 M7 IPC + 4 M8 shell + bridge/ABI)
* `cd frontend && npm run typecheck && npm run build` → clean (474 modules, 401KB JS / 22.8KB CSS)
* Live `uvicorn` → `health ok`, `GET /api/config`, `fs_create/fs_read`, `device_write/device_read`, `device_flush`, `device_interrupt`, IPC queue/SHM/pipe/mutex/semaphore + deadlock recover, `shell_exec`, machine snapshot + budget denial, flat + nested command payloads, `WS` pushes `snapshot + logs` every 2.5s.

---

*End of handover — this doc + `TEAM_OVERVIEW.md` + `FRONTEND_SPEC.md` is the complete picture.*
