# JARVIS OS
# IMPLEMENTATION_PLAN.md

Version: 2.0

---

# 1. Purpose

This document is the engineering implementation plan for JARVIS OS.

It translates the product specification (`PROJECT_SPEC.md`) and the engineering architecture (`ARCHITECTURE.md`) into a concrete, ordered, buildable work breakdown based on the **decisions fixed with the faculty and the team**:

- The operating system core (kernel) is implemented in **C (C17)** and compiled into a **native shared library** (`libjarvis_kernel` / `jarvis_kernel.dll`) that runs **in-process inside the application** — a kernel-layer simulator, not a bootable OS.
- One genuine **x86-64 Assembly (NASM)** module provides register/PC save-restore used by the context switcher (the low-level proof point).
- The **FastAPI (Python) bridge** calls the C kernel via **ctypes**; the kernel talks to the world with a **JSON ABI**.
- Everything upstream of the kernel — Electron, React, voice, Gemini, SQLite, and the agent layer (Agent Simulator, Agent Hub, Agent Studio) — builds on the fixed product vision.

This replaces v1.0 of this document (which assumed a pure-Python simulated kernel).

---

# 2. What Changed vs. v1.0

| Concern | v1.0 (Python kernel) | v2.0 (C-native kernel) |
|---|---|---|
| Kernel language | Python | **C17** compiled to a shared library |
| Context switching | Python functions | C + **NASM** register/PC save-restore stub |
| Frontend↔kernel | REST/WS straight into Python | REST/WS → **FastAPI → ctypes → C ABI** |
| Kernel isolation | app code | symbolic shared library, still in-process, still single source of truth |
| Demo + test | pytest | **Google Test (C)** for kernel modules + pytest for bridge |
| Built OS deliverable | Python app | `kernel.dll` + Electron app (`jarvis.exe`) |

Constants that DID NOT change:

- Kernel is the **only authority**; every request passes through it.
- Subsystems never talk to each other; only through kernel syscalls.
- React owns UI state, SQLite owns persistent state, kernel owns runtime state.
- Same layering, same 3 academic stages, same JARVIS UI / voice / Gemini goals; the OS's "programs" are simulated AI agents running as processes.

---

# 3. Final Architecture

```

        React Desktop UI (Electron)              TypeScript
              │   REST (commands)
              ▼
     FastAPI Bridge (Python, Pydantic)         Python
              │   ctypes
              ▼
       libjarvis_kernel.so/.dll               C (C17)
              │         └──────── one NASM stub (context switch)
     ┌────────┼────────┬────────┬──────────┬───────────┐
     │        │        │        │          │           │
  CPU    Process    Memory     FS     Devices     IPC
              │
              ▼
     Virtual Hardware state (in C)
              │
              ▼ (kernel events → WebSocket → React)
```

---

# 4. Technology Stack (final)

| Layer | Technology | Purpose |
|---|---|---|
| Kernel | **C (C17)** | all OS modules, compiled to shared library |
| Kernel low-level | **x86-64 Assembly (NASM)** | context-switch save/restore stub |
| ABI JSON | **cJSON** (vendored) | serialization across the kernel boundary |
| Bridge | **FastAPI + Pydantic** | REST + WebSocket, forwards to kernel |
| Kernel invocation | **Python `ctypes`** | load `libjarvis_kernel`, call `jvk_*` functions |
| Frontend | **Electron · React 19 · TypeScript** | desktop UI |
| UI kit | **Tailwind · shadcn/ui · Framer Motion · Lucide · Sonner** | JARVIS look |
| State | **Zustand** | UI state only |
| Data | **Axios · WebSocket** | command + live push |
| Charts | **Recharts** | CPU / RAM / processes |
| Voice | **Faster-Whisper · OpenWakeWord · pyttsx3** | STT · wake word · TTS |
| AI reasoning | **Rule-based AI** (local, no API key) | only reasoning tasks |
| Database | **SQLite + SQLAlchemy** | persistent metadata only |
| Kernel testing | **Google Test** | unit tests in C |
| Bridge/backend tests | **pytest · pytest-asyncio** | bridge + API tests |
| Frontend tests | **Vitest · React Testing Library · Playwright** | |
| Build | **Make / CMake · gcc (MinGW-w64 or host gcc)** | kernel lib + tests |
| Packaging | **electron-builder** | `jarvis.exe` |

---

# 5. Repository Layout

