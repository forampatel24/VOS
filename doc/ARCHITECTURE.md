# JARVIS OS
# ARCHITECTURE.md

Version: 1.0

---

# 1. Purpose

This document defines the complete software architecture of JARVIS OS.

Unlike PROJECT_SPEC.md, which describes the product from a functional perspective, this document describes the internal engineering architecture of the system.

It defines

- System Layers
- Module Responsibilities
- Communication Flow
- Folder Responsibilities
- Design Principles
- Development Constraints
- Data Flow
- Control Flow

This document acts as the engineering reference for all developers and AI coding agents working on the project.

---

# 2. Overall Architecture Philosophy

JARVIS OS is **not** a real operating system that replaces Windows or Linux.

Instead, it is a complete software-based operating system simulator that recreates the behaviour of a modern desktop operating system.

Every subsystem is implemented independently inside the project.

The host operating system (Windows/Linux/macOS) is used only to execute the application.

Inside the application, JARVIS maintains its own:

- CPU
- RAM
- Scheduler
- File System
- Processes
- Interrupts
- Drivers
- Devices
- AI Agents

The host operating system is never directly exposed to the user.

To the user, the application should feel like an independent desktop operating system — one whose "programs" are AI agents. Each agent (Finance, Coding, Research, plus user-created ones) runs as a simulated process, is scheduled on the virtual CPU, allocates virtual memory, and communicates only through the kernel. No agent is backed by a local or remote language model: every agent executes a deterministic, simulated reasoning pipeline (Plan → Think → Act → Result), so the whole system demonstrates real operating-system behaviour while running on any laptop.

---

# 3. Architectural Principles

The project follows several core principles.

## 1. Kernel First

Every operation must pass through the Kernel.

No subsystem communicates directly with another subsystem.

Correct

Agent

↓

Kernel

↓

Memory Manager

Wrong

Agent

↓

Memory Manager

---

## 2. Modular Design

Every major operating system component is an independent module.

Each module should have one clearly defined responsibility.

Example

Scheduler

↓

Scheduling only

Memory Manager

↓

Memory only

Filesystem

↓

Storage only

This keeps the system maintainable.

---

## 3. Separation of Concerns

The interface must never contain operating system logic.

The backend must never contain user interface code.

Kernel modules must never depend on frontend components.

Every layer should only perform its own responsibility.

---

## 4. Event Driven Communication

Whenever possible,

modules communicate using events instead of tightly coupled function calls.

Example

Process Created

↓

Memory Allocated

↓

Scheduler Updated

↓

Desktop Updated

↓

Task Manager Updated

Instead of one module manually updating every other module,

the event system broadcasts changes.

---

## 5. API Driven Architecture

The frontend never modifies kernel data directly.

Instead,

every request is sent through Backend APIs.

Example

User clicks

Launch Finance Agent

↓

React

↓

API Request

↓

Kernel

↓

Process Manager

↓

Process Created

↓

Response

↓

GUI Updates

---

## 6. Data Ownership

Every piece of data has exactly one owner.

Examples

Processes

↓

Process Manager

Memory

↓

Memory Manager

Files

↓

Filesystem

Interrupt Queue

↓

Interrupt Controller

The owner module is responsible for creating, updating and deleting its data.

No other subsystem modifies it directly.

---

# 4. Layered Architecture

The entire operating system follows a layered architecture.

```
+------------------------------------------------------+
|                 USER INTERACTION                     |
+------------------------------------------------------+
|      Voice Commands | Keyboard | Mouse | Touch       |
+------------------------------------------------------+
|             React Desktop Environment                |
| Desktop • Agent Consoles • Taskbar • Dock • UI       |
+------------------------------------------------------+
|              FastAPI Backend Services                |
| API • Authentication • Event Router • Logging        |
+------------------------------------------------------+
|                  JARVIS KERNEL                       |
| System Calls • Scheduler • Dispatcher • Event Bus    |
+------------------------------------------------------+
|      Core Operating System Modules                    |
| Process | Agent | Memory | File System | I/O | Int.  |
+------------------------------------------------------+
|         Virtual Hardware Simulation                  |
| CPU • RAM • Disk • Keyboard • Display • Printer      |
+------------------------------------------------------+
|             Host Operating System                    |
| Windows / Linux / macOS (Execution Only)             |
+------------------------------------------------------+
```

Each layer communicates only with the layer immediately below it.

No layer may bypass another.

---

# 5. High-Level Communication Flow

Every action follows a predictable execution path.

Example:

User launches the Finance Agent.

```
User

↓

Desktop GUI

↓

Backend API

↓

Kernel

↓

Process Manager

↓

Memory Manager

↓

Scheduler

↓

CPU

↓

Agent Starts (spawns as a process)

↓

GUI Refreshes
```

The same pattern is followed for every operation.

---

# 6. Complete System Flow

```
             USER

               │

   Voice / Mouse / Keyboard

               │

               ▼

        React Desktop GUI

               │

      HTTP / WebSocket API

               │

               ▼

           FastAPI Backend

               │

         Kernel API Layer

               │

               ▼

           JARVIS Kernel

               │

 ┌─────────┬──────────┬──────────┬─────────┐

 │         │          │          │         │

 ▼         ▼          ▼          ▼         ▼

CPU    Process     Memory     Files    Devices

 │         │          │          │         │

 └─────────┴──────────┴──────────┴─────────┘

               │

               ▼

      Virtual Hardware Layer

               │

               ▼

          UI Updates
```

---

# 7. System Layers

## Layer 1 — Presentation Layer

Responsible for everything visible to the user.

Contains

Desktop

Taskbar

Window Manager

Agent Consoles

Agent Hub

Agent Studio

Settings

Notification Center

Voice Indicator

No operating system logic exists here.

Responsibilities

- Display information
- Receive user input
- Call backend APIs
- Render animations

---

## Layer 2 — Backend Layer

Acts as middleware.

Responsibilities

- Receive frontend requests
- Validate requests
- Call Kernel APIs
- Return responses
- Broadcast events

The backend contains no operating system algorithms.

It only coordinates communication.

---

## Layer 3 — Kernel Layer

The Kernel is the central controller.

Nothing bypasses it.

Responsibilities

- System Calls
- Resource Management
- Scheduling Requests
- Memory Requests
- Interrupt Dispatching
- Event Broadcasting
- Module Coordination

The Kernel knows every subsystem.

Subsystems know only the Kernel.

---

## Layer 4 — Core Modules

Contains the actual operating system logic.

Modules include

- CPU Simulator
- Scheduler
- Process Manager
- Memory Manager
- Filesystem
- Device Manager
- Interrupt Controller
- Shell
- Agent Simulator

Each module is completely independent.

The Agent Simulator gives every agent its behaviour: a deterministic Plan → Think → Act → Result pipeline that consumes CPU time and memory like any real program, while never invoking a language model.

---

## Layer 5 — Virtual Hardware

Simulates physical hardware.

Includes

CPU

RAM

Disk

Display

Keyboard

Mouse

Printer

Clock

These are software models, not real hardware.

---

# 8. Directory Responsibilities

The project structure is organized so that each directory owns a specific part of the operating system.

**Kernel-facing directories** (`kernel/`, `process/`, `scheduler/`, `memory/`, `filesystem/`, `interrupts/`, `drivers/`, `io/`, `ipc/`, `shell/` and the `asm/` context-switch stub) are implemented in **C (C17)** and compiled together into one native shared library (`libjarvis_kernel`). The library runs inside the application process and is reached exclusively through the Python bridge (`backend/`) using `ctypes` over the JSON kernel ABI (§ Kernel ABI below).

**Host-facing directories** (`frontend/`, `backend/`, `ai/`, `database/`) are implemented in TypeScript / Electron and Python.

```
frontend/
```

Owns the complete graphical desktop.

Contains

Desktop

Agent Consoles

Agent Hub

Agent Studio

Window Manager

Taskbar

Dock

Animations

Themes

---

```
backend/
```

Owns API communication.

Contains

REST APIs

WebSocket Services

Authentication

Validation

Logging

---

```
kernel/
```

Owns the operating system.

Responsible for coordinating every subsystem.

Written in C (C17), compiled into a native shared library, and paired with a single x86-64 Assembly (`asm/`) stub for context-switch register save/restore.

Contains

Kernel

