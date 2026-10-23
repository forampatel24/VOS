# JARVIS OS

**A fully software-based educational operating system simulator that manages AI agents as processes.**

JARVIS OS is not a Linux distribution, not a Windows clone, and not an Electron app with random windows. It is a complete operating system simulator that teaches and demonstrates real OS concepts — CPU, processes, scheduling, memory and paging, filesystems, devices, interrupts, IPC — while providing a modern, futuristic, AI-powered desktop experience where the "programs" the OS runs are simulated AI agents.

Each agent (Finance, Coding, Research, and user-created ones) runs as a simulated process: it gets scheduled on the virtual CPU, allocates virtual memory, waits on I/O, and communicates through the kernel — all visualized on the desktop. **No language model runs inside an agent**; agent behaviour is a deterministic simulation, so the entire OS runs on any laptop.

---

## Highlights

- **Real kernel, real ABI** — A C (C17) kernel compiled to `jarvis_kernel.dll`, with an x86-64 Assembly (NASM) context-switch stub. Everything crosses the kernel boundary as JSON (cJSON).
- **Kernel is the only authority** — every request passes through the kernel; the UI never touches kernel state directly.
- **Live dashboards** — registers, page tables, queues, context switches, page faults and kernel events streamed to the UI in real time over WebSocket. Nothing is hardcoded.
- **AI agents as processes** — agents run a deterministic Plan → Think → Act → Report pipeline, consuming real simulated CPU and memory.
- **Educational by design** — schedulers, allocators and page-replacement algorithms are interchangeable strategies you can switch at runtime.
- **Crash-isolated** — the kernel returns JSON errors, never crashes the host process.

---

## Tech Stack

| Layer | Technology |
|---|---|
| Kernel | C (C17) → `jarvis_kernel.dll` |
| Low-level | x86-64 Assembly (NASM) — context-switch stub |
| ABI | cJSON (vendored) — JSON strings across the boundary |
| Bridge | Python 3.12 · FastAPI · Pydantic · ctypes |
| Frontend | Electron · React · TypeScript · Tailwind · Framer Motion · shadcn/ui · Zustand · WebSocket |
| Voice *(post-MVP)* | Faster-Whisper · OpenWakeWord · pyttsx3 |
| AI reasoning *(post-MVP)* | Rule-based local AI + Gemini (reasoning only) |
| Database | SQLite + SQLAlchemy (persistent metadata only — never runtime state) |
| Testing | Google Test (C) · pytest · Playwright |

---

## Repository Layout

```
VOS/
├── AGENTS.md                   AI development guidelines (rules we follow)
├── backend/                    FastAPI bridge (ctypes → kernel)
│   ├── main.py                 lifecycle: boot kernel, serve REST/WS (/health, /api/*, /ws)
│   ├── routers/agents.py           Agent Studio profiles REST (/api/agents)
│   └── services/
│       ├── kernel_loader.py        ctypes bindings + jvk_* wrappers
│       ├── config_loader.py        merges config/*.json into the jvk_init payload
│       └── agent_store.py          SQLite persistence for custom agent profiles
├── config/                     boot JSON: machine, clock, memory, paging, scheduler, fs, device, ipc, agents, theme, voice
├── doc/                        ARCHITECTURE, IMPLEMENTATION_PLAN, PROJECT_SPEC, TEAM_OVERVIEW, FRONTEND_SPEC
├── kernel/                     THE C KERNEL (builds jarvis_kernel.dll)
│   ├── core/                   kernel.c + kernel_memory.c + kernel_fs.c + kernel_devices.c + kernel_ipc*.c + kernel_shell.c
│   ├── machine/                virtual motherboard model (board, POST, slots)
│   ├── cpu/                    registers, ALU, clock, CPU simulator
│   ├── asm/                    context_switch.S (NASM)
│   ├── process/                PCB, PID gen, queues, lifecycle
│   ├── scheduler/              RR/FCFS/SJF/Priority (strategy pattern) + per-tick metrics
│   ├── memory/                 frames, page tables, MMU, alloc/replacement, swap, agent budgets
│   ├── filesystem/             virtual disk, inodes, VFS, open-file table
│   ├── device/                 device manager + 6 drivers (keyboard/mouse/display/printer/disk/clock)
│   ├── interrupts/             priority queue, ISRs (incl. context_switch), error manager, panic
│   ├── ipc/                    queues, shared memory, pipes, mutex/semaphore, deadlock detection
│   ├── shell/                  tokenizer, path resolver, proc/mem/fs/ipc commands
│   ├── agent/                  agent registry + Plan → Think → Act → Report task engine
│   ├── deps/cJSON/             vendored JSON library
│   └── tests/                  C smoke tests (cpu, process, memory, interrupts, filesystem, devices, ipc, shell, system, agents)
├── frontend/                   Electron + Vite + React + Tailwind + Zustand + Framer Motion
│   ├── electron/               main.js (spawns backend) + preload.js
│   └── src/                    13 fixed windows + dynamic agent consoles + taskbar + desktop
├── tests/python/               pytest ABI + FastAPI tests (90 tests)
└── .toolchain/                 local junctions to gcc / nasm (never committed)
```

---

## Build & Run

