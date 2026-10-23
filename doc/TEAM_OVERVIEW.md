# JARVIS OS — Team Overview & Project Status

Version: 1.5 · Date: 2026-10-23

This document is the single overview for the team: what we are building, where the project stands, the step-by-step implementation roadmap, the apps and agents that will exist, and how the final demonstration will run. It is the human-friendly companion to `IMPLEMENTATION_PLAN.md`, `ARCHITECTURE.md`, `PROJECT_SPEC.md`, and `AGENTS.md`.

---

## 1. What JARVIS OS Is

JARVIS OS is a fully software-based educational operating system simulator that manages **AI agents as processes**. It is not a Linux distro, not a Windows clone, and not an Electron app with random windows.

- The "programs" the OS runs are simulated AI agents (Finance, Coding, Research, and user-created ones).
- Each agent runs as a **real simulated process**: it is scheduled on the virtual CPU, allocates virtual memory, waits on I/O, and communicates through the kernel — all visualized on the desktop.
- No language model runs inside an agent. Agent behaviour is a **deterministic simulation**, so the project runs on any laptop.
- The whole thing feels like a complete desktop OS from the moment it boots.

**Core rule:** the C kernel is the only authority. Every request passes through the kernel; the UI only renders what the kernel reports.

---

## 2. Tech Stack (one line each)

| Layer | Technology |
|---|---|
| Kernel | C (C17), compiled to `jarvis_kernel.dll` |
| Low-level proof point | x86-64 Assembly (NASM) — context-switch save/restore stub |
| Kernel ABI | cJSON (vendored), JSON strings across the boundary |
| Bridge | FastAPI (Python) + ctypes + Pydantic |
| Frontend | Electron · React · TypeScript · Tailwind · Framer Motion · shadcn/ui · Zustand |
| Live updates | WebSocket (kernel events pushed to the UI, no polling) |
| AI reasoning (post-MVP) | Rule-based local AI + Gemini (reasoning only) |
| Database | SQLite + SQLAlchemy (persistent metadata only — never runtime state) |

---

## 3. Implementation Roadmap & Status

**Legend:** ✅ Completed · ▶ Next up · ○ Pending

### Phase 1 — MVP: a clickable OS for AI agents (we build this first)

