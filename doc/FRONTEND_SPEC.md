# JARVIS OS — Frontend Specification

Version: 1.3
Date: 2026-10-24
Status: **Adopted** — canonical visual/UX target for all frontend milestones (M11a onward) — *M11a–M11b live; all 21 screens implemented as of 2026-10-24*

---

## 1. Purpose & Philosophy

The frontend is **the window through which the user sees and interacts with the operating
system**. JARVIS OS does not hide its internals the way Windows does — it *exposes* them:
processes, scheduler decisions, page tables, interrupts, IPC and I/O are all visible while
the system runs.

Non-negotiable rules:

1. **Every number on screen comes from real kernel state** via the bridge.
   `React → OS API/Bridge → Kernel → Subsystems → System State → React UI`. Never fake data,
   never placeholder numbers dressed up as real ones.
2. **Every action goes through the kernel.** Clicking `TERMINATE` sends a command; the kernel
   locates the PCB, releases resources, updates queues, logs the event — and the UI renders
   the *new* state pushed back. The UI never mutates its own copy of OS state.
3. **Unbuilt subsystems render an honest offline state** ("subsystem offline — arrives in Mx"),
   never simulated content.
4. Animations must **explain**, not decorate (e.g. a process visibly hopping queues on a
   context switch).

## 2. Technology Stack

| Layer     | Choice |
| --------- | ------ |
| Shell     | **Electron** (confirmed 2026-07-24) — frameless fullscreen window, spawns uvicorn, waits `/health`, boot animation while waking |
| Build     | Vite + TypeScript |
| UI        | React + TailwindCSS + shadcn/ui |
| State     | Zustand (stores fed by WebSocket push, never polling) |
| Motion    | Framer Motion |
| Transport | REST for commands (`POST /api/command`), **WebSocket `/ws`** for live events/state deltas |

## 3. Data Flow (the only allowed path)

```
                    JARVIS UI (Electron + React)
                       │
             REST (commands) · /ws (live state)
                       │
                       ↓
                FastAPI Bridge (ctypes)
                       │
                       ↓
                 C Kernel (jarvis_kernel.dll)
                       │
       ┌───────────────┼────────────────┐
       ↓               ↓                ↓
   Processes        Memory          Scheduler
       │               │                │
      PCBs         Page Tables      Ready Queue
       └───────────────┼────────────────┘
                       ↓
                  System State ──push──▶ React UI
```

Example trace — "Terminate Coding Agent":

```
User clicks TERMINATE
  → POST /api/command {action:"kill_process", pid}
  → Kernel locates PCB → removes from queue → TERMINATED record → slot freed
  → Scheduler updated → PROCESS_KILLED logged
  → /ws pushes new snapshot delta → UI re-renders
```

---

## 4. Screen Inventory (21 screens)

Status legend: LIVE at mid-sem demo · PARTIAL at mid-sem · ARRIVES-WITH-MILESTONE later