```
VOS/
├── doc/                         existing reference docs + this plan
├── kernel/                      THE C KERNEL (builds libjarvis_kernel)
│   ├── CMakeLists.txt            (or Makefile) → shared library + gtest build
│   ├── core/        kernel.c, syscall.c, dispatcher.c, event_bus.c, log.c, error.c
│   ├── cpu/         cpu.c, registers.h, alu.c, clock.c, instruction_set.c
│   ├── asm/         context_switch.S      (NASM)
│   ├── process/     pcb.c, pid_gen.c, queues.c, process_manager.c, context_switch.c
│   ├── scheduler/   scheduler.c, fcfs.c, sjf.c, priority.c, round_robin.c, metrics.c
│   ├── memory/      memory_manager.c, frames.c, paging.c, mmu.c, vm.c,
│   │                alloc/ (first_fit, best_fit, worst_fit), replace/ (fifo, lru, clock)
│   ├── interrupts/  ic.c, isr.c, isr_keyboard.c, isr_timer.c, isr_disk.c, isr_sw.c, priority.c
│   ├── filesystem/  vfs.c, disk.c, entries.c, perms.c, open_table.c
│   ├── device/      device_manager.c, base_device.h, drivers/kb.c mouse.c display.c printer.c disk.c clock.c
│   ├── io/          io_manager.c, buffers.c, spooler.c
│   ├── ipc/         ipc.c, sync.c (mutex, semaphore), deadlock.c
│   ├── shell/       shell.c, parser.c
│   └── deps/        cJSON/          (vendored single-header)
│
├── backend/                  FastAPI bridge
│   ├── main.py               lifecycle: load kernel dll (ctypes), boot, serve
│   ├── services/kernel_loader.py      # ctypes bindings + jvk_* wrappers
│   ├── api/                  process, memory, fs, devices, scheduler, shell, ai, system, voice
│   ├── ws/                   channels: processes, memory, cpu, logs, devices, notifications, voice
│   └── schemas/              pydantic models
│
├── ai/                        voice + LLM
│   ├── stt/, wake/, parser/, tts/, llm/, context/, automation/, conversation/, prompts/
│
├── database/                  SQLite + SQLAlchemy models (Settings, User, InstalledApp, Log, VoiceHistory, FileNode)
├── config/                    boot.json, scheduler.json, memory.json, fs.json, device.json, voice.json, ai.json, theme.json
├── frontend/                  Electron + React + TypeScript (see ARCHITECTURE Part 5)
├── tests/
│   ├── kernel/                Google Test C tests per module
│   └── python/                pytest (unit + integration + ABI), e2e via Playwright
└── assets/
```

---

# 6. Kernel ABI (the C ⇄ Python golden boundary)

Every object crossing the kernel boundary is a **JSON string**. This keeps the kernel UI-free, trivially testable, and interchangeable.

```c
// kernel/abi.h   (exported by libjarvis_kernel)
const char* jvk_init(const char* config_json);      // "ok" or error
const char* jvk_command(const char* action_json);   // {"action":"launch_agent","agent":"finance",...} -> result_json
void        jvk_tick(void);                          // advance virtual clock, fire timer interrupt, run scheduler
const char* jvk_snapshot(void);                      // full system state for UI
const char* jvk_logs(int since);                     // incremental kernel logs
void        jvk_shutdown(void);                      // graceful close, free resources
int         jvk_last_error(char* buf, size_t n);      // last error message (never nullptr)
```

Rules:
- The bridge (`backend/services/kernel_bridge.py`) binds these with `ctypes.CDLL` and wraps them in typed Python functions.
- Every failing `jvk_command` still returns a JSON error object `{"ok":false, "error": ...}` — the kernel never crashes the process.
- Kernel events (process created, page fault, interrupt, context switch, file io) are **polled from C in the bridge via `jvk_snapshot`/`jvk_logs` ticking** and pushed over its WebSocket channels.

---

# 7. Milestones (16) — Each independently shippable & demoable

## M0 — Kernel Toolchain + Scaffold
- Create `kernel/` with CMake (or Make), output target `jarvis_kernel` (shared). Vendored `cJSON`.
- Smallest possible ABI: `jvk_init` + `jvk_command` that echoes `{"ok":true,"kernel":"c-native"}`.
- Python `kernel_bridge.py` bindings + pytest smoke test calling it.
- **DoD:** `python -m pytest tests/integration/test_abi.py` passes; running returned JSON is real (not hardcoded).

---

### M1 — Kernel Core (C)
- `core/`: KernelCore struct, **dispatcher** (routes by action string), **syscall table**, **event bus** (pub/sub), **logger**, **error handler** (never crash → JSON error).
- Events: `PROCESS_CREATED`, `MEMORY_ALLOCATED`, `PAGE_FAULT`, `CONTEXT_SWITCHED`, ...
- **DoD:** GTest sends a fake `create_process` and asserts event emitted + logged; an invalid action returns `{"ok":false,...}`.