| # | Milestone | What it delivers | Status |
|---|---|---|---|
| M0 | Project Skeleton | Repo layout, config files, 4 aligned docs, FastAPI bridge skeleton | ✅ |
| M1 | Toolchain & Kernel Build | C kernel → `jarvis_kernel.dll` (gcc + NASM), ctypes bridge, JSON ABI, 16-test pytest suite | ✅ |
| M2 | Machine, CPU & Clock | Virtual motherboard model (`machine/`: board, chipset, bus, BIOS, POST, RAM/CPU/device slots from `config/machine.json`), registers + ALU, fetch-decode-execute loop, configurable clock + timer interrupt, NASM context-switch stub, round-robin scheduler stub | ✅ |
| M3a | Process Manager | PCB, PID generator, ready/waiting/suspended/terminated queues, lifecycle state machine, suspend/resume/kill, real register+PC context switching, per-tick scheduling metrics (CPU utilisation, avg wait, avg turnaround, completed) | ✅ |
| M3b | Memory Manager | Frames + page tables, virtual memory with paging, MMU translation, allocators (first/best/worst fit), replacement (FIFO/LRU/Clock), swap in/out, page faults, segfault detection, **per-agent page budgets** (`agentBudgetPages`/`agentMemoryLimit`, denied + logged when exceeded) — all config-driven | ✅ |
| M11a | Frontend Foundation (Electron) | Electron+Vite+React scaffold, WebSocket `/ws` push channel, desktop shell + window manager basics, taskbar/clock; 5 real-state windows: System Monitor, Processes, Memory Viewer, Event Log — honest "subsystem offline" placeholders, never fake data | ✅ |
| M4 | Interrupt Controller & Error Manager | Priority interrupt queue, ISR registry (incl. the lowest-priority `context_switch` line raised on every real pid change), error recovery, simulated panic flag — kernel never crashes; wires M3b page faults; Interrupt panel goes live | ✅ |
| M4.5 | Scheduler Strategy (Round-Robin + Priority/SJF/FCFS) | Strategy pattern: RR/FCFS/SJF/Priority selectable at runtime, AT/BT/PRIO per process, `scheduler_config` ABI, `RUNNING/NEXT` in snapshot, PM rotation only for RR | ✅ |
| M5 | File System & Disk | Virtual disk, directory/file tree, CRUD, permissions, open-FD table — never touches real Windows paths; geometry from `config/fs.json`; File Explorer goes live | ✅ |
| M6 | Device Manager & Drivers | Common device interface, keyboard/mouse/display/printer/disk/clock drivers, I/O queues, buffers, printer spooler, disk write-back buffer (`device_flush`), `config/device.json` enable/disable + IRQ rewiring; Device Manager goes live | ✅ |
| M7 | IPC & Synchronization | Message queues, shared memory, pipes, mutex + semaphore, deadlock detection/recovery, live wait-for graph; IPC Lab goes live | ✅ |
| M8 | Shell & CLI | Shell + parser (`ps`, `kill`, `memory`, `files`, `mkdir`, `ls`, `ipc`, `shutdown`, …) proxied through the kernel; Terminal goes live | ✅ |
| M9a | Agent System Core | Agent Simulator in the kernel (Plan → Think → Act → Report), agent registry, task queues; agents run as real processes | ✅ |
| M9b | Agent Consoles & System Tools | 9 built-in agents, live per-pid consoles, Agent Hub, Agent Studio with SQLite persistence; existing process/memory/CPU/file/device/IPC tools stay live (Settings persistence remains M10b) | ✅ |
| M10b | Backend API (full) | Pydantic schemas, REST routers for all subsystems, full WebSocket channels pushing live kernel events | ▶ |
| M11b | Desktop Completion | Boot → login → JARVIS desktop polish, window snapping, dock, search, notifications, tray, clock — **the clickable OS MVP lands here** | ○ |

### Phase 2 — Post-MVP (AI + polish, after the clickable OS)

| # | Milestone | What it delivers | Status |
|---|---|---|---|
| M12 | AI Layer & Gemini | Rule-based local AI; Gemini only for explain-state / automation / health & session summaries; plans execute strictly via kernel commands | ○ |
| M13 | Testing, Hardening & Packaging | Google Test per C module, pytest integration, Playwright e2e, crash isolation, `electron-builder` → `jarvis.exe` | ○ |
| M14 | Documentation & Release | Final docs pass, recorded demo, release build | ○ |
| M15 | Future Enhancements | Ideas beyond core scope. *Voice ("Hey JARVIS") was cut from scope on 2026-07-24.* | ○ |

---

## 4. Apps We Will Have

The desktop is an OS, so everything opens in a movable, resizable window.

**System tools (9)**
1. File Explorer — browse the virtual filesystem
2. Task Manager — live list of all processes (the real PCB table)
3. Memory Viewer — page tables, frames, allocation in real time
4. CPU Monitor — registers, PC, instruction execution, utilization graphs
5. System Monitor — the live dashboard for any process/agent
6. Device Manager — connected devices and driver states
7. Terminal — the kernel shell
8. IPC & Sync Lab — message queues, semaphores, deadlock demos
9. Settings — scheduler, memory, theme (changes take effect instantly, no restart)

**Agent apps**
- **Agent Hub** — browse and launch agents
- **Agent Studio** — create and save custom agents (name, role, tools, memory budget)
- **Agent Consoles** — one window per running agent (Finance console, Coding console, …)

**OS chrome (not apps):** boot screen, login, taskbar, dock, search, notifications, tray, clock.

Total: **12 distinct apps** (9 system tools + Agent Hub + Agent Studio + the consoles), and consoles multiply for each running agent.

---

## 5. Agents We Will Have

**9 built-in agents:**
Finance · Coding · Research · Writing · HR · Legal · Marketing · Travel · Health

**Plus unlimited user-created** agents via Agent Studio. A custom agent registers in the Agent Hub and launches as a process exactly like a built-in one.