Dispatcher

System Calls

Event Bus

Error Handler

---

```
process/
```

Owns process lifecycle.

Contains

PCB

Queues

Context Switching

Lifecycle

---

```
scheduler/
```

Owns CPU scheduling.

Contains

Round Robin

FCFS

SJF

Priority

Scheduling Metrics

---

```
memory/
```

Owns virtual memory.

Contains

Frames

Pages

Page Tables

Allocation

Replacement

Swapping

---

```
filesystem/
```

Owns storage.

Contains

Files

Directories

Permissions

Search

Metadata

Virtual Disk

---

```
interrupts/
```

Owns interrupt handling.

Contains

Interrupt Queue

ISR

Priority

Timer

Page Fault

Software Interrupts

---

```
drivers/
```

Owns device drivers.

Contains

Keyboard

Display

Printer

Mouse

Disk

---

```
shell/
```

Owns terminal interaction.

Contains

Parser

Commands

History

Auto Completion

---

```
ai/
```

Owns intelligent interaction.

Contains

Speech Recognition

Parser

Gemini

Automation

Voice Output

---

# 9. Module Dependency Rules

The following dependencies are allowed.

```
Frontend

↓

Backend

↓

Kernel

↓

Subsystem
```

Subsystems never call the frontend.

Subsystems never communicate directly.

Everything passes through the Kernel.

---

Forbidden

```
Scheduler

↓

Memory Manager
```

Correct

```
Scheduler

↓

Kernel

↓

Memory Manager
```

---

# 10. Design Constraints

The following engineering rules must always be respected.

- Every subsystem is independently testable.
- Every module has a single responsibility.
- No circular dependencies.
- No global mutable state shared between modules.
- Every operation passes through Kernel APIs.
- UI must never manipulate kernel objects directly.
- Kernel must remain independent of React and Electron.
- Virtual hardware must remain independent of GUI rendering.
- Every state change should generate an event.
- Every important operation should be logged.

---

# 11. End of Part 1

At this stage, the complete architectural foundation has been established.

The next part defines the internal architecture of the Kernel itself, including:

- Kernel Core
- System Call Manager
- Event Bus
- CPU Simulation
- Process Manager
- Scheduler
- Dispatcher
- Internal communication between kernel subsystems

# 12. Kernel Architecture

The Kernel is the most important component of JARVIS OS.

It acts as the brain of the operating system.

Every subsystem communicates through the Kernel.

No agent, subsystem, or driver is allowed to bypass it.

The Kernel is responsible for maintaining order, coordinating resources, and ensuring that every module works together correctly.

---

# Kernel Philosophy

The Kernel follows three fundamental principles.

1. Every request enters through the Kernel.
2. Every subsystem is controlled by the Kernel.
3. Every state change is recorded by the Kernel.

The Kernel is therefore the single source of truth for the entire operating system.

---

# 13. Internal Kernel Architecture

```
                        Kernel

                           │

        ┌──────────────────┼──────────────────┐

        │                  │                  │

        ▼                  ▼                  ▼

 System Call         Event Bus          Dispatcher

        │                  │                  │

        └──────────────┬───┴──────────────────┘

                       │

                       ▼

              Kernel Service Layer

                       │

 ┌────────┬────────┬────────┬────────┬────────┐

 ▼        ▼        ▼        ▼        ▼

CPU   Process   Memory   Files   Interrupts

 │        │        │        │        │

 └────────┴────────┴────────┴────────┘

               │

               ▼

          Virtual Hardware
```

---

# 14. Kernel Responsibilities

The Kernel is responsible for

• Process creation

• Process destruction

• Scheduling requests

• Memory requests

• File requests

• Device communication

• Interrupt servicing

• System calls

• Logging

• Event broadcasting

• Resource coordination

The Kernel itself does not implement scheduling algorithms or memory allocation.

Instead, it delegates these responsibilities to specialized modules.

---

# 15. Kernel Core

The Kernel Core acts as the central controller.

Every API request reaches the Kernel Core first.

Example

```
Launch Finance Agent

↓

Kernel Core

↓

Process Manager

↓

Memory Manager

↓

Scheduler

↓

CPU

↓

Success
```

The Kernel Core determines which subsystem should handle the request.

---

# Kernel Core Responsibilities

- Receive requests
- Validate requests
- Select subsystem
- Coordinate execution
- Handle failures
- Broadcast events
- Return responses

---

# 16. System Call Manager

Agents and system tools cannot directly access kernel modules.

Instead, they issue System Calls.

The System Call Manager receives these requests.

---

## Example Flow

```
Finance Agent

↓

system_open_file()

↓

Kernel

↓

Filesystem

↓

File Returned
```

---

## Supported System Calls

Process

```
create_process()

kill_process()

pause_process()

resume_process()
```

Memory

```
allocate_memory()

free_memory()

translate_address()
```

Files

```
create_file()

delete_file()

read_file()

write_file()

rename_file()

move_file()
```

Devices

```
read_keyboard()

display_output()

print_document()
```

System

```
shutdown()

restart()

generate_interrupt()

change_scheduler()
```

Agents and system tools should only know these APIs.

They should never know how the subsystem works internally.

---

# 17. Kernel Dispatcher

The Dispatcher is responsible for forwarding requests to the correct subsystem.

Example

```
create_process()

↓

Dispatcher

↓

Process Manager
```

Another example

```
allocate_memory()

↓

Dispatcher

↓

Memory Manager
```

This keeps the Kernel organized.

---

# Dispatcher Responsibilities

- Route requests
- Validate target module
- Return responses
- Handle unavailable services
- Maintain execution order

---

# 18. Kernel Event Bus

Every important event generated inside the operating system is published to the Event Bus.

Instead of one subsystem calling another,

events are broadcast.

---

## Example

```
Process Created

↓

Event Bus

↓

Memory Manager

↓

Scheduler

↓

Task Manager

↓

System Monitor
```

Each module subscribes only to the events it needs.

---

# Example Events

```
PROCESS_CREATED

PROCESS_TERMINATED

PROCESS_BLOCKED

PROCESS_RESUMED

MEMORY_ALLOCATED

MEMORY_RELEASED

PAGE_FAULT

FILE_CREATED

FILE_DELETED

DEVICE_CONNECTED

DEVICE_DISCONNECTED

INTERRUPT_GENERATED

INTERRUPT_COMPLETED

AGENT_INSTALLED

AGENT_REMOVED

SYSTEM_SHUTDOWN
```

---

# Benefits of Event Bus

- Loose coupling
- Easier testing
- Better scalability
- Cleaner architecture
- Easier debugging

---

# 19. Request Lifecycle

Every operation follows the same lifecycle.

```
User

↓

Frontend

↓

Backend API

↓

Kernel

↓

Dispatcher

↓

Subsystem

↓

Kernel

↓

Response

↓

Frontend
```

The response always returns through the Kernel.

---

# 20. CPU Simulator Architecture

The CPU Simulator represents a virtual processor.

It is completely independent from the host computer's CPU.

It is implemented in C (C17) inside the kernel library; the low-level save/restore of CPU state during a context switch is implemented as a single x86-64 Assembly stub (`kernel/asm/context_switch.S`), symbolically mirroring what a real OS would execute on the hardware.

---

## Internal Components

```
CPU

│

├── Program Counter

├── Instruction Register

├── Register Set

├── Clock

├── ALU (Simulated)

├── Execution Engine

├── Instruction Queue

└── Current Process
```

---

## CPU Responsibilities

- Execute instructions
- Increment Program Counter
- Trigger timer interrupts
- Execute scheduler decisions
- Maintain registers
- Update CPU statistics

---

# CPU Execution Flow

```
Scheduler

↓

Select Process

↓

Load PCB

↓

Restore Registers

↓

Execute Instructions

↓

Update Clock

↓

Quantum Expired?

↓

Yes

↓

Context Switch

↓

Repeat
```

---

# 21. Process Manager Architecture

The Process Manager owns the lifecycle of every process.

No other module creates processes.

---

## Internal Components

```
Process Manager

│

├── PCB Table

├── PID Generator

├── Ready Queue

├── Waiting Queue

├── Suspended Queue

├── Terminated Queue

└── Context Manager
```

---

## Responsibilities

- Create PCB
- Delete PCB
- Update Process State
- Manage Queues
- Suspend
- Resume
- Kill Process
- Maintain Statistics