| #  | Screen                       | Status | Backed by |
| -- | ---------------------------- | ------ | -------------------------------------------- |
| 1  | Boot Screen                  | **LIVE (M11a/M11b)** | `/health`, uvicorn lifecycle, failsafe boot → login → desktop |
| 2  | Command Center / System Monitor | M11a   | snapshot cpu/clock/processes/queues + `machine` panel (M2) + scheduler metrics (M3a) + `/ws` events |
| 3  | Agent Manager                | **LIVE (M9b)** | Agent Hub: live registry, launch/stop, task queue + pipeline |
| 4  | Agent Workspace              | **LIVE (M9b)** | Agent Console per pid: task dispatch, trace, report, queue/history, ticks |
| 5  | Create Agent                 | **LIVE (M9b)** | Agent Studio form → SQLite profile → kernel registry → process launch |
| 6  | Agent Library                | **LIVE (M9b)** | 9 built-in + custom agents in Agent Hub/Studio |
| 7  | Process Manager              | **LIVE (M11a)** | PCBs, queues, lifecycle commands, MEM column (`mem_pages`/`mem_budget_pages`) |
| 8  | CPU & Scheduler              | **LIVE (M4.5)** | clock/quantum + RR/FCFS/SJF/Priority live, `RUNNING/NEXT` + switches + CPU util/avg wait/avg turnaround |
| 9  | Memory Manager               | **LIVE (M11a)** | frames, page tables, faults, per-agent budgets + page-table policy |
| 10 | IPC & Synchronization        | **LIVE (M7)** | queues, shared memory, pipes, mutex/sem, wait-for graph + deadlock recovery |
| 11 | File Explorer                | **LIVE (M5)** | virtual disk + inode tree, CRUD, permissions, open-file table |
| 12 | I/O & Device Manager         | **LIVE (M6)** | 8 devices, driver states, I/O dispatch, fault/repair, disk buffer + flush |
| 13 | Spooling & Buffer Monitor    | **LIVE (M6)** | printer spool + keyboard/display buffers + disk write-back buffer in Device Manager |
| 14 | Interrupt Center             | **LIVE (M4)** | priority queue + panic + page-fault wiring, 7 IRQs (incl. `context_switch`) + error manager |
| 15 | Kernel Monitor               | **LIVE (M11a)** | live event stream; enriched by every milestone |
| 16 | System Logs                  | **LIVE (M11a/M10b)** | `jvk_logs`; live stream, filter and notification derivation |
| 17 | Terminal / JARVIS Shell      | **LIVE (M8)** | kernel shell (`shell_exec`), real prompt/cwd, history, proc/mem/fs/ipc commands |
| 18 | Device/Hardware View         | **LIVE (M6)** | virtual hardware inventory in Device Manager |
| 19 | System Settings              | **LIVE (M10b/M11b)** | config read/write over `config/*.json`, scheduler/memory persistence, live shell accents/font |
| 20 | System Information           | **LIVE (M11b)** | machine, kernel, memory, storage, agents, interrupts via snapshot + `/api/system/info` |
| 21 | Shutdown / Restart Screen    | **LIVE (M11b)** | kernel shutdown/restart + real halted overlay, reopen and power-off controls |

**All 21 canonical screens are implemented as of 2026-10-24 (M11b complete).** The desktop adds a single-user
login (`admin` / `jarvis` on first boot), pinned Dock, Ctrl+K universal search, kernel-log notifications/toasts,
quick-settings tray, window snapping and live theme accents. Full light mode remains post-MVP polish.

---

## 5. Navigation — Left Menu (app launcher)

| #  | Menu item       | Opens screen(s) |
| -- | --------------- | ---------------- |
| 1  | Command Center  | #2 home dashboard |
| 2  | Agents          | #3 Agent Manager (#4–6) |
| 3  | Processes       | #7 Process Manager |
| 4  | CPU & Scheduler | #8 |
| 5  | Memory          | #9 |
| 6  | Files           | #11 File Explorer |
| 7  | IPC & Sync      | #10 |
| 8  | I/O & Devices   | #12, #13, #18 |
| 9  | Interrupts      | #14 |
| 10 | Terminal        | #17 |
| 11 | System Logs     | #16 (+ #15 Kernel Monitor) |
| 12 | Settings        | #19–21 |

At mid-sem the menu is fully visible; items without subsystems launch windows showing the
honest offline panel.

---

## 6. Screen Specifications

### 6.1 Command Center (#2)
Home screen = desktop overview (Windows desktop + Task Manager combined):

```
JARVIS OS
--------------------------------
CPU              24%        <- real: instructions executed / clock budget per window
Memory           42%        <- real: frames allocated / total (M3b)
Processes        18         <- real: live process count
Running / Ready / Blocked / Suspended   <- real: queue lengths
Active Agents    7/10       <- live: agents.stats.live / config.max_live
System Uptime    02:14:36   <- real: uptime_ticks
Recent Events               <- real: /ws log stream
- Coding Agent started
- Finance Agent allocated memory
- Context switch
```

Interactions: click agent -> workspace; click process -> details; click CPU/memory ->
respective screens; launch/pause/resume/stop agents from Agent Hub. Taskbar shows the
live agent count and every open console.

### 6.2 Agent Manager (#3)
List of installed agents with PID, state, CPU%; actions START / PAUSE / RESUME / TERMINATE /
change priority. START performs the full real pipeline:

```
Create Process -> Create PCB -> Allocate Memory -> Assign PID -> Ready Queue
```

