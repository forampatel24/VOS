# JARVIS OS — Team Overview & Project Status

Version: 1.0 · Date: 2026-08-17

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
| Voice (post-MVP) | Faster-Whisper · OpenWakeWord · pyttsx3 |
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
| M2 | CPU & Clock | Registers + ALU, fetch-decode-execute loop, configurable clock + timer interrupt, NASM context-switch stub, round-robin scheduler stub | ✅ |
| M3a | Process Manager | PCB, PID generator, ready/waiting/suspended/terminated queues, lifecycle state machine, suspend/resume/kill, real register+PC context switching | ✅ |
| M3b | Memory Manager | Frames + page tables, virtual memory with paging, MMU translation, allocators (first/best/worst fit), replacement (FIFO/LRU/Clock), swap in/out, page faults | ▶ |
| M4 | Interrupt Controller & Error Manager | Priority interrupt queue, ISR registry, error recovery, simulated panic flag — kernel never crashes | ○ |
| M5 | File System & Disk | Virtual disk, directory/file tree, CRUD, permissions, open-FD table — never touches real Windows paths | ○ |
| M6 | Device Manager & Drivers | Common device interface, keyboard/mouse/display/printer/disk/clock drivers, I/O queues, buffers, printer spooler | ○ |
| M7 | IPC & Synchronization | Message queues, shared memory, pipes, mutex + semaphore, deadlock detection | ○ |
| M8 | Shell & CLI | Shell + parser (`ps`, `kill`, `memory`, `files`, `mkdir`, `ls`, `shutdown`, …) proxied through the kernel | ○ |
| M9a | Agent System Core | Agent Simulator in the kernel (Plan → Think → Act → Report), agent registry, task queues; agents run as real processes | ○ |
| M9b | Agent Consoles & System Tools | Built-in agents, live consoles, system tools (Task Manager, File Explorer, Memory Viewer, CPU Monitor, Device Manager, Settings) | ○ |
| M10b | Backend API (full) | Pydantic schemas, REST routers for all subsystems, WebSocket channels pushing live kernel events | ○ |
| M11 | Desktop & UI | Boot → login → JARVIS desktop, window manager, taskbar, dock, search, notifications, tray, clock — **the clickable OS MVP lands here** | ○ |

### Phase 2 — Post-MVP (voice + AI + polish, after the clickable OS)

| # | Milestone | What it delivers | Status |
|---|---|---|---|
| M10a | Voice | "Hey JARVIS" wake word, STT, local intent parser → kernel command, TTS confirmations — a secondary I/O layer on the finished OS | ○ |
| M12 | AI Layer & Gemini | Rule-based local AI; Gemini only for explain-state / automation / health & session summaries; plans execute strictly via kernel commands | ○ |
| M13 | Testing, Hardening & Packaging | Google Test per C module, pytest integration, Playwright e2e, crash isolation, `electron-builder` → `jarvis.exe` | ○ |
| M14 | Documentation & Release | Final docs pass, recorded demo, release build | ○ |
| M15 | Future Enhancements | Ideas beyond core scope | ○ |

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

- ✅ **M0, M1, M2 complete** — kernel builds as `jarvis_kernel.dll`, bridge works, 16 tests pass, live server boots the kernel and runs CPU programs end-to-end.
- ▶ **Next milestone: M3b Memory Manager** (frames, page tables, and virtual memory).
- The order matters: kernel core first (processes → memory → interrupts → filesystem → devices → IPC), then shell, then the agent layer, then the clickable desktop (MVP), then voice and AI on top.