---

# Process Creation Flow

```
Agent Launch

↓

Kernel

↓

Create PID

↓

Create PCB

↓

Request Memory

↓

Ready Queue

↓

Scheduler

↓

CPU
```

---

# Process State Machine

```
NEW

↓

READY

↓

RUNNING

↓

WAITING

↓

READY

↓

RUNNING

↓

TERMINATED
```

Suspended state may be entered from Ready or Waiting.

---

# 22. PCB Architecture

Every process owns exactly one PCB.

```
PCB

│

├── PID

├── Name

├── State

├── Priority

├── Program Counter

├── Registers

├── Memory Map

├── Open Files

├── CPU Time

├── Waiting Time

├── Parent Process

├── Queue

└── Creation Time
```

The PCB acts as the complete runtime description of a process.

Every AI agent runs as an ordinary process with an extended PCB. In addition to the generic fields above, an agent PCB carries:

- Agent ID and role name (Finance, Coding, Research, …)
- System prompt and personality profile
- Registered tools (read file, write report, query data budget, …)
- Task queue pointer (inbox of tasks submitted by the user or other agents)
- Simulated reasoning state (Planning → Thinking → Acting → Reporting) and current step

The Process Manager treats agents exactly like any other process; only the extra metadata is agent-specific.

---

# 23. Scheduler Architecture

The Scheduler decides which process receives CPU time.

It never executes processes itself.

---

## Internal Components

```
Scheduler

│

├── Ready Queue

├── Scheduling Algorithm

├── Time Quantum

├── Statistics

└── Queue Manager
```

---

## Supported Algorithms

- FCFS
- SJF
- Round Robin
- Priority

Future algorithms can be plugged into the same interface.

---

# Scheduling Flow

```
Ready Queue

↓

Scheduling Algorithm

↓

Selected Process

↓

CPU

↓

Quantum Complete

↓

Ready Queue
```

---

# Runtime Scheduler Switching

Changing the scheduling algorithm should not restart the OS.

Instead

```
Settings

↓

Kernel

↓

Scheduler

↓

Replace Algorithm

↓

Continue Execution
```

Existing processes remain active.

---

# 24. Context Switch Manager

The Context Switch Manager is responsible for saving and restoring process execution.

---

## Flow

```
Running Process

↓

Save Registers

↓

Save Program Counter

↓

Update PCB

↓

Load Next PCB

↓

Restore Registers

↓

Resume Execution
```

This module works closely with both the Scheduler and CPU Simulator.

---

# 25. Kernel Logging Service

Every important kernel action is logged.

Example

```
12:01

Process Created

PID 14

12:02

Memory Allocated

64 MB

12:03

Page Fault

12:04

Context Switch

PID 14 → PID 7

12:05

Interrupt Generated
```

Logs are stored in SQLite and streamed to the System Monitor.

---

# 26. Error Propagation

Subsystems should never crash the Kernel.

Instead

```
Subsystem Error

↓

Kernel

↓

Error Handler

↓

Log Error

↓

Notify User

↓

Continue System
```

The Kernel should always remain operational whenever possible.

---

# 27. Internal Module Communication Rules

Every subsystem communicates only through the Kernel.

Allowed

```
Scheduler

↓

Kernel

↓

Memory Manager
```

Forbidden

```
Scheduler

↓

Memory Manager
```

Allowed

```
Filesystem

↓

Kernel

↓

Interrupt Controller
```

Forbidden

```
Filesystem

↓

Interrupt Controller
```

These rules prevent tight coupling and keep the architecture modular.

---

# 28. Completion Criteria for Kernel Architecture

The kernel architecture is complete when:

- Every request passes through the Kernel Core.
- System Calls are the only interface exposed to agents and system tools.
- The Dispatcher correctly routes requests.
- The Event Bus broadcasts system events.
- CPU, Scheduler, and Process Manager operate independently but coordinate through the Kernel.
- Context switching updates PCBs correctly.
- Kernel logging records all critical events.
- Errors are isolated and handled without crashing the system.

---

# End of Part 2

The next section (Part 3) defines the internal architecture of the Memory Manager, Virtual Memory subsystem, Interrupt Controller, Page Fault handling, and Error Management, including how these modules interact with the Kernel and each other.

# JARVIS OS
# ARCHITECTURE.md

# Part 3A — Memory Architecture

---

# 29. Memory Architecture Overview

The Memory Manager is responsible for managing all memory inside JARVIS OS.

Unlike the host operating system, JARVIS maintains its own simulated RAM.

Every process receives memory from this virtual RAM rather than from Windows.

The Memory Manager is responsible for

- Memory Allocation
- Memory Deallocation
- Address Translation
- Paging
- Virtual Memory
- Page Tables
- Swap Space
- Memory Protection
- Page Replacement
- Memory Statistics

No other subsystem is allowed to directly allocate or free memory.

All requests must pass through the Kernel.

---

# 30. Memory Architecture

```
                 Kernel

                    │

                    ▼

             Memory Manager

                    │

      ┌─────────────┼─────────────┐

      │             │             │

      ▼             ▼             ▼

Frame Allocator  Page Table  Swap Manager

      │             │             │

      └─────────────┼─────────────┘

                    │

                    ▼

             Virtual RAM Model
```

The Memory Manager coordinates every memory operation.

---

# 31. Memory Responsibilities

The Memory Manager owns

- Physical Frames
- Virtual Pages
- Page Tables
- Address Translation
- Memory Statistics
- Allocation
- Deallocation
- Swap Area
- Protection

Other modules never modify memory directly.

---

# 32. Virtual RAM

The operating system contains its own RAM simulation.

Example

```
Total RAM

1024 MB
```

Internally

```
1024 MB

↓

256 Frames

↓

4 MB per Frame
```

The frame size should remain configurable.

---

# Virtual RAM Layout

```
+---------------------------+

Frame 0

Frame 1

Frame 2

Frame 3

...

Frame 255

+---------------------------+
```

Every frame maintains

- Frame Number
- Allocation Status
- Process Owner
- Page Number

---

# 33. Physical Memory Model

Each frame contains

```
Frame

│

├── Frame ID

├── Size

├── Free / Used

├── Process ID

├── Page Number

├── Last Access Time

└── Dirty Bit
```

These values allow efficient memory management.

---

# 34. Virtual Memory

Each process receives its own virtual address space.

Instead of accessing physical RAM directly,

the process works only with virtual addresses.

Example

```
Process A

Virtual Address

↓

Page Table

↓

Frame

↓

Physical Memory
```

This mirrors modern operating systems.

---

# 35. Address Space

Each process owns

```
Process

│

├── Code Segment

├── Data Segment

├── Heap

└── Stack
```

The Memory Manager tracks each segment independently.

---

# 36. Paging Architecture

Memory is divided into

Pages

and

Frames

Pages belong to processes.

Frames belong to RAM.

The Memory Manager maps pages onto frames.

---

## Paging Flow

```
Virtual Address

↓

Page Number

↓

Page Table

↓

Frame Number

↓

Physical Address
```

---

# 37. Page Table

Every process owns exactly one page table.

```
Page Table

│

├── Page Number

├── Frame Number

├── Present Bit

├── Dirty Bit

├── Referenced Bit

├── Protection

└── Last Access
```

The page table performs address translation.

---

# Example

```
Virtual Page 3

↓

Frame 18

↓

Physical Address

18 : Offset
```

---

# 38. Frame Allocator

The Frame Allocator finds available frames.

Responsibilities

- Search Free Frames
- Allocate Frame
- Release Frame
- Track Usage
- Report Statistics

---

## Allocation Flow

```
Memory Request

↓

Find Free Frame

↓

Allocate

↓

Update Frame Table

↓

Return Frame Number
```

---

# 39. Memory Allocation Process

Example

```
Process Needs

40 MB

↓

Kernel

↓

Memory Manager

↓

10 Frames Required

↓

Frames Allocated

↓

PCB Updated

↓

Scheduler Continues
```

---

# 40. Supported Allocation Algorithms

Version 1 should support

- First Fit
- Best Fit
- Worst Fit

Users should be able to switch algorithms in Settings.

Example

Settings

↓

Memory

↓

Allocation

↓

Best Fit

↓

Apply

No restart required.

