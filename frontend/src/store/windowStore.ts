import { create } from "zustand";

export type FixedWindowId =
  | "system-monitor"
  | "processes"
  | "memory"
  | "cpu"
  | "eventlog"
  | "files"
  | "ipc"
  | "devices"
  | "interrupts"
  | "terminal"
  | "settings"
  | "agent-hub"
  | "agent-studio";

export type WindowId = FixedWindowId | `agent-${number}`;

export interface WindowState {
  id: WindowId;
  title: string;
  open: boolean;
  x: number;
  y: number;
  w: number;
  h: number;
  z: number;
  minimized: boolean;
}

interface WindowStore {
  windows: Record<string, WindowState>;
  nextZ: number;
  open: (id: WindowId, title?: string) => void;
  close: (id: WindowId) => void;
  focus: (id: WindowId) => void;
  move: (id: WindowId, x: number, y: number) => void;
  resize: (id: WindowId, w: number, h: number) => void;
  toggleMinimize: (id: WindowId) => void;
}

const defaults: Record<string, WindowState> = {
  "system-monitor": { id: "system-monitor", title: "System Monitor", open: true, x: 40, y: 40, w: 560, h: 360, z: 1, minimized: false },
  processes: { id: "processes", title: "Process Manager", open: false, x: 80, y: 80, w: 640, h: 420, z: 2, minimized: false },
  memory: { id: "memory", title: "Memory Viewer", open: false, x: 120, y: 120, w: 640, h: 460, z: 3, minimized: false },
  cpu: { id: "cpu", title: "CPU & Scheduler", open: false, x: 160, y: 160, w: 560, h: 400, z: 4, minimized: false },
  eventlog: { id: "eventlog", title: "Event Log", open: true, x: 620, y: 40, w: 520, h: 360, z: 5, minimized: false },
  files: { id: "files", title: "File Explorer", open: false, x: 100, y: 100, w: 600, h: 400, z: 6, minimized: false },
  ipc: { id: "ipc", title: "IPC & Sync Lab", open: false, x: 140, y: 140, w: 600, h: 400, z: 7, minimized: false },
  devices: { id: "devices", title: "Device Manager", open: false, x: 180, y: 180, w: 600, h: 400, z: 8, minimized: false },
  interrupts: { id: "interrupts", title: "Interrupt Center", open: false, x: 200, y: 200, w: 600, h: 400, z: 9, minimized: false },
  terminal: { id: "terminal", title: "Terminal", open: false, x: 220, y: 220, w: 600, h: 380, z: 10, minimized: false },
  settings: { id: "settings", title: "Settings", open: false, x: 240, y: 240, w: 520, h: 400, z: 11, minimized: false },
  "agent-hub": { id: "agent-hub", title: "Agent Hub", open: false, x: 60, y: 60, w: 820, h: 560, z: 12, minimized: false },
  "agent-studio": { id: "agent-studio", title: "Agent Studio", open: false, x: 120, y: 80, w: 660, h: 600, z: 13, minimized: false },
};

function makeWindow(id: WindowId, title?: string, count = 0): WindowState {
  return {
    id,
    title: title ?? "Agent Console",
    open: false,
    x: 140 + (count % 6) * 26,
    y: 90 + (count % 5) * 24,
    w: 780,
    h: 580,
    z: 14,
    minimized: false,
  };
}

export const useWindowStore = create<WindowStore>((set) => ({
  windows: { ...defaults },
  nextZ: 20,
  open: (id, title) =>
    set((s) => {
      const z = s.nextZ + 1;
      const base = s.windows[id] ?? makeWindow(id, title, Object.keys(s.windows).length);
      const next = title ? { ...base, title } : base;
      return { windows: { ...s.windows, [id]: { ...next, open: true, minimized: false, z } }, nextZ: z };
    }),
  close: (id) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      return { windows: { ...s.windows, [id]: { ...w, open: false } } };
    }),
  focus: (id) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      const z = s.nextZ + 1;
      return { windows: { ...s.windows, [id]: { ...w, z } }, nextZ: z };
    }),
  move: (id, x, y) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      return { windows: { ...s.windows, [id]: { ...w, x, y } } };
    }),
  resize: (id, w, h) =>
    set((s) => {
      const win = s.windows[id];
      if (!win) return {};
      return {
        windows: {
          ...s.windows,
          [id]: { ...win, w: Math.max(320, w), h: Math.max(200, h) },
        },
      };
    }),
  toggleMinimize: (id) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      return { windows: { ...s.windows, [id]: { ...w, minimized: !w.minimized } } };
    }),
}));
