import { create } from "zustand";

export type WindowId =
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
  | "settings";

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
  windows: Record<WindowId, WindowState>;
  nextZ: number;
  open: (id: WindowId) => void;
  close: (id: WindowId) => void;
  focus: (id: WindowId) => void;
  move: (id: WindowId, x: number, y: number) => void;
  resize: (id: WindowId, w: number, h: number) => void;
  toggleMinimize: (id: WindowId) => void;
}

const defaults: Record<WindowId, WindowState> = {
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
};

export const useWindowStore = create<WindowStore>((set) => ({
  windows: { ...defaults },
  nextZ: 20,
  open: (id) =>
    set((s) => {
      const z = s.nextZ + 1;
      return { windows: { ...s.windows, [id]: { ...s.windows[id], open: true, minimized: false, z } }, nextZ: z };
    }),
  close: (id) =>
    set((s) => ({
      windows: { ...s.windows, [id]: { ...s.windows[id], open: false } },
    })),
  focus: (id) =>
    set((s) => {
      const z = s.nextZ + 1;
      return { windows: { ...s.windows, [id]: { ...s.windows[id], z } }, nextZ: z };
    }),
  move: (id, x, y) =>
    set((s) => ({
      windows: { ...s.windows, [id]: { ...s.windows[id], x, y } },
    })),
  resize: (id, w, h) =>
    set((s) => ({
      windows: { ...s.windows, [id]: { ...s.windows[id], w: Math.max(320, w), h: Math.max(200, h) } },
    })),
  toggleMinimize: (id) =>
    set((s) => ({
      windows: { ...s.windows, [id]: { ...s.windows[id], minimized: !s.windows[id].minimized } },
    })),
}));