State transitions shown are the kernel's own: RUNNING->SUSPENDED (pause),
SUSPENDED->READY (resume), any->TERMINATED. Includes `+ Create Agent` (name, type, rules,
capabilities, priority, memory limit) -> Agent Studio (M9b).

### 6.3 Process Manager (#7)
Task-manager-for-JARVIS. Rows are real PCBs — never decorative entries.

Columns: PID / Name / Owner-agent / State / Priority / CPU time / Memory / Runtime / Created-at.

```
PID 101  Coding Agent   RUNNING   Priority HIGH  Mem 128 KB
```

Click a row -> Process Details (state, priority, cpu time, memory, parent) with
`[PAUSE] [CHANGE PRIORITY] [TERMINATE]` — each firing a real `jvk_command`
(suspend_process / resume_process / kill_process).

### 6.4 CPU & Scheduler (#8)

```
CURRENT PROCESS: <name, pid>     <- when per-process execution lands (M9);
                                    before that: idle/halt status shown honestly
READY QUEUE: [Research] [Finance] ...   <- real queue contents
ALGORITHM: Round Robin - Quantum <n>    <- real config; algorithm SWITCHING is
                                           scheduler-strategy work (post-M11a)
TIMELINE: Coding -> Research -> Finance -> Coding ...  <- replay of real
                                    SCHEDULE/context-switch events
```

### 6.5 Memory Manager (#9)

```
PHYSICAL MEMORY (frames)
Frame 0  Coding Agent   Frame 1  Coding Agent   Frame 2  Research Agent
Frame 3  FREE           Frame 4  Finance Agent  Frame 5  FREE ...

VIRTUAL -> PHYSICAL
Virtual Address -> Page Table -> Physical Frame   <- live translation view

PER-PROCESS: Allocated pages - Frames held - Page Faults   <- real M3b counters
```

Visualizes allocation/deallocation, free frames, page tables, faults, swap in/out.

### 6.6 File Explorer (#11) — LIVE (M5)
Familiar tree + file list over the virtual filesystem. Operations: create/open/read/write/
rename/delete/copy/move/search folder+file. Implemented: path bar with `..`, sortable entry
list, in-place text editor with save, readonly toggle, name search, disk/inode/open-file
stats. Every row comes from `jvk_snapshot().filesystem`; all actions are `fs_*` kernel commands.
Path: `React -> FastAPI -> Kernel -> VFS -> virtual disk blocks`.

### 6.7 IPC & Synchronization (#10) — LIVE (M7)

```
FROM             TO               MESSAGE
Research Agent -> Coding Agent     DATA_READY
Finance Agent  -> Report Agent     REPORT_READY

MUTEX ProjectFile:      Coding=LOCKED  Research=WAITING
SEMAPHORE FileAccess:   avail 0, waiting [PID 108, PID 112]
```

Implemented as the **IPC & Sync Lab**: live object lists for queues (`count/capacity`, blocked
senders/receivers), shared-memory segments (`data`, maps, attached pids), pipes (readers/writers
+ waiters), mutexes (`owner`, recursion, contention, waiters) and semaphores (`value/max`,
holders, waiters); a wait-for graph panel with edges + detected cycles and DETECT / RECOVER
buttons; and an operations console for every `ipc_*` action with pid selection and nonblock
toggle. Every value comes from `jvk_snapshot().ipc`; recovery kills the chosen process through
the kernel.

### 6.8 I/O & Devices (#12) + Spooling Monitor (#13) + Hardware View (#18) — LIVE (M6)
Device list with states (connected/busy/waiting/disconnected/fault/disabled); I/O request chain
visualized end-to-end: `Agent -> I/O request -> device -> waiting -> complete -> READY`;
print/buffer queues from the spooler; virtual hardware inventory. Implemented in one window:
8 devices with power/state controls, fault injection + repair, per-device driver panels
(keyboard inject/read, mouse move, display buffer, printer spool + print-one IRQ, clock timer
IRQ, disk status + write-back buffer with an explicit FLUSH action), plus cumulative
read/write/flush/IRQ/error counters. The board identity shown in the System Monitor MACHINE
panel (board/chipset/bus/BIOS/POST + slots) comes from the M2 machine model.

### 6.9 Interrupt Center (#14)

