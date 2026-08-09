# AGENT.md

# JARVIS OS — AI Development Guidelines

Version: 1.0

---

# Project Goal

You are building **JARVIS OS**, a fully software-based educational operating system simulator.

This is **NOT** a Linux distribution.

This is **NOT** a Windows clone.

This is **NOT** an Electron desktop application with random windows.

The objective is to build an operating system simulator that teaches and demonstrates real Operating System concepts while providing a modern futuristic AI-powered desktop experience.

The project should feel like a complete desktop operating system from the moment it boots.

The user should forget that Windows exists underneath.

Everything inside JARVIS OS should behave like a real operating system.

---

# Primary Design Principles

Always prioritize

1. Clean Architecture
2. Modularity
3. Real OS Concepts
4. Scalability
5. Maintainability
6. Visual Clarity
7. Educational Value

Never sacrifice architecture for shortcuts.

---

# What This Project Is

This project is

• A virtual computer

• A simulated operating system

• A desktop environment

• A kernel simulator

• A process simulator

• A memory simulator

• A filesystem simulator

• A device simulator

• An AI assistant integrated into the operating system

---

# What This Project Is NOT

Never implement

❌ Fake animations without actual logic

❌ Hardcoded process lists

❌ Hardcoded memory values

❌ Fake scheduling

❌ Fake context switching

❌ Fake page faults

❌ Fake filesystem

❌ Direct frontend manipulation of kernel state

❌ Direct access to backend modules from React

❌ Monolithic classes

❌ God Objects

Everything should be generated dynamically.

---

# Technology Stack

Kernel

C (C17)

x86-64 Assembly (NASM)

cJSON (vendored)

Google Test

---

Frontend

Electron

React

TypeScript

TailwindCSS

Framer Motion

shadcn/ui

Zustand

Axios

WebSocket

---

Backend

Python 3.12+

FastAPI

Pydantic

ctypes (kernel bridge)

SQLAlchemy

SQLite

---

Voice

Faster Whisper

OpenWakeWord

pyttsx3

---

AI

Google Gemini API

Only for reasoning tasks.

Never use Gemini for simple commands.

---

# Overall Architecture

```
Frontend (TypeScript / React / Electron)

↓

REST API / WebSocket

↓

FastAPI Bridge (ctypes)

↓

Kernel (C)

↓

Subsystems

↓

Virtual Hardware
```

No shortcuts.

---

# Kernel Rules

The Kernel is the only authority.

Every request must pass through the Kernel.

Subsystems never communicate directly.

Correct

```
Scheduler

↓

Kernel

↓

Memory Manager
```

Wrong

```
Scheduler

↓

Memory Manager
```

---

# API Rules

React never imports backend code.

Backend never imports frontend code.

Everything goes through APIs.

---

# Folder Ownership

Frontend owns

UI

Animations

Windows

Desktop

Taskbar

Rendering

---

Kernel (C) owns

CPU

Memory / Paging

Scheduler

Filesystem

Drivers

Interrupts

IPC

---

Backend (Python) owns

FastAPI Bridge

Voice

AI (Gemini)

---

SQLite owns

Persistent Data

Logs

Settings

Installed Applications

Filesystem Metadata

---

Memory owns

Runtime Simulation

Never store runtime objects in SQLite.

---

# Single Responsibility Principle

Every module should have exactly one responsibility.

Bad

MemoryManager

↓

Scheduling

↓

Logging

↓

Filesystem

Good

MemoryManager

↓

Only Memory

---

# State Ownership

Kernel owns

Runtime State

React owns

UI State

SQLite owns

Persistent State

Never mix them.

---

# Development Order

Always implement in this order.

1

Project Setup

2

Kernel

3

CPU

4

Process Manager

5

Scheduler

6

Memory Manager

7

Paging

8

Interrupts

9

Filesystem

10

Drivers

11

I/O

12

IPC

13

Desktop

14

Applications

15

Voice

16

Gemini

17

Testing

Never skip stages.

---

# Coding Standards

Every class

Maximum

300 lines

If larger

Split it.

Every function

Maximum

50 lines

If larger

Refactor.

Every file

One responsibility.

---

# Naming Conventions

Classes

PascalCase

```
ProcessManager
```

Variables

camelCase

```
currentProcess
```

Constants

UPPER_CASE

```
DEFAULT_QUANTUM
```

Private

```
_internalState
```

---

# Dependency Rules

Allowed

Kernel

↓

Subsystem

