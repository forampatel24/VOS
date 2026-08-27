import { create } from "zustand";
import type { KernelSnapshot, LogEntry } from "@/api/types";
import { createKernelSocket, fetchSnapshot, fetchLogs, sendCommand, tick as apiTick } from "@/api/client";

interface KernelState {
  snapshot: KernelSnapshot | null;
  logs: LogEntry[];
  connected: boolean;
  error: string | null;
  logCursor: number;
  // actions
  init: () => void;
  destroy: () => void;
  refreshSnapshot: () => Promise<void>;
  refreshLogs: () => Promise<void>;
  command: (payload: Record<string, unknown>) => Promise<unknown>;
  doTick: () => Promise<void>;
}

let disconnect: (() => void) | null = null;

export const useKernelStore = create<KernelState>((set, get) => ({
  snapshot: null,
  logs: [],
  connected: false,
  error: null,
  logCursor: 0,

  init: () => {
    // initial REST fetch for immediate paint, then WS takeover
    fetchSnapshot()
      .then((snap) => set({ snapshot: snap }))
      .catch((e) => set({ error: String(e) }));

    fetchLogs(0)
      .then((entries) => {
        if (entries.length > 0) {
          set({ logs: entries, logCursor: Math.max(...entries.map((e) => e.index)) + 1 });
        }
      })
      .catch(() => {});

    if (disconnect) disconnect();
    disconnect = createKernelSocket(
      (snap, newLogs) => {
        set((s) => {
          const merged = [...s.logs];
          for (const e of newLogs) {
            if (!merged.find((m) => m.index === e.index)) merged.push(e);
          }
          // keep last 400 entries for performance
          const trimmed = merged.length > 400 ? merged.slice(-400) : merged;
          const cursor = trimmed.length > 0 ? Math.max(...trimmed.map((x) => x.index)) + 1 : s.logCursor;
          return { snapshot: snap, logs: trimmed, connected: true, error: null, logCursor: cursor };
        });
      },
      () => set({ connected: false })
    );
    set({ connected: true });
  },

  destroy: () => {
    if (disconnect) {
      disconnect();
      disconnect = null;
    }
    set({ connected: false });
  },

  refreshSnapshot: async () => {
    try {
      const snap = await fetchSnapshot();
      set({ snapshot: snap, error: null });
    } catch (e) {
      set({ error: String(e) });
    }
  },

  refreshLogs: async () => {
    const { logCursor } = get();
    try {
      const entries = await fetchLogs(logCursor);
      if (entries.length > 0) {
        set((s) => {
          const merged = [...s.logs, ...entries.filter((e) => !s.logs.find((m) => m.index === e.index))];
          const trimmed = merged.length > 400 ? merged.slice(-400) : merged;
          return { logs: trimmed, logCursor: Math.max(...trimmed.map((x) => x.index)) + 1 };
        });
      }
    } catch {
      // ignore
    }
  },

  command: async (payload) => {
    const result = await sendCommand(payload);
    // snapshot/logs will arrive via WS; also do a one-off refresh for snappiness
    setTimeout(() => {
      get().refreshSnapshot();
      get().refreshLogs();
    }, 120);
    return result;
  },

  doTick: async () => {
    await apiTick();
    get().refreshSnapshot();
    get().refreshLogs();
  },
}));