```
IRQ              SOURCE           STATUS
page_fault (1)   Memory           SERVICED
disk (2)         Device           SERVICED
context_switch (6) Scheduler      SERVICED
```

Live servicing sequence: interrupt -> save state -> ISR -> service -> restore -> continue.
Demo controls generate test interrupts and watch the priority ordering handle them.

### 6.10 Terminal (#17) — LIVE (M8)

```
jarvis@vos:/$ ps
PID  NAME                 STATE      PRI  BURST  CPU  MEM
1    worker               READY      5    4      0    0/64
jarvis@vos:/$ kill worker
kill pid=1 (worker)
```

Implemented as a kernel-backed terminal: prompt and cwd come from the `shell` snapshot,
↑/↓ history, ctrl+l clear, quick-command buttons, and every line is sent as
`shell_exec {line}`. Commands: `help, echo, clear, pwd, cd, version, whoami, exit, shutdown,
ps, kill, suspend, resume, memory/mem, alloc, free, write, read, sched, tick, devices, ipc,
send, recv, files, ls, mkdir, touch, cat, write-file, rm, mv, cp, chmod`.

### 6.11 System Logs (#16) + Kernel Monitor (#15)

```
10:42:18  Process 105 -> READY
10:42:05  Context switch PID 101 -> 102
10:42:01  Memory allocated to PID 101
```

Searchable/filterable by process, event type, time; errors highlighted. Kernel Monitor is the
live "inside-the-OS" feed of syscalls, context switches, memory ops, interrupts.

### 6.12 Settings (#19) / System Info (#20) / Shutdown (#21)
Settings edits real configuration (scheduler algorithm, quantum, agent defaults, theme,
virtual RAM size, logging) persisted to `config/*.json` via backend endpoints.
System Info reads snapshot (kernel version, uptime, arch, stats).
Shutdown/restart runs the controlled kernel sequence with UI confirmation screen.

---

## 7. Milestone <-> Screen Map (vertical slices)

From M4 onward **every kernel milestone ships its UI window in the same milestone**:

| Milestone              | Screens that go live |
| ---------------------- | -------------------- |
| M3b Memory Manager     | data for #9 |
| M11a Frontend Foundation | #1, #2, #7, #8(partial), #9, #15(seed), #16 |
| M4 Interrupts          | #14 |
| M5 Filesystem          | #11 |
| M6 Devices             | #12, #13, #18 |
| M7 IPC                 | #10 (IPC & Sync Lab) |
| M8 Shell               | #17 (Terminal), #21 command side |
| M9a/M9b Agents         | #3, #4, #5, #6 |
| M10b API full          | #19 (settings persistence), mature #15/#16 — LIVE |
| M11b Desktop completion | login, snap/dock/search/tray/notifications, #20, #21, live theme accents — LIVE |

## 8. Display Conventions & Reconciliations

1. **BLOCKED vs WAITING** — kernel enum uses `WAITING`; UI displays `BLOCKED`. Same concept,
   decided at the render layer (kernel rename optional during M3b).
2. **CPU % truthfulness** — the scheduler now reports real metrics (M3a: `cpu_utilization`,
   `avg_wait`, `avg_turnaround`, `completed`, busy ticks) computed from actual scheduled quanta;
   idle/halted states are still shown honestly, and no number is invented by the UI.
3. **Memory units** — simulator counts pages/frames. Convention: `PAGE_SIZE = 4096 bytes`;
   UI derives KB/MB (`pages x 4 KB`). Total RAM displayed = configured frames x page size —
   never a hardcoded "4 GB".
4. **PIDs** — kernel generates from 1; spec examples showing 101+ are cosmetic only.
5. **Priority** — kernel stores integers; UI maps to LOW/MED/HIGH labels for display;
   priority input converts back on command.
6. **Scheduling algorithm selection** — FCFS/RR/SJF/Priority is real as of M4.5
   (`scheduler_config {algo}`), driven by the strategy pattern in the kernel.

## 9. Contingency

If the desktop is not demo-ready ~5 days before the review: build the bare monitor page
(old M3c idea — polls `/health` + `/api/logs`) in one evening as the fallback demo UI.
Never scheduled as a milestone; exists only as written insurance.