---

## M2 — Virtual Machine Model + CPU (C)
- `cpu/`: registers (structs), Program Counter, Instruction Register, ALU ops, clock/timer with configurable speed.
- Fetch-Decode-Execute loop with tiny instruction set (MOV/ADD/SUB/JMP/JZ/HALT/...).
- Every `jvk_tick` runs N instructions; timer interrupt when quantum expires.
- **DoD:** C test executes a small program (e.g. sum loop) and asserts PC/register outcome; the bridge reads registers in snapshots.

---

## M3 — Process Manager (C)
- `process/`: PCB, PID gen, Ready/Waiting/Suspended/Terminated queues, lifecycle state machine, suspend/resume/kill.
- `asm/context_switch.S` — **NASM stub** saving/restoring registers+PC; verified by a C wrapper that swaps two pointer-to-state blobs.
- **DoD:** create/kill/suspend/resume via `jvk_command`; queue contents verified; context switch swaps a placeholder context buffer (payload only — no hardware).

---

## M4 — CPU Scheduler (C)
- Strategy: `fcfs`, `sjf`, `priority`, `round_robin`, behind a single selector interface.
- Switch at runtime: `{"scheduler":"round_robin"}`.
- Metrics: avg wait, avg turnaround, CPU utilization, context switch count.
- **DoD:** stress test with seeded processes; scheduler swappable mid-run and behavior observable via snapshots.

---

## M5 — Memory Manager, Paging & Virtual Memory (C)
- Frames, page tables, MMU translate(virtual→physical) inside the sim; allocation: First/Best/Worst fit; replacement: FIFO/LRU/Clock; swap in/out (in-memory swap area).
- Page fault → interrupt → Memory Manager load + update; all stats updated.
- **DoD:** C tests for each allocator + each replacement; a fault loop test that keeps running with swap; snapshot shows frames/pages/pageTable.

---

## M6 — Interrupt / ISR handling & Error Manager (C)
- `ic.c` queue + priority order (shutdown > page-fault > disk > keyboard > timer > software); ISR registry; runs one ISR at a time.
- Error categories + recovery; simulated `panic()` screen message (purely educational) delivered as a snapshot flag.
- **DoD:** interrupt simulation is servable, priority order is honoured, unrecoverable errors return `{"ok":false,...,"recovered":false}` and the kernel stays up.

---

## M7 — Virtual File System + Virtual Disk (C)
- Tree of dirs/entries, metadata, permissions, read/write/move/copy/delete/search, contiguous block allocation, open-FD table.
- **DoD:** C tests do full CRUD + permission denial; data stored in a `disk image` (virtual blocks in memory, optionally dumped to a binary file). Reads/writes never touch Windows host paths.

---

## M8 — Devices, Drivers, I/O, Buffers & Spooler (C)
- `device_manager.c`, `base_device.c`, drivers: keyboard, mouse, display, printer, disk, clock with a shared interface (init/read/write/handle_interrupt/report_error).
- I/O manager queues requests; input/output buffers; printer spooler (job → current → completed + wait time).
- Open/fault states: Connected/Busy/Waiting/Disconnected.
- **DoD:** device CRUD + a spool of 3 jobs prints in order; bridge snapshot shows device statuses.

---

## M9 — IPC & Synchronization (C)
- Message queues, shared memory, pipes; mutex + counting semaphore; deadlock detection via a resource-allocation graph (cycle detection).
- Synchronization protects printer/disk driver paths.
- **DoD:** producer-consumer bounded buffer test; deadlock scenario detected, then a chosen victim resolves it.

---

## M10 — Shell (C)
- `shell.c` + parser: `help, ps, kill, memory, disk, devices, interrupts, scheduler, files, mkdir, touch, cd, ls, pwd, clear, shutdown, restart, about`.
- **DoD:** each command proxies through `jvk_command`; returns formatted text; history buffer.

---

## M11 — Bridge API & WebSocket (complete)
- Pydantic schemas for every request/response; REST routers; WS channels wired to snapshot/log poller.
- **DoD:** each REST endpoint returns schema-valid typed JSON; `jvk_tick` drives pushes; no continuous browser polling.

---

## M12 — Desktop Shell
- Boot screen → Login → JARVIS theme desktop; window manager (open/move/resize/min/max/snap/focus); taskbar, dock, search, notifications, tray, clock.
- Every agent console reflects a real agent process (`POST /api/agents/launch`); closing one kills the agent via `kill_process`.
- **DoD:** 6 windows simultaneously, correct z-order, drag/resize; closing an agent verified in Task Manager snapshot.

