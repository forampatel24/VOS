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
│   ├── main.py                 lifecycle: boot kernel, serve REST/WS
│   └── services/kernel_loader.py   ctypes bindings + jvk_* wrappers
├── config/                     boot, scheduler, memory, paging, theme, voice, clock JSON
├── doc/                        ARCHITECTURE, IMPLEMENTATION_PLAN, PROJECT_SPEC, TEAM_OVERVIEW
├── kernel/                     THE C KERNEL (builds jarvis_kernel.dll)
│   ├── core/                   kernel.c (dispatcher, JSON ABI)
│   ├── cpu/                    registers, ALU, clock, CPU simulator
│   ├── asm/                    context_switch.S (NASM)
│   ├── scheduler/              round-robin scheduler (stub → strategy pattern)
│   ├── deps/cJSON/             vendored JSON library
│   └── tests/                  C smoke tests
├── frontend/                   Electron + React scaffold
├── tests/python/               pytest ABI + FastAPI tests
└── .toolchain/                 local junctions to gcc / nasm (never committed)
```

---

## Build & Run

### Requirements
- Python 3.12+
- gcc (MinGW-w64) and NASM, reachable via `.toolchain\gcc` and `.toolchain\nasm` junctions
- `mingw32-make` (for the kernel Makefile)

### 1. Build the kernel
```bash
cd kernel
mingw32-make          # produces jarvis_kernel.dll + libjarvis_kernel.a
mingw32-make test     # runs the C smoke test
```

### 2. Run the bridge (FastAPI)
```bash
python -m pytest tests -q        # ABI + API test suite
uvicorn backend.main:app --port 8000   # boots the kernel via ctypes
```

Endpoints: `GET /health`, `POST /api/command` (flat `{"action": ..., ...}` or legacy `{"action": ..., "data": {...}}`), `GET /api/tick`, `GET /api/logs`.

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

Every launched agent is a real simulated process: PCB, PID, scheduling, virtual memory, page faults, context switches — all visible live.

---

## Implementation Status

| Phase | Milestones | Status |
|---|---|---|
| MVP | M0 Project Skeleton · M1 Toolchain & Kernel Build · M2 CPU & Clock | ✅ Completed |
| MVP | M3a Process Manager | ▶ Next up |
| MVP | M3b Memory Manager · M4 Interrupts & Errors · M5 Filesystem · M6 Devices · M7 IPC · M8 Shell · M9a Agent Core · M9b Consoles & Tools · M10b API · M11 Desktop | ○ Pending |
| Post-MVP | M10a Voice · M12 AI & Gemini · M13 Testing/Packaging · M14 Docs/Release · M15 Future | ○ Pending |

The MVP is the **clickable OS for AI agents**; voice and AI reasoning come after.

---

## The Demo

Boot → open Agent Hub → click an agent → watch its console open while the **System Monitor dashboard** shows its live registers, page tables, queue position, context switches and kernel events → open Task Manager to prove multiple agents run as real processes. Full script in `doc/TEAM_OVERVIEW.md`.

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