---

# 41. Address Translation

The MMU (Memory Management Unit) is simulated.

Responsibilities

- Translate Virtual Address
- Check Protection
- Detect Page Fault
- Return Physical Address

---

## Translation Flow

```
CPU

↓

Virtual Address

↓

MMU

↓

Page Table

↓

Frame

↓

Physical Address
```

---

# 42. Page Fault Handler

If a requested page is not currently in RAM,

the MMU generates a page fault.

Flow

```
CPU

↓

Virtual Address

↓

Page Missing

↓

Page Fault

↓

Interrupt

↓

Memory Manager

↓

Load Page

↓

Resume Process
```

The user should see this visually inside the Memory Viewer.

---

# Page Fault Visualization

Display

Requested Page

↓

Missing

↓

Swap Access

↓

Frame Loaded

↓

Execution Continues

This is an excellent educational demonstration.

---

# 43. Swap Manager

The Swap Manager represents virtual disk space.

If RAM becomes full,

unused pages move into swap space.

Flow

```
RAM Full

↓

Choose Victim Page

↓

Move To Swap

↓

Load New Page

↓

Update Page Table
```

---

# Swap Area

```
Virtual Disk

│

├── Swap Block 1

├── Swap Block 2

├── Swap Block 3

└── ...
```

The swap area should be visualized separately from RAM.

---

# 44. Page Replacement Manager

When RAM is full,

a page replacement algorithm decides which page leaves memory.

Version 1 supports

- FIFO
- LRU
- Clock Algorithm

The algorithm should be selectable from Settings.

---

## FIFO

Oldest page leaves first.

---

## LRU

Least Recently Used page leaves.

---

## Clock Algorithm

Uses a reference bit.

More realistic than FIFO.

---

# Replacement Flow

```
RAM Full

↓

Replacement Algorithm

↓

Victim Page

↓

Swap Out

↓

Load New Page

↓

Continue Execution
```

---

# 45. Memory Protection

Each page contains protection flags.

```
Read

Write

Execute
```

Invalid access generates an exception.

Example

```
Write

↓

Read Only Page

↓

Protection Fault

↓

Interrupt

↓

Error Handler
```

---

# 46. Memory Statistics

The Memory Manager continuously calculates

- Total RAM
- Used RAM
- Free RAM
- Frames Used
- Frames Free
- Fragmentation
- Page Fault Count
- Swap Usage
- Allocation Requests

These values are displayed inside the Memory Viewer.

---

# 47. Memory Events

Whenever memory changes,

events are published.

Examples

```
MEMORY_ALLOCATED

MEMORY_RELEASED

PAGE_FAULT

PAGE_REPLACED

SWAP_IN

SWAP_OUT

FRAME_ALLOCATED

FRAME_RELEASED
```

The System Monitor subscribes to these events.

---

# 48. Kernel ↔ Memory Communication

Agents and system tools never communicate with Memory Manager.

Correct

```
Agent

↓

Kernel

↓

Memory Manager
```

Wrong

```
Agent

↓

Memory Manager
```

This rule must never be violated.

---

# 49. Memory Module Completion Criteria

The Memory Architecture is complete when:

✓ Virtual RAM is implemented.

✓ Frames and Pages are tracked independently.

✓ Every process owns a Page Table.

✓ Address Translation works correctly.

✓ Page Faults are generated and handled.

✓ Swap Space is functional.

✓ FIFO, LRU, and Clock replacement algorithms are available.

✓ Memory statistics update in real time.

✓ All memory requests pass only through the Kernel.

---

# End of Part 3A

The next section (Part 3B) covers the Interrupt Controller Architecture, Timer Interrupts, Software Interrupts, Interrupt Service Routines (ISR), Error Handling Architecture, Memory Protection Exceptions, and how the Kernel coordinates all interrupt-driven execution.

# JARVIS OS
# ARCHITECTURE.md

# Part 3B — Interrupt Architecture & Error Handling

---

# 50. Interrupt Architecture Overview

Interrupts are one of the most important concepts in an operating system.

Instead of constantly checking whether something has happened, the CPU is notified through interrupts whenever an important event occurs.

JARVIS OS simulates this behaviour exactly.

Whenever an event requires immediate attention, an interrupt is generated.

The CPU temporarily pauses the current process, executes the Interrupt Service Routine (ISR), and then resumes execution.

---

# 51. Interrupt Controller Architecture

The Interrupt Controller is responsible for managing every interrupt generated inside the operating system.

It acts as the traffic controller between hardware, software, and the CPU.

```
                  Kernel

                    │

                    ▼

          Interrupt Controller

                    │

      ┌─────────────┼─────────────┐

      │             │             │

      ▼             ▼             ▼

 Interrupt Queue   ISR Table   Priority Manager

      │             │             │

      └─────────────┼─────────────┘

                    │

                    ▼

                   CPU
```

---

# 52. Responsibilities

The Interrupt Controller is responsible for

- Receiving interrupts
- Prioritizing interrupts
- Queueing interrupts
- Dispatching interrupts
- Executing ISRs
- Restoring CPU execution
- Logging interrupt activity

---

# 53. Types of Interrupts

Version 1 supports four interrupt categories.

## Hardware Interrupts

Generated by virtual hardware.

Examples

- Keyboard
- Mouse
- Timer
- Printer
- Disk

---

## Software Interrupts

Generated by agents and system tools.

Examples

- Open File
- Create Process
- Shutdown Request
- Install Agent

---

## Memory Interrupts

Generated by the Memory Manager.

Examples

- Page Fault
- Protection Fault
- Invalid Address

---

## System Interrupts

Generated by the Kernel.

Examples

- Scheduler Tick
- Context Switch
- Process Completed
- Device Connected

---

# 54. Interrupt Queue

Incoming interrupts are first stored inside an interrupt queue.

```
Interrupt Generated

↓

Interrupt Queue

↓

Priority Check

↓

CPU

↓

ISR

↓

Completed
```

The queue guarantees interrupts are processed in the correct order.

---

# 55. Interrupt Priority Levels

Some interrupts are more important than others.

Priority order

```
Critical

↓

High

↓

Medium

↓

Low
```

Example

```
Page Fault

↓

Critical
```

```
Keyboard Input

↓

Medium
```

```
Timer Tick

↓

High
```

---

# 56. Interrupt Service Routine (ISR)

Each interrupt type has its own ISR.

```
Interrupt

↓

ISR

↓

Perform Operation

↓

Return Control
```

Example

```
Keyboard Interrupt

↓

Keyboard ISR

↓

Read Character

↓

Send Character

↓

Resume CPU
```

---

# 57. Timer Interrupt

The Timer Interrupt drives the entire operating system.

Every fixed interval

```
CPU Clock

↓

Timer Interrupt

↓

Scheduler

↓

Quantum Expired?

↓

Yes

↓

Context Switch

↓

Resume
```

Without the timer interrupt, multitasking would not function.

---

# 58. Keyboard Interrupt

Whenever the user presses a key,

the keyboard generates an interrupt.

```
Key Press

↓

Keyboard Driver

↓

Interrupt

↓

ISR

↓

Input Buffer

↓

Agent Console
```

---

# 59. Mouse Interrupt

Mouse movement and clicks generate interrupts.

```
Mouse Click

↓

Mouse Driver

↓

Interrupt

↓

ISR

↓

GUI Update
```

---

# 60. Printer Interrupt

Printing completes asynchronously.

```
Print Complete

↓

Printer Interrupt

↓

Kernel

↓

Update Queue

↓

Notify User
```

---

# 61. Disk Interrupt

Disk operations complete in the background.

```
Read Finished

↓

Disk Interrupt

↓

Filesystem

↓

Return Data

↓

Resume Process
```

---

# 62. Page Fault Interrupt

The Memory Manager generates a page fault interrupt whenever a required page is not present in RAM.

```
CPU

↓

Memory Access

↓

Missing Page

↓

Page Fault Interrupt

↓

Memory Manager

↓

Load Page

↓

Resume Process
```

---

# 63. Context Switching Through Interrupts

Context switches are interrupt-driven.

```
Timer Interrupt

↓

Save PCB

↓

Scheduler

↓

Load Next PCB

↓

Restore Registers

↓

Continue
```

This accurately models modern operating systems.

---

# 64. Interrupt Logging