---

## M13 — Agent Runtime & Built-in Agents
- Agent Simulator in the kernel (Plan → Think → Act → Report pipeline), agent registry, task queues; Agent Studio (create/save custom agents) and Agent Hub.
- Built-in agents: Finance · Coding · Research · Writing · HR · Legal · Marketing · Travel · Health.
- System tools: File Explorer · Task Manager · Memory Viewer · CPU Monitor · System Monitor · Device Manager · Terminal · IPC & Sync Lab · Settings.
- **DoD:** every agent runs as a process, consumes CPU/memory, and reports results; Settings change scheduler/memory/replacement instantly (no restart).

---

## M14 — Voice
- STT (Faster-Whisper), wake word (OpenWakeWord) gating "Hey JARVIS", local intent parser → `jvk_command`, TTS (pyttsx3) confirmations.
- **Principle:** Voice is a secondary I/O layer. Every voice-capable feature must already be executable without voice (JSON ABI / shell / GUI). Voice only maps speech → `jvk_command` and responses → speech.
- **DoD:** "launch finance agent" → kernel call without network; <1s local.

---

## M15 — Gemini (reasoning only)
- `context/build_context` builds a kernel snapshot; Gemini only for Explain state / Automation / Health summary / Session summary; every plan executes strictly through `jvk_command`.
- **DoD:** mock Gemini returns a plan; executor verifies each step hit the kernel once.

---

## M16 — Testing, Hardening, Packaging, Presentation
- Google Test suites for every C module; pytest integration suites; Playwright e2e (boot→login→launch agents→save→voice→shutdown).
- Guard: no subsystem imports, kernel is crash-isolated, runtime never in SQLite.
- electron-builder → `jarvis.exe`, no manual install besides Node/Python; final demo recorded.

---

# 8. Cross-cutting rules (enforced by tests)

1. **Kernel only** — every mutation goes through `jvk_command`/`jvk_tick` (test: no other path exists).
2. **JSON ABI only** — no binary structs leak over the bridge (keeps language portability of the kernel).
3. **No crash** — ctypes call guarded; C returns error JSON, never terminates.
4. **Strategy everywhere** — scheduler, allocator, replacement are strategy-selectable at runtime.
5. **Persist only metadata** — SQLite never holds processes, RAM, queues, page tables (test `tests/integration/test_persistence_rules.py`).
6. **No hardcodes** — simulation values come from data seeded via config + state, never from UI code.

---

# 9. Build & Run (dev loop)

```bash
# 1) kernel
cd kernel && cmake -S . -B build && cmake --build build     # -> build/libjarvis_kernel.so/.dll
ctest --test-dir build                                       # GTest suites

# 2) bridge
python -m venv .venv && .venv/Scripts/activate
pip install -e backend[dev]
uvicorn backend.main:app --reload --port 8000               # boots kernel via ctypes

# 3) frontend
cd frontend && npm install && npm run dev                    # electron window
```

---

# 10. Milestone Dependency Graph

```
M0 Toolchain (ctypes → tests)
 └─> M1 Kernel core (dispatcher, syscall, events, log, errors)
       ├─ M2 CPU model + NASM context stub
       │    ├─ M3 Process Manager     │
       │    │    └─ M4 Scheduler
       │    └─ M5 Memory (+paging, VM)
       └─ M3+M5 ──> M6 Interrupts & errors
              └─> M7 Filesystem
                    ├─> M8 Devices / I/O
                    ├─> M9 IPC + Sync
                    └─> M10 Shell
M11 Bridge+WS (full) → M12 Desktop → M13 Agents → M14 Voice → M15 AI
M16 Test/Harden/Package/Present (from M0 onward)
```

Every milestone keeps a demo: from M1 you can type `jvk_command` in Python REPL; from M11 the whole OS is visible in React.

---

# 11. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| C memory bugs/hard crashes | whole app dies | crash-isolated layer: ctypes returns cleanup; CTest each module; no `free` after return strings (owned by kernel), `panic` path never reaches host. |
| Bridge type mismatch | schema drift | Pydantic schemas forced in/out; snapshot version field. |
| Scope creep (UI/AI starving the kernel) | unfinished core | enforce milestone order; each milestone's DoD is checked before the next begins. |
| Faculty "not a real OS" objection | credibility | showcase the C algorithms + NASM stub + real kernel ABI; live demo of crash-isolation, paging, scheduling. |

---

*This plan is the executable companion to PROJECT_SPEC.md, ARCHITECTURE.md, and AGENTS.md — now written for a C-native kernel. No document contradicts these choices.*