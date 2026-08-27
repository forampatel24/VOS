import axios from "axios";
import type { KernelSnapshot, LogEntry } from "./types";

const API_BASE = ""; // proxied via Vite to 127.0.0.1:8000

const http = axios.create({
  baseURL: API_BASE,
  timeout: 5000,
  headers: { "Content-Type": "application/json" },
});

export async function fetchSnapshot(): Promise<KernelSnapshot> {
  const res = await http.get("/api/snapshot");
  return res.data as KernelSnapshot;
}

export async function fetchHealth(): Promise<{ ok: boolean; kernel: KernelSnapshot }> {
  const res = await http.get("/health");
  return res.data;
}

export async function fetchLogs(since: number): Promise<LogEntry[]> {
  const res = await http.get("/api/logs", { params: { since } });
  return (res.data.logs as LogEntry[]) ?? [];
}

export async function sendCommand(payload: Record<string, unknown>) {
  const res = await http.post("/api/command", payload);
  return res.data;
}

export async function tick() {
  const res = await http.get("/api/tick");
  return res.data;
}

// WebSocket helper — single shared connection, auto-reconnect
export function createKernelSocket(
  onUpdate: (snap: KernelSnapshot, logs: LogEntry[]) => void,
  onError?: (e: Event) => void
): () => void {
  const proto = window.location.protocol === "https:" ? "wss:" : "ws:";
  const url = `${proto}//${window.location.host}/ws`;
  // Fallback when Vite proxies: use direct backend
  const wsUrl = window.location.port === "5173" ? "ws://127.0.0.1:8000/ws" : url;

  let ws: WebSocket | null = null;
  let closed = false;
  let retryMs = 1000;

  function connect() {
    if (closed) return;
    ws = new WebSocket(wsUrl);

    ws.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        if (msg.type === "update" && msg.snapshot) {
          retryMs = 1000;
          onUpdate(msg.snapshot as KernelSnapshot, (msg.logs as LogEntry[]) ?? []);
        }
      } catch {
        // ignore malformed
      }
    };

    ws.onerror = (e) => onError?.(e);
    ws.onclose = () => {
      if (closed) return;
      setTimeout(connect, retryMs);
      retryMs = Math.min(retryMs * 1.5, 8000);
    };
  }

  connect();

  return () => {
    closed = true;
    try {
      ws?.close();
    } catch {
      // ignore
    }
  };
}

export function wsRequestTick(ws: WebSocket | null) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ action: "tick" }));
  }
}