Every interrupt is recorded.

Example

```
11:24

Keyboard Interrupt

11:25

Page Fault

11:26

Timer Interrupt

11:27

Context Switch

11:28

Printer Interrupt
```

These logs appear in the System Monitor.

---

# 65. Interrupt Visualization

The Interrupt Manager window should display

- Active Interrupt
- Queue Length
- ISR Executing
- Priority
- Processing Time
- Completed Interrupts

This gives students a visual understanding of interrupt handling.

---

# 66. Error Handling Architecture

The Error Handler ensures failures do not crash the operating system.

Whenever an error occurs,

the responsible subsystem reports it to the Kernel.

The Kernel forwards it to the Error Handler.

```
Subsystem

↓

Kernel

↓

Error Handler

↓

Recovery

↓

Continue Execution
```

---

# 67. Responsibilities

The Error Handler

- Detects errors
- Classifies errors
- Logs errors
- Displays notifications
- Attempts recovery
- Protects kernel stability

---

# 68. Error Categories

Version 1 supports

## Process Errors

- Invalid PID
- Process Already Running
- Process Not Found

---

## Memory Errors

- Out Of Memory
- Invalid Address
- Page Fault
- Protection Fault

---

## Filesystem Errors

- File Not Found
- Directory Missing
- Permission Denied

---

## Device Errors

- Device Busy
- Device Missing
- Driver Failure

---

## Scheduler Errors

- Empty Ready Queue
- Invalid Scheduling Algorithm

---

# 69. Error Recovery Flow

```
Error

↓

Kernel

↓

Error Handler

↓

Can Recover?

↓

YES

↓

Recover

↓

Continue

OR

↓

NO

↓

Terminate Process

↓

Log Error

↓

Notify User
```

The operating system should always recover whenever possible.

---

# 70. Memory Protection

Memory protection prevents illegal access.

Examples

```
Read Only Page

↓

Write Request

↓

Protection Exception
```

```
Invalid Address

↓

Memory Exception
```

Both generate interrupts handled by the Error Manager.

---

# 71. Kernel Panic (Simulation)

Although rare,

some critical failures cannot be recovered.

Examples

- Kernel Data Corruption
- Invalid Kernel State
- Fatal Memory Error

The simulator should display

```
KERNEL PANIC

Critical Kernel Failure

System Halted
```

This is purely educational.

---

# 72. System Logs

The Logger stores

- Errors
- Warnings
- Kernel Events
- Interrupts
- Memory Events
- Process Events

Example

```
[INFO]

Process Created

[WARNING]

Memory Usage High

[ERROR]

Page Fault

[CRITICAL]

Kernel Panic
```

---

# 73. Notifications

Whenever an important error occurs,

the Notification Center informs the user.

Examples

"Page Fault handled successfully."

"Printer disconnected."

"Memory allocation failed."

"Disk is full."

These notifications should also trigger optional voice announcements.

---

# 74. Communication Rules

Only the Kernel communicates with the Interrupt Controller.

Allowed

```
Kernel

↓

Interrupt Controller
```

Forbidden

```
Memory Manager

↓

CPU
```

Correct

```
Memory Manager

↓

Kernel

↓

Interrupt Controller

↓

CPU
```

---

# 75. Completion Criteria

The Interrupt & Error Handling subsystem is complete when

✓ Interrupt Queue works correctly.

✓ Priority scheduling functions correctly.

✓ ISRs execute independently.

✓ Timer Interrupt drives scheduling.

✓ Keyboard, Mouse, Disk and Printer interrupts are simulated.

✓ Page Fault Interrupt integrates with the Memory Manager.

✓ Error Handler classifies all major errors.

✓ Kernel recovers from recoverable failures.

✓ Logs update automatically.

✓ Notifications are displayed correctly.

---

# End of Part 3B

The next part (Part 4) defines the architecture of the Virtual File System, Virtual Disk, Device Manager, Driver Layer, I/O Manager, Inter-Process Communication (IPC), Synchronization (Mutexes, Semaphores), Buffering, Spooling, and the Terminal (Shell). At this point, the complete core operating system architecture will be defined.

# JARVIS OS
# ARCHITECTURE.md

# Part 4 — File System, I/O, Devices, IPC & Synchronization

---

# 76. File System Architecture

The Virtual File System (VFS) is responsible for managing every file and directory inside JARVIS OS.

Unlike Windows, JARVIS maintains its own completely isolated storage.

Every agent and system tool interacts only with this virtual storage.

The host operating system is never accessed directly except for optionally saving the virtual disk image.

---

# File System Responsibilities

The File System owns

- Files
- Directories
- Metadata
- Permissions
- Virtual Disk
- File Allocation
- Search
- Copy
- Move
- Delete
- Rename

Only the File System module may modify storage.

---

# File System Architecture

```
                Kernel

                  │

                  ▼

          Virtual File System

                  │

      ┌───────────┼───────────┐

      │           │           │

      ▼           ▼           ▼

 Directory    File Table   Disk Manager

                  │

                  ▼

            Virtual Disk
```

---

# 77. Directory Structure

The filesystem should mimic a modern desktop operating system.

```
/

├── System

├── Agents

├── Users

│      └── Admin

│            ├── Desktop

│            ├── Documents

│            ├── Downloads

│            ├── Pictures

│            └── Music

├── Temp

├── Logs

└── Swap
```

---

# 78. File Metadata

Every file stores

```
File ID

File Name

Extension

Owner

Created Time

Modified Time

Size

Permissions

Parent Directory

Storage Blocks

Hidden

Read Only
```

---

# 79. File Allocation

Version 1 uses contiguous allocation.

```
File

↓

Allocate Blocks

↓

Store Block Numbers

↓

Update Directory

↓

Return Success
```

Future versions may support linked and indexed allocation.

---

# 80. File Operations

Supported operations

```
Create

Open

Read

Write

Append

Rename

Move

Copy

Delete

Search
```

Every operation passes through the Kernel.

---

# Example Flow

```
Writing Agent

↓

Save File

↓

Kernel

↓

File System

↓

Disk Manager

↓

Write Blocks

↓

Return Success
```

---

# 81. File Table

Every open file has an entry.

```
Open File Table

│

├── File Descriptor

├── File Name

├── Owner Process

├── Cursor Position

├── Mode

└── Status
```

---

# 82. Virtual Disk

The Virtual Disk represents permanent storage.

Internally it consists of blocks.

Example

```
Virtual Disk

1024 Blocks

↓

Each Block

4 KB
```

---

# Disk Layout

```
Boot

System

Agents

User Files

Logs

Swap

Unused
```

---

# Disk Statistics

Display

- Total Space
- Used Space
- Free Space
- Fragmentation
- Read Speed
- Write Speed

---

# 83. Device Manager

The Device Manager controls every virtual hardware device.

It acts similarly to Device Manager in Windows.

---

# Supported Devices

```
CPU

RAM

Disk

Keyboard

Mouse

Display

Printer

Clock
```

Future

```
USB

Network

Camera

Bluetooth
```

---

# Device Model

Every device contains

```
Device ID

Device Name

Driver

Status

Interrupt Number

Power State

Statistics
```

---

# Device States

```
Connected

Disconnected

Busy

Waiting

Fault

Disabled
```

---

# 84. Driver Architecture

Drivers are software modules responsible for communicating with virtual hardware.

Agents and system tools never access devices directly.

```
Agent

↓

Kernel

↓

Driver

↓

Device
```

---

# Supported Drivers

```
Keyboard Driver

Mouse Driver

Display Driver

Disk Driver

Printer Driver

Clock Driver
```

Each driver exposes a common interface.

---

# Driver Responsibilities

- Initialize Device
- Shutdown Device
- Read Data
- Write Data
- Generate Interrupts
- Report Errors

---

# 85. I/O Manager

The I/O Manager coordinates all input and output operations.

Instead of allowing every agent and system tool to communicate with devices,

the I/O Manager schedules every request.

---

# Architecture

```
Agent

↓

Kernel

↓

I/O Manager

↓

Driver

↓

Device

↓

Interrupt

↓

Kernel

↓

Agent
```

---

# Responsibilities

- Queue Requests
- Schedule Requests
- Dispatch Drivers
- Handle Completion
- Notify Agents

---

# 86. Input Buffer

Keyboard input first enters an input buffer.

