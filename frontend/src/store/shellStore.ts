import { create } from "zustand";
import { DEFAULT_PINNED } from "@/lib/apps";
import type { FixedWindowId } from "@/store/windowStore";

export type NoticeLevel = "info" | "success" | "warning" | "error";
export type PowerDialog = "shutdown" | "restart" | "halted" | null;

export interface Toast {
  id: string;
  level: NoticeLevel;
  topic: string;
  message: string;
  createdAt: number;
}

const PIN_KEY = "jarvis.dock.pinned";
let toastSeq = 0;

function loadPinned(): FixedWindowId[] {
  try {
    const raw = localStorage.getItem(PIN_KEY);
    if (!raw) return [...DEFAULT_PINNED];
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed) && parsed.every((id) => typeof id === "string")) {
      return parsed as FixedWindowId[];
    }
  } catch {
    // localStorage may be unavailable in a hardened renderer.
  }
  return [...DEFAULT_PINNED];
}

function savePinned(pinned: FixedWindowId[]) {
  try {
    localStorage.setItem(PIN_KEY, JSON.stringify(pinned));
  } catch {
    // Docking still works for this session if persistence is unavailable.
  }
}

interface ShellState {
  launcherOpen: boolean;
  searchOpen: boolean;
  notificationsOpen: boolean;
  trayOpen: boolean;
  powerDialog: PowerDialog;
  fileTarget: string | null;
  pinned: FixedWindowId[];
  lastReadIndex: number;
  toasts: Toast[];
  toggleLauncher: () => void;
  closeLauncher: () => void;
  openSearch: () => void;
  closeSearch: () => void;
  toggleSearch: () => void;
  toggleNotifications: () => void;
  closeNotifications: () => void;
  toggleTray: () => void;
  closeTray: () => void;
  closeMenus: () => void;
  requestPower: (dialog: Exclude<PowerDialog, null>) => void;
  closePower: () => void;
  setFileTarget: (path: string | null) => void;
  togglePin: (id: FixedWindowId) => void;
  markNotificationsRead: (index: number) => void;
  pushToast: (toast: Omit<Toast, "id" | "createdAt">) => void;
  dismissToast: (id: string) => void;
}

export const useShellStore = create<ShellState>((set) => ({
  launcherOpen: false,
  searchOpen: false,
  notificationsOpen: false,
  trayOpen: false,
  powerDialog: null,
  fileTarget: null,
  pinned: loadPinned(),
  lastReadIndex: 0,
  toasts: [],

  toggleLauncher: () =>
    set((s) => ({
      launcherOpen: !s.launcherOpen,
      searchOpen: false,
      notificationsOpen: false,
      trayOpen: false,
    })),
  closeLauncher: () => set({ launcherOpen: false }),
  openSearch: () =>
    set({
      searchOpen: true,
      launcherOpen: false,
      notificationsOpen: false,
      trayOpen: false,
    }),
  closeSearch: () => set({ searchOpen: false }),
  toggleSearch: () =>
    set((s) => ({
      searchOpen: !s.searchOpen,
      launcherOpen: false,
      notificationsOpen: false,
      trayOpen: false,
    })),
  toggleNotifications: () =>
    set((s) => ({
      notificationsOpen: !s.notificationsOpen,
      launcherOpen: false,
      searchOpen: false,
      trayOpen: false,
    })),
  closeNotifications: () => set({ notificationsOpen: false }),
  toggleTray: () =>
    set((s) => ({
      trayOpen: !s.trayOpen,
      launcherOpen: false,
      searchOpen: false,
      notificationsOpen: false,
    })),
  closeTray: () => set({ trayOpen: false }),
  closeMenus: () =>
    set({
      launcherOpen: false,
      searchOpen: false,
      notificationsOpen: false,
      trayOpen: false,
    }),
  requestPower: (dialog) => set({ powerDialog: dialog, trayOpen: false }),
  closePower: () => set({ powerDialog: null }),
  setFileTarget: (path) => set({ fileTarget: path }),
  togglePin: (id) =>
    set((s) => {
      const pinned = s.pinned.includes(id)
        ? s.pinned.filter((entry) => entry !== id)
        : [...s.pinned, id];
      savePinned(pinned);
      return { pinned };
    }),
  markNotificationsRead: (index) =>
    set((s) => ({ lastReadIndex: Math.max(s.lastReadIndex, index) })),
  pushToast: (toast) =>
    set((s) => {
      const next: Toast = {
        ...toast,
        id: `toast-${Date.now()}-${toastSeq++}`,
        createdAt: Date.now(),
      };
      return { toasts: [...s.toasts, next].slice(-4) };
    }),
  dismissToast: (id) =>
    set((s) => ({ toasts: s.toasts.filter((toast) => toast.id !== id) })),
}));
