import type { FixedWindowId } from "@/store/windowStore";

export interface AppEntry {
  id: FixedWindowId;
  label: string;
  short: string;
  icon: string;
  dot: string;
  description: string;
  keywords: string[];
}

export const APP_CATALOG: AppEntry[] = [
  {
    id: "system-monitor",
    label: "System Monitor",
    short: "Monitor",
    icon: "SM",
    dot: "bg-cyan-400",
    description: "Live kernel dashboard",
    keywords: ["dashboard", "overview", "health", "home"],
  },
  {
    id: "processes",
    label: "Process Manager",
    short: "Processes",
    icon: "PS",
    dot: "bg-emerald-400",
    description: "PCBs, queues and lifecycle",
    keywords: ["process", "pid", "task", "kill", "suspend"],
  },
  {
    id: "memory",
    label: "Memory Viewer",
    short: "Memory",
    icon: "MM",
    dot: "bg-violet-400",
    description: "Frames, pages and swapping",
    keywords: ["ram", "page", "frame", "swap", "allocator"],
  },
  {
    id: "cpu",
    label: "CPU & Scheduler",
    short: "CPU",
    icon: "CPU",
    dot: "bg-amber-400",
    description: "Clock, quantum and scheduling",
    keywords: ["scheduler", "clock", "quantum", "round robin", "sjf"],
  },
  {
    id: "eventlog",
    label: "Event Log",
    short: "Logs",
    icon: "LG",
    dot: "bg-pink-400",
    description: "Kernel event stream",
    keywords: ["log", "events", "kernel", "stream"],
  },
  {
    id: "files",
    label: "File Explorer",
    short: "Files",
    icon: "FS",
    dot: "bg-slate-400",
    description: "Virtual disk and inodes",
    keywords: ["file", "folder", "disk", "inode", "explorer"],
  },
  {
    id: "devices",
    label: "Device Manager",
    short: "Devices",
    icon: "DV",
    dot: "bg-slate-400",
    description: "Drivers, IRQs and I/O buffers",
    keywords: ["device", "driver", "io", "spool", "printer"],
  },
  {
    id: "interrupts",
    label: "Interrupt Center",
    short: "IRQs",
    icon: "IRQ",
    dot: "bg-slate-400",
    description: "Interrupt queue and panic state",
    keywords: ["interrupt", "irq", "panic", "error"],
  },
  {
    id: "ipc",
    label: "IPC & Sync",
    short: "IPC",
    icon: "IPC",
    dot: "bg-fuchsia-400",
    description: "Pipes, queues and deadlocks",
    keywords: ["ipc", "pipe", "mutex", "semaphore", "deadlock"],
  },
  {
    id: "terminal",
    label: "Terminal",
    short: "Terminal",
    icon: ">_",
    dot: "bg-cyan-400",
    description: "Kernel shell",
    keywords: ["shell", "command", "cli", "console"],
  },
  {
    id: "agent-hub",
    label: "Agent Hub",
    short: "Agents",
    icon: "AI",
    dot: "bg-emerald-300",
    description: "Launch and inspect AI agents",
    keywords: ["agent", "ai", "launch", "task", "console"],
  },
  {
    id: "agent-studio",
    label: "Agent Studio",
    short: "Studio",
    icon: "AS",
    dot: "bg-violet-300",
    description: "Create durable agent profiles",
    keywords: ["agent", "studio", "create", "profile"],
  },
  {
    id: "settings",
    label: "Settings",
    short: "Settings",
    icon: "CFG",
    dot: "bg-white/40",
    description: "Kernel settings and appearance",
    keywords: ["settings", "config", "theme", "scheduler", "memory"],
  },
  {
    id: "system-info",
    label: "System Information",
    short: "System Info",
    icon: "SYS",
    dot: "bg-cyan-300",
    description: "Machine, kernel and capacity",
    keywords: ["about", "version", "machine", "hardware", "info", "bios"],
  },
];

export const APP_MAP = APP_CATALOG.reduce<Record<string, AppEntry>>(
  (acc, app) => {
    acc[app.id] = app;
    return acc;
  },
  {}
);

export const DEFAULT_PINNED: FixedWindowId[] = [
  "agent-hub",
  "files",
  "terminal",
  "system-monitor",
  "system-info",
  "settings",
];

export function appFor(id: string): AppEntry | undefined {
  return APP_MAP[id];
}