```
Keyboard

↓

Driver

↓

Input Buffer

↓

Agent Console
```

This prevents lost keystrokes.

---

# Output Buffer

Similarly,

display output

```
Agent

↓

Output Buffer

↓

Display
```

---

# Disk Buffer

```
Agent

↓

Buffer

↓

Disk
```

Buffers improve performance.

---

# 87. Spooling

Printing should be asynchronous.

```
Document

↓

Print Queue

↓

Printer

↓

Completed
```

Multiple jobs may wait inside the queue.

---

# Print Queue

```
Print Job

↓

Waiting

↓

Printing

↓

Completed
```

The GUI should visualize this queue.

---

# 88. Inter Process Communication (IPC)

Processes often need to exchange information.

Instead of communicating directly,

they use IPC.

---

# Supported IPC

```
Message Queue

Shared Memory

Pipes
```

Future

```
Sockets

Signals
```

---

# IPC Architecture

```
Process A

↓

Kernel

↓

IPC Manager

↓

Kernel

↓

Process B
```

---

# Message Queue

Messages are stored temporarily.

```
Sender

↓

Queue

↓

Receiver
```

---

# Shared Memory

Two processes share a common memory region.

```
Process A

↓

Shared Memory

↑

Process B
```

This provides high performance.

---

# Pipes

Pipes provide one-way communication.

```
Producer

↓

Pipe

↓

Consumer
```

---

# 89. Synchronization Manager

When multiple processes access the same resource,

Synchronization prevents conflicts.

---

# Responsibilities

- Mutex
- Semaphore
- Lock Management
- Deadlock Detection
- Resource Allocation

---

# Mutex

```
Printer

↓

Mutex

↓

One Process
```

The second process waits.

---

# Semaphore

```
Three Printers

↓

Semaphore = 3

↓

Three Processes

↓

Fourth Waits
```

---

# Resource Locking

```
Process

↓

Acquire Lock

↓

Use Resource

↓

Release Lock
```

---

# Deadlock Detection

The Synchronization Manager should monitor

```
Resource Allocation Graph

↓

Cycle?

↓

Deadlock
```

The GUI highlights deadlocked processes.

---

# 90. Shell Architecture

The Shell provides command-line interaction.

It communicates only through Kernel APIs.

---

# Shell Flow

```
User

↓

Command

↓

Parser

↓

Kernel

↓

Subsystem

↓

Response

↓

Terminal
```

---

# Supported Commands

```
help

ps

kill

memory

disk

devices

files

mkdir

touch

cd

ls

pwd

clear

shutdown

restart
```

---

# Shell Components

```
Shell

│

├── Parser

├── History

├── Auto Complete

├── Command Executor

└── Output Renderer
```

---

# 91. Logging Architecture

Every subsystem publishes logs.

```
Kernel

↓

Logger

↓

SQLite

↓

System Monitor
```

Subsystems never write directly to the database.

---

# Logged Events

```
Process Events

Memory Events

File Events

Interrupts

Device Events

Errors

Warnings

Voice Commands

AI Events
```

---

# 92. Communication Rules

Allowed

```
Agent

↓

Kernel

↓

Filesystem
```

Allowed

```
Agent

↓

Kernel

↓

Device Manager
```

Forbidden

```
Agent

↓

Filesystem
```

Forbidden

```
Filesystem

↓

Memory Manager
```

Everything routes through the Kernel.

---

# 93. Completion Criteria

The Storage & I/O subsystem is complete when

✓ Virtual File System is functional.

✓ Virtual Disk stores files.

✓ File metadata is maintained.

✓ Device Manager controls all devices.

✓ Drivers are modular.

✓ I/O Manager schedules requests.

✓ Input/Output buffers function correctly.

✓ Printer spooling works.

✓ IPC supports Message Queues, Shared Memory and Pipes.

✓ Synchronization supports Mutexes and Semaphores.

✓ Shell communicates only through Kernel APIs.

✓ Logging is centralized.

---

# End of Part 4

The next part (Part 5) defines the complete **Frontend Architecture**, including the Desktop Environment, Window Manager, React component hierarchy, state management, agent consoles and Agent Studio, Electron integration, and how the UI stays synchronized with the Kernel in real time.

# JARVIS OS
# ARCHITECTURE.md

# Part 5 — Frontend Architecture, Desktop Environment & Agent Consoles

---

# 94. Frontend Architecture Overview

The frontend is responsible for presenting the operating system to the user.

It behaves like a real desktop operating system while remaining completely independent from the kernel.

The frontend never performs operating system logic.

Its responsibilities are to

- Display information
- Receive user input
- Render animations
- Manage windows
- Communicate with Backend APIs

---

# 95. Frontend Technology Stack

| Component | Technology |
|-----------|------------|
| Desktop Application | Electron |
| UI Framework | React 19 |
| Language | TypeScript |
| Styling | Tailwind CSS |
| Component Library | shadcn/ui |
| Icons | Lucide React |
| Animation | Framer Motion |
| State Management | Zustand |
| API Communication | Axios |
| Real-time Updates | WebSockets |
| Charts | Recharts |
| Notifications | Sonner |

---

# 96. Frontend Layer Architecture

```
                Electron

                    │

                    ▼

               React App

                    │

        ┌───────────┼───────────┐

        ▼           ▼           ▼

 Desktop UI    Window System   AI UI

        │           │           │

        └───────────┼───────────┘

                    │

                    ▼

            Zustand Store

                    │

                    ▼

              API Services

                    │

                    ▼

               FastAPI Backend
```

---

# 97. Desktop Environment

The desktop is the primary workspace.

The user should feel like they have booted into an entirely new operating system.

The desktop contains

- Wallpaper
- Taskbar
- Dock
- Desktop Icons
- Notification Center
- System Tray
- Search
- Clock
- Voice Indicator

---

# Desktop Layout

```
 --------------------------------------------------

 Desktop Wallpaper

            [ Agent Console Windows ]

----------------------------------------------------

 Dock         Taskbar        Clock     System Tray

----------------------------------------------------
```

---

# 98. Window Manager

Every agent console and system tool runs inside a managed window.

The Window Manager controls

- Opening
- Closing
- Moving
- Resizing
- Maximizing
- Minimizing
- Snapping
- Focus

The Window Manager is independent of every agent and tool.

---

# Window Lifecycle

```
Open Console

↓

Create Window

↓

Render

↓

Focus

↓

Minimize

↓

Restore

↓

Close

↓

Destroy
```

---

# Window Object

Each window maintains

```
Window ID

Agent / Tool

Title

Position

Width

Height

State

Focused

Z Index

Resizable

Closable

Minimized
```

---

# 99. State Management

Global UI state is stored inside Zustand.

Examples

```
Running Agents

↓

Open Windows

↓

Selected Theme

↓

Notifications

↓

Current User

↓

Voice Status

↓

Quick Settings
```

Kernel state is never stored here.

Kernel state always comes from backend APIs.

---

# 100. API Layer

The frontend communicates with the backend using a centralized API service.

```
React Component

↓

API Service

↓

FastAPI

↓

Kernel

↓

Response
```

Every request follows this architecture.

---

# Example

Launching the Finance Agent

```
Agent Hub

↓

POST /agents/launch

↓

Kernel

↓

Process Manager

↓

Success

↓

Desktop Updates
```

---

# 101. WebSocket Layer

Many kernel events occur continuously.

Instead of polling,

the frontend subscribes to WebSockets.

Examples

```
CPU Usage

Memory Usage

Process Created

Interrupt Generated

File Updated

Device Connected

Voice Status
```

These update the interface in real time.

---

# 102. Component Hierarchy

```
App

│

├── Boot Screen

├── Login Screen

├── Desktop

│      ├── Wallpaper

│      ├── Taskbar

│      ├── Dock

│      ├── Agent Hub

│      ├── Agent Console

│      ├── Agent Studio

│      ├── Desktop Icons

│      ├── Notification Center

│      ├── Search

│      ├── Window Manager

│      └── Voice Assistant

└── Settings
```

---

# 103. Desktop Icons

Each agent and system tool appears as an icon.

Example

```
Finance Agent

Coding Agent

Research Agent

Agent Studio

Terminal

Memory Viewer

File Explorer

System Monitor

Settings

Device Manager
```