### Requirements
- Python 3.12+
- gcc + NASM. **Linux/WSL:** system `gcc`, `nasm`, `make`. **Windows:** MinGW-w64 and NASM reachable via `.toolchain\gcc` and `.toolchain\nasm` junctions, plus `mingw32-make`.

### 1. Build the kernel
```bash
cd kernel

# Linux / Ubuntu WSL
make clean && make && make test

# Windows
mingw32-make && mingw32-make test
```

Linux produces `libjarvis_kernel.so`; Windows produces `jarvis_kernel.dll` + `libjarvis_kernel.a`. The C smoke suite runs 10 binaries and must end with 10 PASS lines.

### 2. Run the bridge (FastAPI)
```bash
python -m pytest tests -q        # 90 tests (ABI + API + FS + devices + machine/metrics/budgets + IPC + shell + agents)
python -m uvicorn backend.main:app --port 8000   # boots the kernel via ctypes + config/*.json
```

On Ubuntu/WSL use the project venv and run from the repository root:

```bash
source .venv/bin/activate
export PYTHONPATH=/mnt/d/Foram_TP/VOS
python -m uvicorn backend.main:app --host 127.0.0.1 --port 8000
```

Endpoints: `GET /health`, `GET /api/snapshot`, `GET /api/config`, `POST /api/command` (flat `{"action": ..., ...}`), `GET /api/tick`, `GET /api/logs`, `GET|POST /api/agents`, `PUT|DELETE /api/agents/{id}`, `WebSocket /ws` (live snapshot/logs, auto-tick every 2.5s).

### 3. Run the frontend (Electron + React)
```bash
cd frontend
npm install
npm run dev
```

---

## Apps & Agents

**12 apps:** File Explorer · Task Manager · Memory Viewer · CPU Monitor · System Monitor (live process dashboard) · Device Manager · Terminal · IPC & Sync Lab · Settings · Agent Hub · Agent Studio · Agent Consoles.

**9 built-in agents:** Finance · Coding · Research · Writing · HR · Legal · Marketing · Travel · Health — plus unlimited user-created agents via Agent Studio.

Every launched agent is a real simulated process: PCB, PID, scheduling, virtual memory, page faults, context switches — all visible live. Tasks advance through a deterministic `QUEUED → PLANNING → THINKING → ACTING → REPORTING → DONE` pipeline only while the agent's pid owns the CPU, and finished reports are written to `/Agents/<key>/report-<task>.md` in the virtual filesystem. Custom agents created in Agent Studio persist in SQLite and re-register at boot.

---

## Implementation Status

| Phase | Milestones | Status |
|---|---|---|
| MVP | M0 Project Skeleton · M1 Toolchain & Kernel Build · M2 Machine & CPU/Clock | ✅ Completed — motherboard model (board/chipset/bus/BIOS/slots) from `config/machine.json` |
| MVP | M3a Process Manager · M3b Memory Manager · M11a Frontend Foundation (Electron) · M4 Interrupts & Errors · M4.5 Scheduler Strategy (RR/FCFS/SJF/Priority) | ✅ Completed — scheduler metrics (CPU util / avg wait / avg turnaround), per-agent memory budgets, `context_switch` IRQ |
| MVP | M5 Virtual Filesystem & Disk · M6 Devices & Drivers | ✅ Completed — File Explorer + Device Manager live; config-driven devices + disk write buffer; 70 pytest + 7 C smokes |
| MVP | M7 IPC & Synchronization · M8 Shell & CLI | ✅ Completed — IPC Lab (queues, shared memory, pipes, mutex/semaphore, wait-for graph, deadlock recovery) and Terminal (kernel shell) live; 90 pytest + 10 C smokes as of M9b |
| MVP | M9a Agent Core · M9b Consoles & Tools | ✅ Completed — 9 built-in agents, kernel task pipeline, Agent Hub, Agent Studio (SQLite profiles), dynamic per-pid consoles; 90 pytest + 10 C smokes |
| MVP | M10b API · M11b Desktop | ▶ Next: M10b Backend API Full, then M11b Desktop completion |
| Post-MVP | M10a Voice · M12 AI & Gemini · M13 Testing/Packaging · M14 Docs/Release · M15 Future | ○ Pending |

The MVP is the **clickable OS for AI agents**; voice and AI reasoning come after.

---

## The Demo

Boot → open Agent Hub → click an agent → watch its console open while the **System Monitor dashboard** shows its live registers, page tables, queue position, context switches and kernel events → open Task Manager to prove multiple agents run as real processes. IPC Lab demonstrates blocked queues and deadlock recovery; the Terminal runs the same kernel commands. Full script in `doc/TEAM_OVERVIEW.md`.

---

## Documentation

| Doc | Purpose |
|---|---|
| `doc/PROJECT_SPEC.md` | Product specification |
| `doc/ARCHITECTURE.md` | Engineering architecture |
| `doc/IMPLEMENTATION_PLAN.md` | Milestones & work breakdown |
| `doc/TEAM_OVERVIEW.md` | Team overview, status, demo script |
| `AGENTS.md` | AI development guidelines |

---

*JARVIS OS — an educational OS simulator where the programs are AI agents.*