Each agent, when launched:
- Gets a PID and a PCB,
- Is scheduled on the virtual CPU,
- Allocates virtual memory (page table + frames),
- Runs its deterministic Plan → Think → Act → Report pipeline,
- Reports results back to its console.

---

## 6. The Final Demonstration (script)

This is the highlight-reel demo we will show faculty.

1. **Boot** — JARVIS OS boots with its boot screen (<5s), then login, then the futuristic desktop.
2. **Launch an agent** — open the **Agent Hub**, click the **Finance Agent** (or Coding/Research). It spawns as a real process: console window opens, PID assigned, queued, scheduled, memory allocated.
3. **The dashboard moment** — open the **System Monitor** (or "Details" on the agent) for a **live per-process dashboard** of that exact agent:
   - **CPU**: fetch-decode-execute live — register values, PC, instruction executing now
   - **Memory**: page table + frames for the process, virtual vs physical addresses, page faults as they happen
   - **Scheduler**: queue position, time slice, context-switch count, when it gets the CPU
   - **Process**: PID, state, priority, open files, I/O waits
   - **Event log**: scrolling kernel events (`PROCESS_CREATED`, `MEMORY_ALLOCATED`, `PAGE_FAULT`, `CONTEXT_SWITCHED`, `TIMER_INTERRUPT`)
   - **Live graphs**: CPU utilization / memory curves
4. **Proof it is real** — open **Task Manager** beside it to show *multiple* agents running as processes simultaneously. Everything on screen is genuinely from the kernel (via `jvk_snapshot`/`jvk_logs` pushed over WebSocket). Nothing is hardcoded.
5. **If faculty dig deeper** — Memory Viewer (paging), Device Manager (drivers), IPC & Sync Lab (deadlock demo), Terminal (kernel shell commands), Agent Studio (create a custom agent live).

**The selling point:** every number on screen — registers, page tables, queue states, context switches — is real kernel state, not animation. Animations only *explain* (e.g. a page fault visually walks fault → interrupt → memory manager loads page → resume).

---

## 7. Where We Are Right Now