Double-click launches the agent or tool.

---

# 104. Taskbar

The Taskbar displays

- Running Agents
- Active Agent
- Notifications
- Quick Settings
- Search
- Time
- Voice Status

It updates automatically whenever agent processes change.

---

# 105. Dock

The Dock contains frequently used agents.

Users can pin or unpin agents.

Example

```
Finance Agent

Coding Agent

Research Agent

Agent Studio

Terminal
```

---

# 106. Notification Center

Displays

```
Kernel Events

Warnings

Errors

Voice Notifications

Updates

Print Completed

Agent Task Completed
```

Notifications arrive through WebSockets.

---

# 107. Search

Global Search searches

- Agents
- Files
- Directories
- Settings

Search requests pass through the backend.

---

# 108. Built-in Agents

Each agent demonstrates one subsystem.

---

## Agent Simulator

Every built-in agent is a simulated process.

There is no local or remote language model powering it.

Agent behaviour is a deterministic pipeline that consumes real CPU time and memory:

```
Agent Task

↓

Plan

↓

Think

↓

Act (Tool Call)

↓

Report

↓

Result Emitted
```

The kernel treats each agent like any other process: it has a PCB, a page table, a priority, and a task queue.

---

## Finance Agent

Purpose

Demonstrates

- Process Creation
- CPU Arithmetic
- Memory Allocation
- File Writing (budget reports)

Simulated tools

- query_budget
- compute_total

---

## Coding Agent

Purpose

Demonstrates

- CPU-bound scheduling
- Heap allocation
- File creation (generates source files)

Simulated tools

- read_spec
- generate_code
- run_tests

---

## Research Agent

Purpose

Demonstrates

- Filesystem reads
- Global search
- Optional Gemini summarization

Simulated tools

- search_files
- read_file
- summarize

---

## Writing Agent

Purpose

Demonstrates

- Filesystem writes
- Text output
- Spellcheck simulation (dictionary reads)

Simulated tools

- write_draft
- rewrite

---

## HR Agent

Simulates employee scheduling, onboarding checklists, and policy lookups.

## Legal Agent

Simulates document clause extraction and compliance checks.

## Marketing Agent

Simulates campaign brainstorming and report generation.

## Travel Agent

Simulates itinerary planning and booking workflows.

## Health Agent

Simulates wellness checklists, reminders, and report generation.

---

## Agent Studio

Purpose

Lets the user create their own agent.

Inputs

- Name
- Role description (system prompt)
- Personality
- Color and icon
- Tool selection
- Priority and initial memory budget

Flow

```
User Saves Agent Config

↓

Config Stored in SQLite

↓

Agent Record Created

↓

Registered in Agent Hub

↓

User Launches → Process Created
```

---

## File Explorer

Purpose

Demonstrates

- Directories
- File Operations
- Search
- Virtual Disk

---

## Task Manager

Purpose

Displays

- Running Agent Processes
- Scheduler
- PCB
- CPU Usage
- Memory Usage

---

## Memory Viewer

Purpose

Displays

- RAM
- Frames
- Pages
- Page Table
- Swap Area
- Page Faults

---

## CPU Monitor

Purpose

Displays

- Registers
- Current Process
- Scheduler
- Clock
- Quantum

---

## Device Manager

Purpose

Displays

- Devices
- Drivers
- Status
- Interrupts

---

## System Monitor

Purpose

Displays

Overall

- CPU
- Memory
- Disk
- Devices
- Agent Processes
- Logs

---

## Terminal

Purpose

Provides command-line access.

Commands communicate through Kernel APIs.

---

## Settings

Allows configuration of

- Theme
- Voice
- Scheduler
- Memory Algorithm
- Page Replacement
- Notifications

---

# 109. Theme Engine

Version 1 ships with one primary theme.

**JARVIS AI Theme**

Characteristics

- Dark Interface
- Glassmorphism
- Neon Cyan Accents
- Smooth Animations
- Rounded Panels
- Holographic Elements

Future versions may support multiple themes.

---

# 110. Animation Principles

Animations should improve clarity rather than distract.

Examples

- Window Open
- Window Close
- Context Switch Highlight
- Memory Allocation Animation
- Page Fault Animation
- CPU Switching
- Notification Slide
- Voice Pulse Animation

---

# 111. Boot Experience

Startup sequence

```
Electron Launch

↓

JARVIS Boot Logo

↓

Loading Services

↓

Kernel Initialized

↓

Desktop Loaded
```

This creates the feeling of booting into an operating system.

---

# 112. Login Screen

Version 1 supports a single user.

The login screen includes

- User Avatar
- Password
- Boot Animation
- Time
- Background Animation

Future versions can support multiple users.

---

# 113. Frontend Communication Rules

Allowed

```
React

↓

Backend API

↓

Kernel
```

Forbidden

```
React

↓

Kernel
```

Allowed

```
Window

↓

API

↓

Process Manager
```

Forbidden

```
Window

↓

Process Manager
```

---

# 114. Performance Guidelines

The frontend should

- Maintain 60 FPS animations
- Avoid unnecessary re-renders
- Lazy load heavy components
- Use memoization where appropriate
- Minimize WebSocket payload size
- Separate UI state from Kernel state

---

# 115. Completion Criteria

The Frontend Architecture is complete when

✓ Desktop loads successfully.

✓ Window Manager supports multiple windows.

✓ Taskbar updates automatically.

✓ Agents and tools communicate only through APIs.

✓ WebSockets synchronize UI in real time.

✓ Notifications display kernel events.

✓ All built-in agents run correctly as processes.

✓ Boot and Login screens work.

✓ Theme engine is implemented.

✓ State management is centralized.

---

# End of Part 5

The next and final part (Part 6) defines the AI Architecture, Voice Pipeline, Wake Word Detection, Gemini Integration, Local Command Parser, API Contracts, Database Schema, complete request lifecycle, development workflow, testing strategy, and deployment architecture for JARVIS OS.

# JARVIS OS
# ARCHITECTURE.md

# Part 6 — AI Architecture, Voice Pipeline, API Contracts & Deployment

**Voice is a secondary I/O layer.** Voice (speech in / speech out) is one of several ways to drive JARVIS OS; it is never the only way. Every capability — launching agents, dispatching tasks, process control, filesystem, scheduling, memory, shell, devices — must be fully executable through the core clock-driven JSON ABI, the shell, and the GUI **without voice**. Voice only translates speech into the same kernel commands (and kernel responses into speech) that the text/clock/UI paths already use. Nothing may exist only as a voice feature.

---

# 116. AI Architecture Overview

The Artificial Intelligence layer is an interface built on top of the operating system.

It is **not** responsible for CPU scheduling, memory allocation, file management, or process execution.

Instead, it understands user intent and translates it into structured kernel operations.

The AI never bypasses the Kernel.

Every request ultimately becomes one or more Kernel API calls.

---

# 117. AI Design Philosophy

The AI follows these principles.

- AI never owns operating system state.
- AI never modifies kernel data directly.
- AI only translates user intent.
- The Kernel remains the single source of truth.
- Simple commands execute locally.
- Only reasoning tasks use Gemini.

---

# 118. AI Layer Architecture

```
                    User

                       │

             Voice / Text Input

                       │

                       ▼

             Speech Recognition

                       │

                       ▼

              Wake Word Detector

                       │

                       ▼

              Local Command Parser

            ┌──────────┴──────────┐

            │                     │

      Local Command         AI Reasoning

            │                     │

            ▼                     ▼

        Kernel API          Gemini Service

            │                     │

            └──────────┬──────────┘

                       ▼

                 Kernel Execution

                       ▼

                UI + Voice Output
```

---

# 119. AI Module Structure

```
ai/

├── speech/

├── parser/

├── wakeword/

├── automation/

├── llm/

├── tts/

├── conversation/

├── prompts/

└── services/
```

Each folder owns one responsibility.

---

# 120. Speech Recognition

Speech Recognition converts spoken audio into text.

Implemented with

```
Faster Whisper
```

The output is always plain text.

Example

```
"Launch the Finance Agent"
```

↓

```
launch the finance agent
```

No kernel interaction occurs at this stage.

---

# 121. Wake Word Detection

JARVIS continuously listens for

```
Hey Jarvis
```

Implemented with

```
OpenWakeWord
```

Until the wake word is detected,