Allowed

Subsystem

↓

Utility

Forbidden

Subsystem

↓

Subsystem

Forbidden

React

↓

Kernel

Forbidden

React

↓

SQLite

---

# Error Handling

Never crash.

Always

Validate

↓

Catch

↓

Log

↓

Recover

↓

Notify

Never ignore exceptions.

---

# Logging

Every major event must be logged.

Examples

Process Created

Memory Allocated

Page Fault

Interrupt

Shutdown

Voice Command

Gemini Call

Application Installed

---

# Event Bus

Never directly notify components.

Publish events.

Subscribers update themselves.

Always prefer events over callbacks.

---

# Scheduler Rules

Scheduling algorithms must be interchangeable.

Implement Strategy Pattern.

Never hardcode algorithm selection.

---

# Memory Rules

Virtual Memory only.

Every allocation updates

Page Table

Frame Table

Statistics

Logs

Events

Never skip updates.

---

# Filesystem Rules

Never access Windows files directly.

Everything uses

Virtual Disk

Virtual Filesystem

Kernel APIs

---

# Device Rules

Every device

Has a Driver.

Every Driver

Implements common interface.

---

# Process Rules

Every Process

Owns

PCB

Memory

Registers

State

Statistics

Open Files

Never store process data elsewhere.

---

# Context Switching

Must

Save Registers

↓

Save PCB

↓

Select Process

↓

Restore PCB

↓

Resume

Never fake context switches.

---

# Interrupt Rules

Interrupts

↓

Queue

↓

Priority

↓

ISR

↓

Resume

Never bypass ISR.

---

# IPC Rules

Only Kernel coordinates IPC.

Never allow Process A

↓

Process B

Direct communication.

---

# Frontend Rules

Desktop must feel like

A Real Computer.

Everything opens in windows.

Every window movable.

Every window resizable.

Everything should support

Dark futuristic UI.

---

# Animation Rules

Animations should explain.

Not decorate.

Good

Memory allocation animation.

Bad

Random glowing effects.

---

# Voice Rules

Simple commands

↓

Parser

↓

Kernel

Complex reasoning

↓

Gemini

↓

Kernel

Never call Gemini unnecessarily.

---

# Gemini Rules

Allowed

Explain Current State

Automation

Session Summary

Health Summary

Natural Language Reasoning

Forbidden

Open Calculator

Kill Process

Restart

Open Explorer

Delete File

Create Folder

Simple commands stay local.

---

# API Design

REST

For Commands

WebSocket

For Live Updates

Never poll continuously.

---

# Database Rules

SQLite stores

Settings

Logs

Filesystem

Installed Apps

Users

Never store

Processes

RAM

Registers

Queues

Current Scheduler State

Those belong to runtime.

---

# UI Philosophy

Modern

Minimal

Futuristic

AI-first

No Retro UI.

No Windows clone.

Original Identity.

---

# Performance Targets

Boot

<5 sec

UI

60 FPS

Voice

<1 sec

Memory

Efficient

Avoid unnecessary renders.

---

# Testing

Every module

Unit Test

Every subsystem

Integration Test

Entire OS

End-to-End Test

Testing is mandatory.

---

# Code Quality

Always

Type Hint

Document

Comment complex logic

Use dataclasses/Pydantic models where appropriate

Avoid duplicated code

Prefer composition over inheritance

Use interfaces wherever practical

---

# Git Workflow

Feature Branches

```
feature/kernel

feature/memory

feature/filesystem

feature/voice

feature/frontend
```

Never develop everything on one branch.

---

# Documentation

Every module should include

Purpose

Responsibilities

Inputs

Outputs

Dependencies

Public APIs

Examples

---

# Final Goal

When the user launches JARVIS OS, they should experience:

- A boot sequence that feels like starting a new computer.
- A complete desktop environment with multiple applications.
- A functioning virtual kernel handling processes, scheduling, memory, files, devices, interrupts, IPC, and synchronization.
- Real-time visualizations of internal operating system concepts.
- Voice interaction through "Hey JARVIS" for local commands.
- AI-powered reasoning (via Gemini) only where natural-language understanding or summarization adds value.
- A modular architecture where every subsystem communicates only through the Kernel.
- An educational yet professional operating system simulator that demonstrates all three core OS stages while feeling like a polished, futuristic product rather than a typical college project.

If any implementation decision conflicts with these principles, prioritize architectural correctness, modularity, and accurate operating system behavior over convenience.
