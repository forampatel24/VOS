# JARVIS OS — Frontend Specification

Version: 1.0
Date: 2026-07-24
Status: **Adopted** — canonical visual/UX target for all frontend milestones (M11a onward)

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
| 1  | Boot Screen                  | M11a   | `/health`, uvicorn lifecycle |
| 2  | Command Center               | M11a   | snapshot cpu/clock/processes/queues + `/ws` events |
| 3  | Agent Manager                | M9b    | agent registry (M9a) |
| 4  | Agent Workspace              | M9b    | agent process + task pipeline (M9a) |
| 5  | Create Agent                 | M9b    | Agent Studio → process creation (M9a) |
| 6  | Agent Library                | M9b    | built-in + custom agents (M9a) |
| 7  | Process Manager              | M11a   | **kernel done (M3a)** — PCBs, queues, lifecycle commands |
| 8  | CPU & Scheduler              | M11a*  | clock/quantum/RR exist; usage + ready queue live; *algorithm switching waits for scheduler-strategy work* |
| 9  | Memory Manager               | M11a   | M3b (frames, page tables, faults) |
| 10 | IPC & Synchronization        | M7     | msg queues, shared mem, pipes, mutex/sem |
| 11 | File Explorer                | M5     | virtual filesystem |
| 12 | I/O & Device Manager         | M6     | device manager + drivers |
| 13 | Spooling & Buffer Monitor    | M6     | printer spooler, I/O buffers |
| 14 | Interrupt Center             | M4     | interrupt controller (panel live same milestone) |
| 15 | Kernel Monitor               | M11a*  | starts as live event stream; enriches every milestone |
| 16 | System Logs                  | M11a   | `jvk_logs` (exists since M1); search/filter matures M10b |
| 17 | Terminal / JARVIS Shell      | M8     | shell parser through `jvk_command` |
| 18 | Device/Hardware View         | M6     | virtual hardware inventory |
| 19 | System Settings              | M10b/M11b | config read/write endpoints over `config/*.json` |
| 20 | System Information           | post-M11a | trivial snapshot extensions |
| 21 | Shutdown / Restart Screen    | M8/M11b | `shutdown` command + Electron close sequence |

**Mid-sem review shows ~6 fully-live screens (1, 2, 7, 8-partial, 9, 16) with the rest present
in the menu, greyed as "subsystem offline".**

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
Active Agents    7/10       <- arrives M9
System Uptime    02:14:36   <- real: uptime_ticks
Recent Events               <- real: /ws log stream
- Coding Agent started
- Finance Agent allocated memory
- Context switch
```

Interactions: click agent -> workspace (M9); click process -> details; click CPU/memory ->
respective screens; start/stop agents (M9). Before M9, agent tiles show offline state.

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

### 6.6 File Explorer (#11)
Familiar tree + file list over the virtual filesystem. Operations: create/open/read/write/
rename/delete/copy/move/search folder+file.
Path: `React -> FS API -> Kernel -> Filesystem -> disk image`.

### 6.7 IPC & Synchronization (#10)

```
FROM             TO               MESSAGE
Research Agent -> Coding Agent     DATA_READY
Finance Agent  -> Report Agent     REPORT_READY

MUTEX ProjectFile:      Coding=LOCKED  Research=WAITING
SEMAPHORE FileAccess:   avail 0, waiting [PID 108, PID 112]
```

Messages animate between processes; blocked/waiting processes listed from real queues.

### 6.8 I/O & Devices (#12) + Spooling Monitor (#13) + Hardware View (#18)
Device list with states (connected/busy/waiting/disconnected); I/O request chain visualized
end-to-end: `Agent -> I/O request -> device -> waiting -> complete -> READY`;
print/buffer queues from the spooler; virtual hardware inventory.

### 6.9 Interrupt Center (#14)

```
IRQ   SOURCE     STATUS
0     Timer      SERVICED
1     Keyboard   SERVICED
14    Disk       PENDING
```

Live servicing sequence: interrupt -> save state -> ISR -> service -> restore -> continue.
Demo controls generate test interrupts and watch the priority ordering handle them.

### 6.10 Terminal (#17)

```
JARVIS > agents
PID   NAME             STATE
101   Coding Agent     RUNNING
...
JARVIS > kill 103
```

Commands incl.: agents, processes, memory, schedule rr, files, mkdir, ls, kill, help...

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
| M7 IPC                 | #10 |
| M8 Shell               | #17, #21(command side) |
| M9a/M9b Agents         | #3, #4, #5, #6 |
| M10b API full          | #19 (settings persistence), mature #15/#16 |
| M11b Desktop completion | login, snap/dock/search/tray/notifications, #20 |

## 8. Display Conventions & Reconciliations

1. **BLOCKED vs WAITING** — kernel enum uses `WAITING`; UI displays `BLOCKED`. Same concept,
   decided at the render layer (kernel rename optional during M3b).
2. **CPU % truthfulness** — real value is 0 until agents execute on the vCPU (M9a).
   Until then panels show idle/halt state honestly; no invented utilization numbers.
3. **Memory units** — simulator counts pages/frames. Convention: `PAGE_SIZE = 4096 bytes`;
   UI derives KB/MB (`pages x 4 KB`). Total RAM displayed = configured frames x page size —
   never a hardcoded "4 GB".
4. **PIDs** — kernel generates from 1; spec examples showing 101+ are cosmetic only.
5. **Priority** — kernel stores integers; UI maps to LOW/MED/HIGH labels for display;
   priority input converts back on command.
6. **Scheduling algorithm selection** — FCFS/RR/Priority radio becomes real only with the
   scheduler-strategy implementation (post-M11a); until then RR is shown as a fixed fact.

## 9. Contingency

If the desktop is not demo-ready ~5 days before the review: build the bare monitor page
(old M3c idea — polls `/health` + `/api/logs`) in one evening as the fallback demo UI.
Never scheduled as a milestone; exists only as written insurance.