all recognized speech is ignored.

Flow

```
Microphone

↓

Wake Word

↓

Command Mode Enabled

↓

User Speaks

↓

Speech Recognition
```

---

# 122. Command Parser

The Local Parser converts text into structured commands.

Example

Input

```
Launch the Finance Agent
```

Output

```json
{
  "action":"launch_agent",
  "target":"finance"
}
```

The parser uses predefined rules.

No LLM required.

---

# 123. Local Command Engine

Commands handled without AI

- Launch Agent
- Close Agent
- Ask Agent (route task to an agent)
- Kill Process
- Show Tasks
- Show Memory
- Show CPU
- Create Folder
- Delete File
- Restart
- Shutdown
- Install Agent
- Uninstall Agent
- Search Files
- Device Manager
- Interrupt Simulation
- Scheduler Selection

Execution time should be under one second.

---

# 124. Gemini Integration

Gemini is only used when reasoning is required.

Examples

```
Why is Process 8 waiting?

↓

Kernel State

↓

Gemini

↓

Explanation
```

Another example

```
Create a project folder
with three files
and open the editor.

↓

Gemini

↓

Multiple Kernel Actions
```

---

# 125. AI Features Using Gemini

Only these features require an LLM.

- Explain Current State
- AI Automation
- System Health Summary
- Session Summary
- Natural Language Reasoning

Everything else executes locally.

---

# 126. Automation Engine

Automation converts one request into multiple kernel operations.

Example

```
Create a folder

↓

Create three text files

↓

Ask the Coding Agent to review them

↓

Open Folder
```

Kernel receives four separate commands.

The automation layer only creates the plan.

---

# 127. Text-to-Speech

Every important action should receive voice confirmation.

Examples

```
Finance Agent report ready.

Memory allocated.

Print completed.

System shutting down.

File deleted successfully.
```

Recommended

```
pyttsx3
```

No internet required.

---

# 128. Voice Notifications

The AI can proactively announce events.

Examples

```
Coding Agent finished its task.

Printing complete.

Agent installed.

Battery low.

Scheduler changed to Round Robin.
```

Voice notifications subscribe to kernel events.

---

# 129. AI Context Builder

When Gemini is called,

it never receives the entire operating system.

Instead,

the Context Builder extracts only relevant information.

Example

```
Running Processes

CPU Usage

Memory Usage

Scheduler

Current Logs

↓

Gemini
```

This reduces API cost.

---

# 130. AI Communication Rules

Allowed

```
AI

↓

Kernel API
```

Forbidden

```
AI

↓

Memory Manager
```

Allowed

```
AI

↓

Gemini

↓

Kernel API
```

Forbidden

```
Gemini

↓

Filesystem
```

The AI never directly controls operating system modules.

---

# 131. Backend API Architecture

Frontend communicates using REST APIs.

Examples

```
POST /agents/launch

POST /agents/close

POST /agents/task

POST /process/create

POST /process/kill

GET /processes

GET /agents

GET /memory

GET /filesystem

GET /devices

POST /voice

POST /automation
```

Every endpoint forwards requests to the Kernel.

---

## The Kernel ABI (FastAPI → C kernel)

The bridge never talks to kernel internals. All communication uses a compact **JSON ABI** exported by the C kernel library (`libjarvis_kernel`) and invoked from FastAPI through **ctypes**:

```
jvk_init(config_json)          → "ok" / error
jvk_command(action_json)       → result_json        (create_process, kill, alloc_mem, open_file, change_scheduler …)
jvk_tick()                                          advance clock, fire timer interrupt, run scheduler
jvk_snapshot()                → full system state JSON
jvk_logs(since)               → incremental logs JSON
jvk_shutdown()               → graceful shutdown
```

Rules:

- The kernel is the single source of truth; the bridge is a pure forwarding + validation layer (Pydantic on both sides).
- Every request still follows the lifecycle `Frontend → Backend → Kernel ABI → subsystem → result back through Kernel`.
- Live updates are produced by the bridge periodically calling `jvk_tick()` + `jvk_snapshot()`/`jvk_logs()` and publishing results into the WebSocket channels below. Immediate commands use `jvk_command()`.

---

# 132. WebSocket Channels

Real-time updates

```
/ws/processes

/ws/memory

/ws/cpu

/ws/logs

/ws/devices

/ws/notifications

/ws/voice
```

The frontend subscribes once during startup.

---

# 133. Database Architecture

SQLite stores persistent information.

Tables

```
Users

Agents

InstalledAgents

Logs

VoiceHistory

Settings

Files

Directories
```

Simulation state (CPU, RAM, PCB, queues) should remain in memory while the OS is running.

---

# 134. Configuration Files

```
config/

scheduler.json

memory.json

filesystem.json

voice.json

theme.json

ai.json
```

No values should be hardcoded.

---

# 135. Complete Request Lifecycle

Example

```
User

↓

Hey Jarvis

↓

Speech Recognition

↓

Command Parser

↓

Kernel API

↓

Kernel

↓

Process Manager

↓

Memory Manager

↓

Scheduler

↓

CPU

↓

Agent Starts (process run)

↓

Frontend Updates

↓

Voice Confirmation
```

Every command follows this lifecycle.

---

# 136. Development Workflow

Recommended implementation order

### Step 1

Project Setup

Kernel toolchain first — `gcc` (MinGW-w64 on Windows), CMake/Make, NASM, Google Test wired to build `libjarvis_kernel`

Electron

React

FastAPI

---

### Step 2

Kernel

System Calls

Event Bus

Dispatcher

---

### Step 3

CPU

Scheduler

Process Manager

PCB

---

### Step 4

Memory

Paging

Virtual Memory

Interrupts

---

### Step 5

Filesystem

Devices

Drivers

Shell

---

### Step 6

Desktop

Taskbar

Window Manager

Agent Hub & Consoles

Agent Studio

---

### Step 7

Voice Recognition

Wake Word

Parser

TTS

---

### Step 8

Gemini

Automation

Reasoning

Session Summary

---

### Step 9

Testing

Optimization

Documentation

Deployment

---

# 137. Testing Strategy

Every module should be tested independently.

Unit Tests

- Scheduler (Google Test, C)
- Process Manager (Google Test, C)
- Memory Manager (Google Test, C)
- Filesystem (Google Test, C)
- IPC (Google Test, C)
- Drivers (Google Test, C)
- Kernel ABI / bridge (pytest via ctypes)
- React components (Vitest)

Integration Tests

- Kernel ABI → C kernel (ctypes round-trip)
- Kernel + Memory
- Kernel + Scheduler
- Kernel + Filesystem
- AI + Kernel

End-to-End Tests

- Boot OS
- Launch and run Agents
- Save Files
- Voice Commands
- Shutdown

---

# 138. Deployment Architecture

```
Electron

↓

React Frontend

↓

FastAPI Backend

↓

Kernel Modules (C17 native library)

↓

SQLite

↓

Gemini API
```

Everything runs locally except Gemini API requests.

No local LLM is required.

---

# 139. Security Principles

Version 1 security goals

- Validate all API inputs.
- Restrict direct filesystem access.
- Sanitize voice commands.
- Protect configuration files.
- Never expose agent API keys.

- Agents are rule-based; no API keys required for agent decisions.
---

# 140. Performance Goals

Boot Time

< 5 seconds

Voice Command

< 1 second (local)

Gemini Response

2–5 seconds

UI

60 FPS

Memory Usage

< 500 MB (excluding Electron overhead)

---

# 141. Future Architecture

The modular design allows future expansion.

Possible additions

- Multi-user support
- Networking stack
- Package Manager
- Plugin SDK
- Local LLM support
- Distributed OS simulation
- Mobile Companion
- Cloud Sync
- IoT Integration

No architectural redesign should be required.

---

# 142. Final Architecture Summary

JARVIS OS is organized into six major layers.

```
User

↓

Desktop Interface

↓

Backend APIs

↓

Kernel

↓

Core OS Modules

↓

Virtual Hardware
```

The AI layer exists alongside the user interface and communicates only through Kernel APIs.

Every subsystem has a single responsibility.

Every operation passes through the Kernel.

The architecture is modular, scalable, testable, and closely resembles the design philosophy of modern operating systems while remaining fully software-based.

---

# End of ARCHITECTURE.md

Version 1.0