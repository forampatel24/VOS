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
  | "system-info"
  | "agent-hub"
  | "agent-studio";

export type WindowId = FixedWindowId | `agent-${number}`;
export type SnapEdge = "left" | "right" | "maximized";

export interface WindowRect {
  x: number;
  y: number;
  w: number;
  h: number;
}

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
  snap: SnapEdge | null;
  restore: WindowRect | null;
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
  toggleMaximize: (id: WindowId) => void;
  snapWindow: (id: WindowId, edge: "left" | "right" | "top") => void;
  restoreWindow: (id: WindowId) => void;
}

const TASKBAR_HEIGHT = 44;

function workArea() {
  return {
    width: Math.max(640, window.innerWidth),
    height: Math.max(320, window.innerHeight - TASKBAR_HEIGHT),
  };
}

const defaults: Record<string, WindowState> = {
  "system-monitor": { id: "system-monitor", title: "System Monitor", open: true, x: 40, y: 40, w: 560, h: 360, z: 1, minimized: false, snap: null, restore: null },
  processes: { id: "processes", title: "Process Manager", open: false, x: 80, y: 80, w: 640, h: 420, z: 2, minimized: false, snap: null, restore: null },
  memory: { id: "memory", title: "Memory Viewer", open: false, x: 120, y: 120, w: 640, h: 460, z: 3, minimized: false, snap: null, restore: null },
  cpu: { id: "cpu", title: "CPU & Scheduler", open: false, x: 160, y: 160, w: 560, h: 400, z: 4, minimized: false, snap: null, restore: null },
  eventlog: { id: "eventlog", title: "Event Log", open: true, x: 620, y: 40, w: 520, h: 360, z: 5, minimized: false, snap: null, restore: null },
  files: { id: "files", title: "File Explorer", open: false, x: 100, y: 100, w: 600, h: 400, z: 6, minimized: false, snap: null, restore: null },
  ipc: { id: "ipc", title: "IPC & Sync Lab", open: false, x: 140, y: 140, w: 600, h: 400, z: 7, minimized: false, snap: null, restore: null },
  devices: { id: "devices", title: "Device Manager", open: false, x: 180, y: 180, w: 600, h: 400, z: 8, minimized: false, snap: null, restore: null },
  interrupts: { id: "interrupts", title: "Interrupt Center", open: false, x: 200, y: 200, w: 600, h: 400, z: 9, minimized: false, snap: null, restore: null },
  terminal: { id: "terminal", title: "Terminal", open: false, x: 220, y: 220, w: 600, h: 380, z: 10, minimized: false, snap: null, restore: null },
  settings: { id: "settings", title: "Settings", open: false, x: 240, y: 240, w: 520, h: 400, z: 11, minimized: false, snap: null, restore: null },
  "system-info": { id: "system-info", title: "System Information", open: false, x: 260, y: 100, w: 700, h: 520, z: 12, minimized: false, snap: null, restore: null },
  "agent-hub": { id: "agent-hub", title: "Agent Hub", open: false, x: 60, y: 60, w: 820, h: 560, z: 13, minimized: false, snap: null, restore: null },
  "agent-studio": { id: "agent-studio", title: "Agent Studio", open: false, x: 120, y: 80, w: 660, h: 600, z: 14, minimized: false, snap: null, restore: null },
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
    z: 15,
    minimized: false,
    snap: null,
    restore: null,
  };
}

function rectOf(w: WindowState): WindowRect {
  return { x: w.x, y: w.y, w: w.w, h: w.h };
}

// Windows live below the Dock (z-30) and taskbar (z-40). If focus grows the
// layer without bound, a long session would eventually cover the shell, so
// compact the stack before that happens.
const MAX_WINDOW_Z = 26;

function raiseWindow(windows: Record<string, WindowState>, nextZ: number) {
  const z = nextZ + 1;
  if (z < MAX_WINDOW_Z) return { windows, z, nextZ: z };
  const ordered = Object.values(windows).sort((a, b) => a.z - b.z);
  const compact: Record<string, WindowState> = {};
  ordered.forEach((win, index) => {
    compact[win.id] = { ...win, z: index + 1 };
  });
  const compactZ = ordered.length + 1;
  return { windows: compact, z: compactZ, nextZ: compactZ };
}

export const useWindowStore = create<WindowStore>((set) => ({
  windows: { ...defaults },
  nextZ: 20,
  open: (id, title) =>
    set((s) => {
      const layer = raiseWindow(s.windows, s.nextZ);
      const base = s.windows[id] ?? makeWindow(id, title, Object.keys(s.windows).length);
      const next = title ? { ...base, title } : base;
      return {
        windows: {
          ...layer.windows,
          [id]: { ...next, open: true, minimized: false, z: layer.z },
        },
        nextZ: layer.nextZ,
      };
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
      const layer = raiseWindow(s.windows, s.nextZ);
      return {
        windows: { ...layer.windows, [id]: { ...w, z: layer.z } },
        nextZ: layer.nextZ,
      };
    }),
  move: (id, x, y) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      return { windows: { ...s.windows, [id]: { ...w, x, y, snap: null } } };
    }),
  resize: (id, w, h) =>
    set((s) => {
      const win = s.windows[id];
      if (!win) return {};
      return {
        windows: {
          ...s.windows,
          [id]: { ...win, w: Math.max(320, w), h: Math.max(200, h), snap: null },
        },
      };
    }),
  toggleMinimize: (id) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      return { windows: { ...s.windows, [id]: { ...w, minimized: !w.minimized } } };
    }),
  toggleMaximize: (id) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      if (w.snap === "maximized" && w.restore) {
        return {
          windows: {
            ...s.windows,
            [id]: { ...w, ...w.restore, snap: null, restore: null },
          },
        };
      }
      const area = workArea();
      return {
        windows: {
          ...s.windows,
          [id]: {
            ...w,
            x: 0,
            y: 0,
            w: area.width,
            h: area.height,
            snap: "maximized",
            restore: w.restore ?? rectOf(w),
          },
        },
      };
    }),
  snapWindow: (id, edge) =>
    set((s) => {
      const w = s.windows[id];
      if (!w) return {};
      if (edge === "top") {
        const area = workArea();
        return {
          windows: {
            ...s.windows,
            [id]: {
              ...w,
              x: 0,
              y: 0,
              w: area.width,
              h: area.height,
              snap: "maximized",
              restore: w.restore ?? rectOf(w),
            },
          },
        };
      }
      const area = workArea();
      const half = Math.floor(area.width / 2);
      return {
        windows: {
          ...s.windows,
          [id]: {
            ...w,
            x: edge === "left" ? 0 : half,
            y: 0,
            w: half,
            h: area.height,
            snap: edge,
            restore: w.restore ?? rectOf(w),
          },
        },
      };
    }),
  restoreWindow: (id) =>
    set((s) => {
      const w = s.windows[id];
      if (!w?.restore) return {};
      return {
        windows: {
          ...s.windows,
          [id]: { ...w, ...w.restore, snap: null, restore: null },
        },
      };
    }),
}));