- ✅ **M0, M1, M2, M3a complete** — kernel builds as `jarvis_kernel.dll`, bridge works, CPU + scheduler + full process lifecycle verified.
- ✅ **M3b Memory Manager complete** — frame table, per-process page tables, MMU translation with segfault detection, first/best/worst-fit placement (config-driven), FIFO/LRU/Clock replacement (runtime-switchable), swap out/in with data preservation, external-fragmentation fallback, `mem_alloc/mem_free/mem_read/mem_write/mem_config` ABI commands, live snapshot (`frame_map`, `page_tables`, stats). Verified: 3 C smoke tests pass, 32 pytest tests pass, live ctypes round-trip confirmed.
- ✅ **M11a Frontend Foundation complete** — Electron main process (spawns uvicorn, waits `/health`, frameless fullscreen, boot animation), Vite+React+TS+Tailwind+Zustand+Framer Motion scaffold, typed REST client + WebSocket `/ws` push channel (snapshot deltas every ~0.9s + auto-tick), window manager (open/close/move/resize/focus/z-order), taskbar with kernel status + clock, 5 live windows (System Monitor, Process Manager, Memory Viewer, CPU & Scheduler, Event Log) rendered purely from `jvk_snapshot`/`jvk_logs` — no fake data, offline subsystems show honest placeholders. Verified: `vite build` (352KB JS, 19KB CSS), live uvicorn integration (create/alloc/read/write/suspend/resume/kill + memory free, WS push, tick, logs all real), 32 pytest + 3 C smokes still green.
- ✅ **M4 Interrupt Controller & Error Manager complete** — priority queue (shutdown 0 > page_fault 1 > disk 2 > keyboard 3 > timer 4 > software 5), ISR registry (one-at-a-time, 6 handlers), queue cap 32 + dropped counter, panic flag + reason, page faults wired via trampoline (`PAGE_FAULT → IRQ_PAGE_FAULT`), timer wired in `jvk_tick`, drain-all-then-schedule with panic-freeze, error manager (4 categories + counts, system→panic), snapshot `interrupts{queue,pending,handled,dropped,panic}` + `error_manager`. ABI: `trigger_interrupt`/`list_interrupts`/`panic`/`clear_panic`. Vertical slice: Interrupt Center live (queue table, panic banner, trigger buttons, error counts). Verified: 4 C smokes PASS, 36 pytest PASS, live WS priority-order (page_fault beats software), panic round-trip, `vite build` 469 modules 358KB JS.
- ✅ **M4.5 Scheduler Strategy complete** — PCB extended with `burst_time` (BT) + `priority` (PRIO) + `created_ticks` (AT), `scheduler` strategy pattern (RR/FCFS/SJF/Priority) selectable via `scheduler_config {algo}`, `scheduler_register_ex` stores per-process attrs, `scheduler_schedule` picks correctly (RR rotates, FCFS earliest AT, SJF shortest BT, Priority highest PRIO), snapshot `scheduler{algo,current,next,switches}` + `process_list{burst_time}`, tick rotates PM only for RR, panic freezes scheduling, `RUNNING/NEXT` pills in UI. Frontend: `Process Manager` now has PRIO/BT inputs + AT/BT columns, `CPU & Scheduler` has RR/FCFS/SJF/Priority switch. Verified: 4 C smokes PASS, 40 pytest PASS (3 new scheduler tests), live priority/SJF/FCFS ordering, `vite build` 469 modules 360KB JS.
- ✅ **M5 File System & Disk complete** — in-memory virtual disk (contiguous first-fit, configurable blocks/block size), 128-inode tree with directories/files, path resolution incl. `.`/`..`, permissions (`rwx` owner/other + readonly), open-file table (fd≥3, owner/cursor/mode), CRUD + rename/copy/move/recursive copy/search, seeded `/System /Agents /Users /Temp /Logs /Swap` tree, 15 `fs_*` ABI actions, `filesystem` snapshot (config/stats/disk/entries/open_files), kernel closing a process's FDs on kill. Vertical slice: **File Explorer** live (path bar, create/rename/delete, in-place editor with save, readonly toggle, search, disk/inode stats) — every number from `jvk_snapshot`.
- ✅ **M6 Device Manager & Drivers complete** — common `jvk_driver_ops_t` interface + device manager (registry cap 16, power/state control, IRQ delivery, stats, error reporting), six drivers: keyboard input buffer, mouse coordinates, display line buffer, printer spool queue (bounded, printed per IRQ), disk transfer counters, clock hardware ticks; 8 registered devices (CPU/RAM/Disk/Keyboard/Mouse/Display/Printer/Clock) mapped to real IRQ lines and forwarded through the ISR registry; 7 `device_*`/`list_devices` ABI actions and `devices` snapshot with per-device `info`. Vertical slice: **Device Manager** live (state/power controls, fault/repair, keyboard inject+read, mouse move, display buffer, printer spooling, clock timer IRQ).
- ✅ **M0–M6 spec-gap closing complete (2026-10-21)** — new `kernel/machine/` motherboard model (board/chipset/bus/BIOS/POST + CPU/RAM/PCI slots) driven by `config/machine.json` and serialised as the `machine` snapshot object; scheduler metrics (`scheduler_account`/`scheduler_metrics`: CPU utilisation, avg wait, avg turnaround, completed, busy ticks) charged every `jvk_tick` and exposed in `scheduler`; per-agent memory budgets (`MM_ERR_BUDGET`, `mm_set_budget`/`mm_budget_of`/`mm_pages_of`, `pm_set_budget`, `create_process {mem_budget|mem_budget_mb}`, denial logged as `MEM_BUDGET_DENIED`, per-process `mem_pages`/`mem_budget_pages` and per-table `used`/`budget`); paging policy honoured from config (`pageTableStrategy`, `presentBit`, `dirtyBit` — clean pages dropped, `referencedBit` — false degenerates Clock→FIFO); lowest-priority `context_switch` IRQ (priority 6) raised on every real pid change and counted in the snapshot; `config/fs.json`, `config/device.json`, `config/machine.json` + `backend/services/config_loader.py` (merges and normalises `config/*.json` into the `jvk_init` payload, strips `_comment`), devices can be disabled or rewired from config; disk driver write-back buffer (8 blocks) with `device_flush` and `buffered`/`flushes` info; new `tests/test_system.c` integration smoke + 9 new pytest cases; `GET /api/config`. UI: System Monitor gains a MACHINE panel and live scheduler metrics, Process Manager a MEM column, Memory Viewer budget lines.
- ✅ **M7 IPC & Synchronization complete** — `kernel/ipc/` message queues (blocking sender commit + per-pid receiver delivery), shared-memory segments with attach checks, named bounded pipes, recursive mutexes with unlock handoff, counting semaphores with permit handoff, wait-for graph + DFS cycle detection (deduped signatures), `IPC_DEADLOCK` observer event/software IRQ, `ipc_recover` through the kernel kill hook. 26 IPC actions + `list_ipc`; `config/ipc.json` seeds `agent-bus`, `shared-board`, `console-pipe`, `log-lock`, `worker-slots`; new `ipc` error category and snapshot object. Vertical slice: **IPC & Sync Lab** live (objects, waiters, graph, cycles, DETECT/RECOVER, full operations console).
- ✅ **M8 Shell & CLI complete** — `kernel/shell/` tokenizer + path normaliser + 30 commands (proc/mem/sched/devices/ipc/fs), executed by re-entering `jvk_command` through `shell_exec` (no direct subsystem access); new `list_memory`, `list_filesystem`, `tick {count}`, `shutdown` actions; shared kill helper frees process/scheduler/memory/files/IPC. Vertical slice: **Terminal** live (real prompt/cwd, history, quick commands, clear/exit).
- ✅ **M9a Agent System Core complete** — `kernel/agent/` registry (24 definitions, 9 built-in seeded from `config/agents.json`) and deterministic task engine; every task advances `QUEUED → PLANNING → THINKING → ACTING → REPORTING → DONE` only while its pid is the scheduler-selected process. Launch creates a real process (`pm_create` + scheduler registration + memory budget/allocation) and a `/Agents/<key>` workspace; ACT runs simulated tool effects (budget math, VFS research, code/draft generation, HR/legal/marketing/travel/health checks), may grow memory by up to 2 pages, writes drafts, and stores `report-<task>.md` in the VFS. ABI: `list_agents`, `agent_list`, `agent_tasks`, `agent_define`, `agent_remove`, `agent_launch`, `agent_task`, `agent_cancel`, `agent_kill`; snapshot `agents{config,stats,definitions,instances,tasks}`; kill hook keeps the agent registry consistent. C smoke `test_agents` PASS.
- ✅ **M9b Agent Consoles & System Tools complete** — **Agent Hub** (live stats, 9 built-in + custom definitions, per-agent launch/stop, task queue and pipeline progress), **Agent Studio** (name, role/system prompt, personality, colour, icon, ≤4 simulated tools, priority, memory budget; create/edit/delete persisted in SQLite via `/api/agents`, re-registered at boot), and dynamic **Agent Consoles** keyed `agent-<pid>` (task dispatch with title/input, queue/history, PLAN/THINK/ACT/RESULT/REPORT trace, CPU/memory, +1/+10 ticks, cancel, terminate). Window manager now supports arbitrary dynamic window ids; taskbar lists live consoles. Verified with a real TestClient round-trip: launch finance → queue task → tick to `DONE 100%` → 355-char report, profile persist.
- ✅ **Verified on Ubuntu 24.04 WSL (2026-10-23):** `make clean && make && make test` → `libjarvis_kernel.so`, **10/10 C smoke tests PASS**; `python -m pytest tests -q` → **90 passed**; `npm run typecheck` + `npm run build` → clean (477 modules, 426KB JS); live kernel round-trip for agent launch/task/report and profile persistence.
- ▶ **Next milestone: M10b Backend API (full)** — Pydantic schemas and REST/WS surface for the remaining subsystems; then M11b desktop completion (boot → login → notifications/search/tray) lands the clickable OS MVP.
- The vertical-slice ladder stays in force: future kernel milestones ship their UI window in the same milestone.
- The order matters: kernel core first (processes → memory → interrupts → filesystem → devices → IPC → shell), then the agent layer, then the clickable desktop completion (M11b MVP), then AI on top. Voice was cut from scope on 2026-07-24.
